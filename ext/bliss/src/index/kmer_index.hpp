/**
 * @file    kmer_index.hpp
 * @ingroup index
 * @author  Tony Pan <tpan7@gatech.edu>
 * @brief	High level kmer indexing API
 * @details	3 primary Kmer Index classes are currently provided:
 * 			Kmer Count Index,
 * 			Kmer Position Index, and
 * 			Kmer Position + Quality score index.
 *
 *
 *		Currently, KmerIndex classes only supports FASTQ files, but the intent is to
 *		  allow replaceable module for file reading.
 *
 *		Data distribution is via replaceable Hash functions.  currently,
 *		  PrefixHasher is used to distribute to MPI processes based on the first log_2(P) bits of the kmer
 *		  InfixHasher is used to distribute to threads within a process, based on the next log_2(p) bits of the kmer
 *		  SuffixHasher is used for threadlocal unordered_map hashing
 *
 *		All are deterministic to allow simple lookup processes.
 *
 *
 * Copyright (c) 2014 Georgia Institute of Technology.  All Rights Reserved.
 *
 * TODO add License
 */
#ifndef KMERINDEX2_HPP_
#define KMERINDEX2_HPP_

#include "bliss-config.hpp"

#if defined(USE_MPI)
#include "mpi.h"
#endif

//#if defined(USE_OPENMP)
//#include "omp.h"
//#endif

#include <unistd.h>     // sysconf
#include <sys/stat.h>   // block size.
#include <tuple>        // tuple and utility functions
#include <utility>      // pair and utility functions.
#include <type_traits>
#include <cctype>       // tolower.

#include "io/fastq_loader.hpp"
#include "io/fasta_loader.hpp"
//#include "io/fasta_iterator.hpp"

#include "utils/logging.h"
#include "common/alphabets.hpp"
#include "common/kmer.hpp"
#include "common/base_types.hpp"
#include "common/sequence.hpp"
#include "utils/kmer_utils.hpp"
#include "index/kmer_hash.hpp"
#include "common/kmer_transform.hpp"

#include "io/mxx_support.hpp"
#include "containers/distributed_unordered_map.hpp"
#include "containers/distributed_sorted_map.hpp"
// way too slow.  also not updated.  #include "containers/distributed_map.hpp"

#include "io/sequence_iterator.hpp"
#include "io/sequence_id_iterator.hpp"
#include "iterators/transform_iterator.hpp"
#include "common/kmer_iterators.hpp"
#include "iterators/zip_iterator.hpp"
#include "iterators/unzip_iterator.hpp"
#include "iterators/constant_iterator.hpp"
#include "index/quality_score_iterator.hpp"

#include "utils/timer.hpp"
#include "utils/file_utils.hpp"

#include <fstream> // debug only
#include <iostream>  // debug only
#include <sstream>  // debug only

namespace bliss
{
namespace index
{
namespace kmer
{

/**
 * @tparam MapType  	container type
 * @tparam KmerParser		functor to generate kmer (tuple) from input.  specified here so we specialize for different index.  note KmerParser needs to be supplied with a data type.
 */
template <typename MapType, typename KmerParser>
class Index {
protected:

	MapType map;

	const mxx::comm& comm;

public:
	using KmerType = typename MapType::key_type;
	using ValueType   = typename MapType::mapped_type;
	using TupleType =      std::pair<KmerType, ValueType>;
	using Alphabet = typename KmerType::KmerAlphabet;

	Index(const mxx::comm& _comm) : map(_comm), comm(_comm) {
	}

	virtual ~Index() {};

	MapType & get_map() {
		return map;
	}

	/**
	 * @brief  generate kmers or kmer tuples for 1 block of raw data.
	 * @note   requires that SeqParser be passed in and operates on the Block's Iterators.
	 *          Mostly, this is because we need to broadcast the state of SeqParser to procs on the same node (L1 seq info) and recreate on child procs a new SeqParser (for L2)
	 * @tparam SeqParser		parser type for extracting sequences.  supports FASTQ and FASTA.  template template parameter, param is iterator
   * @tparam KP           parser type for generating Kmer.  supports kmer, kmer+pos, kmer+count, kmer+pos/qual.
	 * @tparam BlockType		input partition type, supports in memory (vector) vs memmapped.
	 * @param partition
	 * @param result        output vector.  should be pre allocated.
	 */
	template <typename KP, template <typename> class SeqParser, typename BlockType>
	static size_t read_block(BlockType & partition, SeqParser<typename BlockType::iterator> const &seq_parser, std::vector<typename KP::value_type>& result) {

		// from FileLoader type, get the block iter type and range type
		using BlockIterType = typename BlockType::iterator;

		using SeqIterType = ::bliss::io::SequencesIterator<BlockIterType, SeqParser >;
//		using SeqType = typename ::std::iterator_traits<SeqIterType>::value_type;

		//== sequence parser type
		KP kmer_parser;

		//== process the chunk of data

		//==  and wrap the chunk inside an iterator that emits Reads.
		SeqIterType seqs_start(seq_parser, partition.begin(), partition.end(), partition.getRange().start);
		SeqIterType seqs_end(partition.end());

    ::fsc::back_emplace_iterator<std::vector<typename KP::value_type> > emplace_iter(result);

    size_t before = result.size();

		//== loop over the reads
		for (; seqs_start != seqs_end; ++seqs_start)
		{
//		  std::cout << "** seq: " << (*seqs_start).id.id << ", ";
//		  ostream_iterator<typename std::iterator_traits<typename SeqType::IteratorType>::value_type> osi(std::cout);
//		  std::copy((*seqs_start).seq_begin, (*seqs_start).seq_end, osi);
//		  std::cout << std::endl;

			emplace_iter = kmer_parser(*seqs_start, emplace_iter);

//	    std::cout << "Last: pos - kmer " << result.back() << std::endl;

		}

		return result.size() - before;
	}



	/**
	 * @brief read a file's content and generate kmers, place in a vector as return result.
	 * @note  static so can be used wihtout instantiating a internal map.
	 * @tparam SeqParser		parser type for extracting sequences.  supports FASTQ and FASTA.   template template parameter, param is iterator
   * @tparam KmerParser   parser type for generating Kmer.  supports kmer, kmer+pos, kmer+count, kmer+pos/qual.
	 */
	template <template <typename> class SeqParser, typename KP = KmerParser, template<typename> class PreCanonicalizer=bliss::kmer::transform::identity>
	static size_t read_file(const std::string & filename, std::vector<typename KP::value_type>& result, MPI_Comm _comm) {

		int p, rank;
		MPI_Comm_size(_comm, &p);
		MPI_Comm_rank(_comm, &rank);

		using FileLoaderType = bliss::io::FileLoader<CharType, 0, SeqParser, false, false>; // raw data type :  use CharType

		//====  now process the file, one L1 block (block partition by MPI Rank) at a time

		size_t before = result.size();

		TIMER_INIT(file);
		{  // ensure that fileloader is closed at the end.

			TIMER_START(file);
			//==== create file Loader
			FileLoaderType loader(filename, _comm, 1, sysconf(_SC_PAGE_SIZE));  // this handle is alive through the entire building process.
			typename FileLoaderType::L1BlockType partition = loader.getNextL1Block();
			TIMER_END(file, "open", partition.getRange().size());

			//== reserve
			TIMER_START(file);
			// modifying the local index directly here causes a thread safety issue, since callback thread is already running.
			// index reserve internally sends a message to itself.

			// call after getting first L1Block to ensure that file is loaded.
			size_t est_size = (loader.getKmerCountEstimate(KmerType::size) + p - 1) / p;
			result.reserve(est_size);
			TIMER_END(file, "reserve", est_size);

      // not reusing the SeqParser in loader.  instead, reinitializing one.
      TIMER_START(file);
      auto l1parser = loader.getSeqParser();
      l1parser.init_parser(partition.begin(), loader.getFileRange(), partition.getRange(), partition.getRange(), _comm);
      TIMER_END(file, "mark_seqs", est_size);

			TIMER_START(file);
			//=== copy into array
			while (partition.getRange().size() > 0) {

				read_block<KP>(partition, l1parser, result);

				partition = loader.getNextL1Block();
			}
			TIMER_END(file, "read", result.size());
      // std::cout << "Last: pos - kmer " << result.back() << std::endl;
		}

		if (!::std::is_same<PreCanonicalizer<KmerType>, ::bliss::kmer::transform::identity<KmerType> >::value) {
      TIMER_START(file);

      ::bliss::kmer::transform::tuple_transform<KmerType, PreCanonicalizer> tuple_trans;
      ::std::for_each(result.begin(), result.end(), tuple_trans);

      TIMER_END(file, "canonicalize", result.size());
		}

		TIMER_REPORT_MPI(file, rank, _comm);
		return result.size() - before;
	}


	/**
	 * @tparam KmerParser		parser type for generating Kmer.  supports kmer, kmer+pos, kmer+count, kmer+pos/qual.
	 * @tparam SeqParser		parser type for extracting sequences.  supports FASTQ and FASTA.  template template parameter, param is iterator
	 */
	template <template <typename> class SeqParser, typename KP = KmerParser, template<typename> class PreCanonicalizer=bliss::kmer::transform::identity>
	static size_t read_file_mpi_subcomm(const std::string & filename, std::vector<typename KP::value_type>& result, const mxx::comm& comm) {

	  /*
		int p, rank;
		MPI_Comm_size(_comm, &p);
		MPI_Comm_rank(_comm, &rank);
		*/

		// split the communcator so 1 proc from each host does the read, then redistribute.
		mxx::comm group = comm.split_shared();
		mxx::comm group_leaders = comm.split(group.rank() == 0);

		/*
		int g_size, g_rank;
		MPI_Comm_rank(group, &g_rank);
		MPI_Comm_size(group, &g_size);
		*/

		/*
		int gl_size = MPI_UNDEFINED;
		int gl_rank = MPI_UNDEFINED;
		if (group_leaders != MPI_COMM_NULL) {
			MPI_Comm_rank(group_leaders, &gl_rank);
			MPI_Comm_size(group_leaders, &gl_size);
		}
		*/

		// raw data type :  use CharType.   block partition at L1 and L2.  no buffering at all, since we will be copying data to the group members anyways.
		using FileLoaderType = bliss::io::FileLoader<CharType, 0, SeqParser, false, false,
		    bliss::partition::BlockPartitioner<bliss::partition::range<size_t> >,  bliss::partition::BlockPartitioner<bliss::partition::range<size_t> >>;

		size_t before = result.size();

		typename FileLoaderType::RangeType file_range;

		TIMER_INIT(file);
		{

			typename FileLoaderType::L2BlockType block;
			typename FileLoaderType::RangeType range;
			::std::vector<CharType> data;

			::std::vector<typename FileLoaderType::RangeType> ranges;
			::std::vector<size_t> send_counts;
			typename FileLoaderType::L1BlockType partition;

			// first load the file using the group loader's communicator.
			TIMER_START(file);
			size_t est_size = 1;
			if (group.rank() == 0) {  // ensure file loader is closed properly.
				//==== create file Loader. this handle is alive through the entire building process.
				FileLoaderType loader(filename, group_leaders, group.size());  // for member of group_leaders, each create g_size L2blocks.

				// modifying the local index directly here causes a thread safety issue, since callback thread is already running.

				partition = loader.getNextL1Block();

				// call after getting first L1Block to ensure that file is loaded.
				est_size = (loader.getKmerCountEstimate(KmerType::size) + comm.size() - 1) / comm.size();

				//====  now compute the send counts and ranges to be scattered.
				ranges.resize(group.size());
				send_counts.resize(group.size());
				for (int i = 0; i < group.size(); ++i) {
					block = loader.getNextL2Block(i);
					ranges[i] = block.getRange();
					send_counts[i] = ranges[i].size();
				}

				// scatter the data .  this call here relies on FileLoader still having the memory mapped.
				// TODO; use iterators!
				data = mxx::scatterv(&(*partition.begin()), send_counts, 0, group);

				// send the file range.
				file_range = loader.getFileRange();


			} else {
        // replicated here because the root's copy needs to be inside the if clause - requires FileLoader to be open for the sender.

				// scatter the data.  this is the receiver end of the thing.
				data = mxx::scatterv(&(*partition.begin()), send_counts, 0, group);

			}

			// scatter the ranges
			// TODO: mxx::bcast function
      mxx::datatype range_dt = mxx::get_datatype<typename FileLoaderType::RangeType >();
			MPI_Bcast(&file_range, 1, range_dt.type(), 0, group);

			range = mxx::scatter_one(ranges, 0, group);
			// now create the L2Blocks from the data  (reuse block)
			block.assign(&(data[0]), &(data[0]) + range.size(), range);
			TIMER_END(file, "open", data.size());

			// not reusing the SeqParser in loader.  instead, reinitializing one.
      TIMER_START(file);
			SeqParser<typename FileLoaderType::L2BlockType::iterator> l2parser;
			l2parser.init_parser(block.begin(), file_range, range, range, comm);
      TIMER_END(file, "mark_seqs", est_size);

			//== reserve
			TIMER_START(file);
			// broadcast the estimated size
      mxx::datatype size_dt = mxx::get_datatype<size_t>();
			MPI_Bcast(&est_size, 1, size_dt.type(), 0, group);
			result.reserve(est_size);
			TIMER_END(file, "reserve", est_size);


			// == parse kmer/tuples iterator
			TIMER_START(file);
			read_block<KP>(block, l2parser, result);
			TIMER_END(file, "read", result.size());
      BL_INFO("Last: pos - kmer " << result.back());
		}

    if (!::std::is_same<PreCanonicalizer<KmerType>, ::bliss::kmer::transform::identity<KmerType> >::value) {
      TIMER_START(file);

      ::bliss::kmer::transform::tuple_transform<KmerType, PreCanonicalizer> tuple_trans;
      ::std::for_each(result.begin(), result.end(), tuple_trans);

      TIMER_END(file, "canonicalize", result.size());
    }

    TIMER_REPORT_MPI(file, comm.rank(), comm);

		return result.size() - before;
	}


	std::vector<TupleType> find(std::vector<KmerType> &query) {
		return map.find(query);
	}

	std::vector< std::pair<KmerType, size_t> > count(std::vector<KmerType> &query) {
		return map.count(query);
	}

	void erase(std::vector<KmerType> &query) {
		map.erase(query);
	}


	template <typename Predicate>
	std::vector<TupleType> find_if(std::vector<KmerType> &query, Predicate const &pred) {
		return map.find_if(query, pred);
	}
	template <typename Predicate>
	std::vector<TupleType> find_if(Predicate const &pred) {
		return map.find_if(pred);
	}

	template <typename Predicate>
	std::vector<std::pair<KmerType, size_t> > count_if(std::vector<KmerType> &query, Predicate const &pred) {
		return map.count_if(query, pred);
	}

	template <typename Predicate>
	std::vector<std::pair<KmerType, size_t> > count_if(Predicate const &pred) {
		return map.count_if(pred);
	}


	template <typename Predicate>
	void erase_if(std::vector<KmerType> &query, Predicate const &pred) {
		map.erase_if(query, pred);
	}

	template <typename Predicate>
	void erase_if(Predicate const &pred) {
		map.erase_if(pred);
	}

	size_t local_size() {
		return map.local_size();
	}

	/**
	 * @tparam T 	input type may not be same as map's value types, so map need to provide overloads (and potentially with transform operators)
	 */
	 template <typename T>
	void insert(std::vector<T> &temp) {
		TIMER_INIT(build);

		// do not reserve until insertion - less transient memory used.
//		TIMER_START(build);
//		this->map.reserve(this->map.size() + temp.size());
//		TIMER_END(build, "reserve", temp.size());

		// distribute
		TIMER_START(build);
		this->map.insert(temp);
		TIMER_END(build, "insert", this->map.local_size());


		TIMER_START(build);
		size_t m = 0;
    m = this->map.update_multiplicity();
		TIMER_END(build, "multiplicity", m);

		TIMER_REPORT_MPI(build, this->comm.rank(), this->comm);

	 }

	 // Note that KmerParserType may depend on knowing the Sequence Parser Type (e.g. provide quality score iterators)
	 //	Output type of KmerParserType may not match Map value type, in which case the map needs to do its own transform.
	 //     since Kmer template parameter is not explicitly known, we can't hard code the return types of KmerParserType.

	 /// convenience function for building index.
	 template <template <typename> class SeqParser, template<typename> class PreCanonicalizer=bliss::kmer::transform::identity>
	 void build(const std::string & filename, MPI_Comm comm) {

	   // file extension determines SeqParserType
	   std::string extension = ::bliss::utils::file::get_file_extension(filename);
	   std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	   if ((extension.compare("fastq") != 0) && (extension.compare("fasta") != 0)) {
	     throw std::invalid_argument("input filename extension is not supported.");
	   }

	   // check to make sure that the file parser will work
	   if ((extension.compare("fastq") == 0) && (!std::is_same<SeqParser<char*>, ::bliss::io::FASTQParser<char*> >::value)) {
	     throw std::invalid_argument("Specified File Parser template parameter does not support files with fastq extension.");
	   } else if ((extension.compare("fasta") == 0) && (!std::is_same<SeqParser<char*>, ::bliss::io::FASTAParser<char*> >::value)) {
       throw std::invalid_argument("Specified File Parser template parameter does not support files with fasta extension.");
     }

	   // proceed
     ::std::vector<typename KmerParser::value_type> temp;
     this->read_file<SeqParser, KmerParser, PreCanonicalizer >(filename, temp, comm);


//        // dump the generated kmers to see if they look okay.
//         std::stringstream ss;
//         ss << "test." << commRank << ".log";
//         std::ofstream ofs(ss.str());
//         for (int i = 0; i < temp.size(); ++i) {
//          ofs << "item: " << i << " value: " << temp[i] << std::endl;
//         }
//         ofs.close();

     BL_DEBUG("Last: pos - kmer " << temp.back());
     this->insert(temp);

	 }


   template <template <typename> class SeqParser, template<typename> class PreCanonicalizer=bliss::kmer::transform::identity>
	 void build_with_mpi_subcomm(const std::string & filename, MPI_Comm comm) {

     // file extension determines SeqParserType
     std::string extension = ::bliss::utils::file::get_file_extension(filename);
     std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
     if ((extension.compare("fastq") != 0) && (extension.compare("fasta") != 0)) {
       throw std::invalid_argument("input filename extension is not supported.");
     }

     // check to make sure that the file parser will work
     if ((extension.compare("fastq") == 0) && (!std::is_same<SeqParser<char*>, ::bliss::io::FASTQParser<char*> >::value)) {
       throw std::invalid_argument("Specified File Parser template parameter does not support files with fastq extension.");
     } else if ((extension.compare("fasta") == 0) && (!std::is_same<SeqParser<char*>, ::bliss::io::FASTAParser<char*> >::value)) {
       throw std::invalid_argument("Specified File Parser template parameter does not support files with fasta extension.");
     }

     // proceed
     ::std::vector<typename KmerParser::value_type> temp;
     this->read_file_mpi_subcomm<SeqParser, KmerParser, PreCanonicalizer  >(filename, temp, comm);

     BL_DEBUG("Last: pos - kmer " << temp.back());
     this->insert(temp);

	 }


   typename MapType::const_iterator cbegin() const
   {
     return map.cbegin();
   }

   typename MapType::const_iterator cend() const {
     return map.cend();
   }

   size_t size() const {
     return map.size();
   }

   size_t local_size() const {
     return map.local_size();
   }

};


struct NotEOL {
  template <typename CharType>
  bool operator()(CharType const & x) {
	return (x != '\n') && (x != '\r' );
  }

  template <typename CharType, typename MDType>
  bool operator()(std::pair<CharType, MDType> const & x) {
	  return (x.first != '\n') && (x.first != '\r');
  }
};

template <typename Iter>
using NonEOLIter = bliss::iterator::filter_iterator<NotEOL, Iter>;



/**
 * @tparam KmerType       output value type of this parser.  not necessarily the same as the map's final storage type.
 */
template <typename KmerType>
struct KmerParser {

    /// type of element generated by this parser.  since kmer itself is parameterized, this is not hard coded.
	using value_type = KmerType;


  /**
   * @brief generate kmers from 1 sequence.  result inserted into output_iter, which may be preallocated.
   * @param read          sequence object, which has pointers to the raw byte array.
   * @param output_iter   output iterator pointing to insertion point for underlying container.
   * @return new position for output_iter
   * @tparam SeqType      type of sequence.  inferred.
   * @tparam OutputIt     output iterator type, inferred.
   */
	template <typename SeqType, typename OutputIt>
	OutputIt operator()(SeqType & read, OutputIt output_iter) {

		static_assert(std::is_same<KmerType, typename ::std::iterator_traits<OutputIt>::value_type>::value,
						"output type and output container value type are not the same");

		using Alphabet = typename KmerType::KmerAlphabet;

		/// converter from ascii to alphabet values
		using BaseCharIterator = bliss::iterator::transform_iterator<NonEOLIter<typename SeqType::IteratorType>, bliss::common::ASCII2<Alphabet> >;

		/// kmer generation iterator
		using KmerIterType = bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>;

		static_assert(std::is_same<typename std::iterator_traits<KmerIterType>::value_type,
				KmerType>::value,
				"input iterator and output iterator's value types differ");

		// then compute and store into index (this will generate kmers and insert into index)
    if (::std::distance(read.seq_begin, read.seq_end) < KmerType::size) return output_iter;

		//== filtering iterator
		NotEOL neol;
		NonEOLIter<typename SeqType::IteratorType> eolstart(neol, read.seq_begin, read.seq_end);
		NonEOLIter<typename SeqType::IteratorType> eolend(neol, read.seq_end);

		//== set up the kmer generating iterators.
		KmerIterType start(BaseCharIterator(eolstart, bliss::common::ASCII2<Alphabet>()), true);
		KmerIterType end(BaseCharIterator(eolend, bliss::common::ASCII2<Alphabet>()), false);

//    printf("First: pos %lu kmer %s\n", read.id.id, bliss::utils::KmerUtils::toASCIIString(*start).c_str());

		return ::std::copy(start, end, output_iter);

	}
};


/**
 * @tparam TupleType       output value type of this parser.  not necessarily the same as the map's final storage type.
 */
template <typename TupleType>
struct KmerPositionTupleParser {

    /// type of element generated by this parser.  since kmer itself is parameterized, this is not hard coded.
	using value_type = TupleType;


  /**
   * @brief generate kmer-position pairs from 1 sequence.  result inserted into output_iter, which may be preallocated.
   * @param read          sequence object, which has pointers to the raw byte array.
   * @param output_iter   output iterator pointing to insertion point for underlying container.
   * @return new position for output_iter
   * @tparam SeqType      type of sequence.  inferred.
   * @tparam OutputIt     output iterator type, inferred.
   */
	template <typename SeqType, typename OutputIt>
	OutputIt operator()(SeqType & read, OutputIt output_iter) {

		static_assert(std::is_same<TupleType, typename ::std::iterator_traits<OutputIt>::value_type>::value,
						"output type and output container value type are not the same");
		static_assert(::std::tuple_size<TupleType>::value == 2, "kmer-pos-qual index data type should be a pair");

		using KmerType = typename std::tuple_element<0, TupleType>::type;
		using Alphabet = typename KmerType::KmerAlphabet;

		// filter out EOL characters
		using CharIter = NonEOLIter<typename SeqType::IteratorType>;
    // converter from ascii to alphabet values
    using BaseCharIterator = bliss::iterator::transform_iterator<CharIter, bliss::common::ASCII2<Alphabet> >;
    // kmer generation iterator
    using KmerIter = bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>;

		//== next figure out starting positions for the kmers, accounting for EOL char presenses.
		using IdType = typename std::tuple_element<1, TupleType>::type;
		// kmer position iterator type
		using IdIterType = bliss::iterator::SequenceIdIterator<IdType>;

    // use zip iterator to tie together the iteration of sequence raw data and id.
    using PairedIter = bliss::iterator::ZipIterator<typename SeqType::IteratorType, IdIterType>;
    using CharPosIter = NonEOLIter<PairedIter>;
		// now use 2 unzip iterators to access the values.  one of them advances.  all subsequent wrapping
		// iterators trickle back to the zip iterator above, again, one of the 2 unzip iterator will call operator++ on the underlying zip iterator
		using IdIter = bliss::iterator::AdvancingUnzipIterator<CharPosIter, 1>;

		// rezip the results
		using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIter, IdIter>;

		static_assert(std::is_same<typename std::iterator_traits<KmerIndexIterType>::value_type,
				TupleType>::value,
				"input iterator and output iterator's value types differ");


		// then compute and store into index (this will generate kmers and insert into index)
		if (::std::distance(read.seq_begin, read.seq_end) < KmerType::size) return output_iter;  // if too short...

    //== set up the kmer generating iterators.
    NotEOL neol;
    KmerIter start(BaseCharIterator(CharIter(neol, read.seq_begin, read.seq_end), bliss::common::ASCII2<Alphabet>()), true);
    KmerIter end(BaseCharIterator(CharIter(neol, read.seq_end), bliss::common::ASCII2<Alphabet>()), false);


		//== set up the position iterators
		IdType seq_begin_id(read.id);
		seq_begin_id += read.seq_offset;  // change id to point to start of sequence (in file coord)
		IdType seq_end_id(seq_begin_id);
		seq_end_id += std::distance(read.seq_begin, read.seq_end);

		// tie chars and id together
		PairedIter pp_begin(read.seq_begin, IdIterType(seq_begin_id));
		PairedIter pp_end(read.seq_end, IdIterType(seq_end_id));

    // filter eol
		CharPosIter cp_begin(neol, pp_begin, pp_end);
    CharPosIter cp_end(neol, pp_end);

		// ==== extract new id and rezip iterators
		KmerIndexIterType index_start(start, IdIter(cp_begin));
		KmerIndexIterType index_end(end, IdIter(cp_end));


//		for (; index_start != index_end; ++index_start) {
//		  auto tp = *index_start;
//
//		  printf("TCP id = %lu, pos = %lu, kmer = %s\n", tp.second.get_id(), tp.second.get_pos(), bliss::utils::KmerUtils::toASCIIString(tp.first).c_str());
//
//		  *output_iter = tp;
//
//		}
		return ::std::copy(index_start, index_end, output_iter);
	}

};


/**
 * @tparam TupleType       output value type of this parser.  not necessarily the same as the map's final storage type.
 */
template <typename TupleType, template<typename> class QualityEncoder = bliss::index::Illumina18QualityScoreCodec>
struct KmerPositionQualityTupleParser {

    /// type of element generated by this parser.  since kmer itself is parameterized, this is not hard coded.
	using value_type = TupleType;

  /**
   * @brief generate kmer-position-quality pairs from 1 sequence.  result inserted into output_iter, which may be preallocated.
   * @param read          sequence object, which has pointers to the raw byte array.
   * @param output_iter   output iterator pointing to insertion point for underlying container.
   * @return new position for output_iter
   * @tparam SeqType      type of sequence.  inferred.
   * @tparam OutputIt     output iterator type, inferred.
   */
	template <typename SeqType, typename OutputIt>
	OutputIt operator()(SeqType & read, OutputIt output_iter) {


		static_assert(SeqType::has_quality(), "Sequence Parser needs to support quality scores");

		static_assert(std::is_same<TupleType, typename ::std::iterator_traits<OutputIt>::value_type>::value,
				"output type and output container value type are not the same");

		static_assert(::std::tuple_size<TupleType>::value == 2, "kmer-pos-qual index data type should be a pair");

		using KmerType = typename std::tuple_element<0, TupleType>::type;
		using Alphabet = typename KmerType::KmerAlphabet;

		static_assert(::std::tuple_size<typename std::tuple_element<1, TupleType>::type>::value == 2, "pos-qual index data type should be a pair");


    // filter out EOL characters
    using CharIter = NonEOLIter<typename SeqType::IteratorType>;
    // converter from ascii to alphabet values
    using BaseCharIterator = bliss::iterator::transform_iterator<CharIter, bliss::common::ASCII2<Alphabet> >;
    // kmer generation iterator
    using KmerIter = bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>;

    //== next figure out starting positions for the kmers, accounting for EOL char presenses.
    using IdType = typename std::tuple_element<0, typename std::tuple_element<1, TupleType>::type >::type;
    // kmer position iterator type
    using IdIterType = bliss::iterator::SequenceIdIterator<IdType>;

    // use zip iterator to tie together the iteration of sequence raw data and id.
    using PairedIter = bliss::iterator::ZipIterator<typename SeqType::IteratorType, IdIterType>;
    using CharPosIter = NonEOLIter<PairedIter>;
    // now use 2 unzip iterators to access the values.  one of them advances.  all subsequent wrapping
    // iterators trickle back to the zip iterator above, again, one of the 2 unzip iterator will call operator++ on the underlying zip iterator
    using IdIter = bliss::iterator::AdvancingUnzipIterator<CharPosIter, 1>;


		using QualType = typename std::tuple_element<1, typename std::tuple_element<1, TupleType>::type>::type;
		//static_assert(::std::is_same<typename SeqType::IdType, IdType>::value, "position type does not match for input and output iterators" );

		// also remove eol from quality score
		using QualIterType =
				bliss::index::QualityScoreGenerationIterator<NonEOLIter<typename SeqType::IteratorType>, KmerType::size, QualityEncoder<QualType> >;

		/// combine kmer iterator and position iterator to create an index iterator type.
		using KmerInfoIterType = bliss::iterator::ZipIterator<IdIter, QualIterType>;

		static_assert(std::is_same<typename std::iterator_traits<KmerInfoIterType>::value_type,
				typename std::tuple_element<1, TupleType>::type >::value,
				"kmer info input iterator and output iterator's value types differ");


		using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIter, KmerInfoIterType>;

		static_assert(std::is_same<typename std::iterator_traits<KmerIndexIterType>::value_type,
				TupleType>::value,
				"input iterator and output iterator's value types differ");


		// then compute and store into index (this will generate kmers and insert into index)
		if ((::std::distance(read.seq_begin, read.seq_end) < KmerType::size) ||
		    (::std::distance(read.qual_begin, read.qual_end) < KmerType::size)) return output_iter;
		assert(::std::distance(read.seq_begin, read.seq_end) <= ::std::distance(read.qual_begin, read.qual_end));


    //== set up the kmer generating iterators.
    NotEOL neol;
    KmerIter start(BaseCharIterator(CharIter(neol, read.seq_begin, read.seq_end), bliss::common::ASCII2<Alphabet>()), true);
    KmerIter end(BaseCharIterator(CharIter(neol, read.seq_end), bliss::common::ASCII2<Alphabet>()), false);


    //== set up the position iterators
    IdType seq_begin_id(read.id);
    seq_begin_id += read.seq_offset;  // change id to point to start of sequence (in file coord)
    IdType seq_end_id(seq_begin_id);
    seq_end_id += std::distance(read.seq_begin, read.seq_end);

    // tie chars and id together
    PairedIter pp_begin(read.seq_begin, IdIterType(seq_begin_id));
    PairedIter pp_end(read.seq_end, IdIterType(seq_end_id));

    // filter eol
    CharPosIter cp_begin(neol, pp_begin, pp_end);
    CharPosIter cp_end(neol, pp_end);

    // ==== quality scoring
		// filter eol and generate quality scores
		QualIterType qual_start(CharIter(neol, read.qual_begin, read.qual_end));
		QualIterType qual_end(CharIter(neol, read.qual_end));

		KmerInfoIterType info_start(IdIter(cp_begin), qual_start);
		KmerInfoIterType info_end(IdIter(cp_end), qual_end);


		// ==== set up the zip iterators
		KmerIndexIterType index_start(start, info_start);
		KmerIndexIterType index_end(end, info_end);

		//    printf("First: pos %lu kmer %s\n", read.id.id, bliss::utils::KmerUtils::toASCIIString(*start).c_str());

		return ::std::copy(index_start, index_end, output_iter);
	}
};



/**
 * @tparam TupleType       output value type of this parser.  not necessarily the same as the map's final storage type.
 */
template <typename TupleType>
struct KmerCountTupleParser {

  /// type of element generated by this parser.  since kmer itself is parameterized, this is not hard coded.
	using value_type = TupleType;

	/**
	 * @brief generate kmer-count pairs from 1 sequence.  result inserted into output_iter, which may be preallocated.
	 * @param read          sequence object, which has pointers to the raw byte array.
	 * @param output_iter   output iterator pointing to insertion point for underlying container.
	 * @return new position for output_iter
	 * @tparam SeqType      type of sequence.  inferred.
	 * @tparam OutputIt     output iterator type, inferred.
	 */
	template <typename SeqType, typename OutputIt>
	OutputIt operator()(SeqType & read, OutputIt output_iter) {

		static_assert(std::is_same<TupleType, typename ::std::iterator_traits<OutputIt>::value_type>::value,
				"output type and output container value type are not the same");
		static_assert(::std::tuple_size<TupleType>::value == 2, "count data type should be a pair");

		using KmerType = typename std::tuple_element<0, TupleType>::type;
		using CountType = typename std::tuple_element<1, TupleType>::type;

		using Alphabet = typename KmerType::KmerAlphabet;

		/// converter from ascii to alphabet values
		using BaseCharIterator = bliss::iterator::transform_iterator<NonEOLIter<typename SeqType::IteratorType>, bliss::common::ASCII2<Alphabet> >;

		/// kmer generation iterator
		using KmerIterType = bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>;
		using CountIterType = bliss::iterator::ConstantIterator<CountType>;

		using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIterType, CountIterType>;

		static_assert(std::is_same<typename std::iterator_traits<KmerIndexIterType>::value_type, std::pair<KmerType, CountType> >::value,
				"count zip iterator not producing the right type.");

		static_assert(std::is_same<typename std::iterator_traits<KmerIndexIterType>::value_type,
				TupleType>::value,
				"count: input iterator and output iterator's value types differ");

		CountIterType count_start(1);

		// then compute and store into index (this will generate kmers and insert into index)
    if (::std::distance(read.seq_begin, read.seq_end) < KmerType::size) return output_iter;

		//== set up the kmer generating iterators.
		NotEOL neol;
		NonEOLIter<typename SeqType::IteratorType> eolstart(neol, read.seq_begin, read.seq_end);
		NonEOLIter<typename SeqType::IteratorType> eolend(neol, read.seq_end);

		KmerIterType start(BaseCharIterator(eolstart, bliss::common::ASCII2<Alphabet>()), true);
		KmerIterType end(BaseCharIterator(eolend, bliss::common::ASCII2<Alphabet>()), false);

		KmerIndexIterType istart(start, count_start);
		KmerIndexIterType iend(end, count_start);

		//    printf("First: pos %lu kmer %s\n", read.id.id, bliss::utils::KmerUtils::toASCIIString(*start).c_str());

		return ::std::copy(istart, iend, output_iter);
	}

};

// TODO: the types of Map that is used should be restricted.  (perhaps via map traits)
template <typename MapType>
using KmerIndex = Index<MapType, KmerParser<typename MapType::key_type> >;

template <typename MapType>
using PositionIndex = Index<MapType, KmerPositionTupleParser<std::pair<typename MapType::key_type, typename MapType::mapped_type> > >;

template <typename MapType>
using PositionQualityIndex = Index<MapType, KmerPositionQualityTupleParser<std::pair<typename MapType::key_type, typename MapType::mapped_type> > >;

template <typename MapType>
using CountIndex = Index<MapType, KmerCountTupleParser<std::pair<typename MapType::key_type, typename MapType::mapped_type> > >;
template <typename MapType>
using CountIndex2 = Index<MapType, KmerParser<typename MapType::key_type> >;


} /* namespace kmer */

} /* namespace index */
} /* namespace bliss */

#endif /* KMERINDEX2_HPP_ */
