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
 *		  PrefixHasher is used to distribute to MPI processes based on the first ceil(log_2(P)) bits of the kmer
 *		  PrefixHasher is used to distribute to threads within a process, based on the next ceil(log_2(P+T)) bits of the kmer
 *		  SuffixHasher is used for threadlocal unordered_map hashing
 *
 *		All are deterministic to allow simple lookup processes.
 *
 *
 * Copyright (c) 2014 Georgia Institute of Technology.  All Rights Reserved.
 *
 * TODO add License
 */
#ifndef KMERINDEX_HPP_
#define KMERINDEX_HPP_

#include "bliss-config.hpp"


#if defined(USE_MPI)
#include "mpi.h"
#endif

#if defined(USE_OPENMP)
#include "omp.h"
#endif

#include <unistd.h>     // sysconf
#include <sys/stat.h>   // block size.

#include "common/kmer.hpp"
#include "common/base_types.hpp"
#include "common/alphabet_traits.hpp"
#include "io/fastq_loader.hpp"
//#include "io/fasta_loader.hpp"
//#include "io/fasta_iterator.hpp"
#include "io/file_loader.hpp"
#include "io/communication_layer.hpp"
#include "index/distributed_map.hpp"
#include "iterators/zip_iterator.hpp"
#include "iterators/transform_iterator.hpp"
#include "io/sequence_id_iterator.hpp"
#include "io/sequence_iterator.hpp"
#include "common/kmer_iterators.hpp"
#include "index/quality_score_iterator.hpp"
#include "common/kmer_hash.hpp"

#include "retired/kmer_index_generator.hpp"
#include "retired/kmer_index_functors.hpp"
#include "retired/buffered_transform_iterator.hpp"

namespace bliss
{
  namespace index
  {

    //Template specialized later
    template<typename MapType, typename FileFormat, typename Quality = void>
    class KmerIndex;

    //Template specialized later
    template<typename KmerType, typename FileFormat>
    class KmerCountIndex;

    /**
     * @class    bliss::index::KmerIndex
     * @brief    base type for All Kmer Index
     * @details  this is a base classe that provides basic file reading and sequence iteration
     * 			capabilities.  Subclasses only need to override the buildSequence or buildL2Block
     * 			methods to provide custom Kmer indexing such as Count index, position index,
     * 			and position + quality index.
     *
     */
    template<typename MapType, typename Quality>
    class KmerIndex<MapType, bliss::io::FASTQ, Quality> {
      public:
        //==================== COMMON TYPES

        /// DEFINE file loader.  this only provides the L1 and L2 blocks, not reads.
        using FileLoaderType = bliss::io::FASTQLoader<CharType, true, false>; // raw data type :  use CharType

        // from FileLoader type, get the block iter type and range type
        using FileBlockIterType = typename FileLoaderType::L2BlockType::iterator;

        /// DEFINE the iterator parser to get fastq records.  this type determines both the file format and whether quality scoring is used or not.
        using ParserType = bliss::io::FASTQParser<FileBlockIterType>;

        /// DEFINE Kmer type, extracted from LocalContainer's key type
        using KmerType = typename MapType::value_type::first_type;
        /// DEFINE Value type, extracted from LocalContainer's value type
        using ValueType = typename MapType::value_type::second_type;

        /// DEFINE the transform iterator type for parsing the FASTQ file into sequence records.
        using SeqIterType = bliss::io::SequencesIterator<FileBlockIterType, bliss::io::FASTQParser>;

        /// index instance to store data
        MapType index;

        /// MPI communicator used by index and fileloader.
        MPI_Comm comm;

        /// MPI rank within the communicator
        int rank;

        /// size of communicator
        int commSize;

      protected:
        using SeqType = typename ::std::iterator_traits<SeqIterType>::value_type;

      public:
        /**
         * @brief initializes the index
         * @param comm				MPI communicator
         * @param comm_size
         * @param callbackFunction  for query result handling.
         * @param num_threads
         */
        KmerIndex(MPI_Comm _comm, int _comm_size,
                  const std::function<void(std::pair<KmerType, ValueType>*, std::size_t)> &callbackFunction,
                  int num_threads = 1) :
          index(_comm, _comm_size, num_threads, 
                typename MapType::ThreadHashType(ceilLog2(num_threads * _comm_size)),
                typename MapType::ProcessHashType(ceilLog2(_comm_size))),
          comm(_comm), commSize(_comm_size) {
          MPI_Comm_rank(comm, &rank);

          this->index.setLookupAnswerCallback(callbackFunction);
        };

        /// default destructor
        virtual ~KmerIndex() {
          index.finish();
        };

        /// Finalize terminates communication, thus preventing further index modification and query.
        void finalize() {
          index.finish();
        }

        /// get size of the local portion of the distributed map.  primarily for debugging
        size_t local_size() const {
          return index.local_size();
        }

        /// get size of the local portion of the distributed map.  primarily for debugging
        std::vector<size_t> local_sizes() const {
          return index.local_sizes();
        }

        /// access the local portion of the distributed map.
        MapType& getLocalIndex() {
          return index;
        }

        /**
         * @brief   build index from file using specified number of threads
         * @note    default to chunksize = system PAGE SIZE
         * @param filename
         * @param nthreads
         */
        void build(const std::string &filename, const int nthreads) {
          build(filename, nthreads, sysconf(_SC_PAGE_SIZE));
        }

        /**
         * @brief build index from a file using the specified number of threads.
         * @param filename
         * @param nthreads
         * @param chunkSize
         */
        void build(const std::string &filename, const int nthreads, const int chunkSize) {
          // scoped so FileLoader is deleted after.
          {
            std::chrono::high_resolution_clock::time_point t1, t2;
            std::chrono::duration<double> time_span;

            t1 = std::chrono::high_resolution_clock::now();
            //==== create file Loader
            FileLoaderType loader(filename, comm, nthreads, chunkSize);  // this handle is alive through the entire building process.
            typename FileLoaderType::L1BlockType partition = loader.getNextL1Block();
            t2 = std::chrono::high_resolution_clock::now();
             time_span =
                 std::chrono::duration_cast<std::chrono::duration<double>>(
                     t2 - t1);
             INFOF("R %d file open time: %f", rank, time_span.count());

             t1 = std::chrono::high_resolution_clock::now();
            // modifying the local index directly here causes a thread safety issue, since callback thread is already running.
            // index reserve internally sends a message to itself.
             // call after getting first L1Block to ensure that file is loaded.
            index.reserve((loader.getKmerCountEstimate(KmerType::size) + commSize - 1) / commSize);
            t2 = std::chrono::high_resolution_clock::now();
             time_span =
                 std::chrono::duration_cast<std::chrono::duration<double>>(
                     t2 - t1);
             INFOF("R %d reserve time: %f", rank, time_span.count());


             t1 = std::chrono::high_resolution_clock::now();
            //====  now process the file, one L1 block (block partition by MPI Rank) at a time
            while (partition.getRange().size() > 0) {
              buildForL1Block(loader, index, rank, nthreads, chunkSize);
              partition = loader.getNextL1Block();
            }
            t2 = std::chrono::high_resolution_clock::now();
             time_span =
                 std::chrono::duration_cast<std::chrono::duration<double>>(
                     t2 - t1);
             INFOF("R %d file load/kmer gen time: %f", rank, time_span.count());

            // make index communicate all pending messages to other processes.
             t1 = std::chrono::high_resolution_clock::now();
            index.flush();
            t2 = std::chrono::high_resolution_clock::now();
             time_span =
                 std::chrono::duration_cast<std::chrono::duration<double>>(
                     t2 - t1);
             INFOF("R %d kmer flush time: %f", rank, time_span.count());
          }  // scope to ensure file loader is destroyed.

          auto sizes = index.local_sizes();
          std::stringstream ss;
          std::copy(sizes.begin(), sizes.end(), std::ostream_iterator<size_t>(ss, ","));
          INFOF("R %d kmer local per thread assignment load balance: [%s]", rank, ss.str().c_str());
        }

        //============== Query API.  sendQuery allows overlapped IO and computation

        /**
         * @brief    Add a new query to buffer for asynchronous communication.
         * @details  when the buffer is full, then the query is executed.  result is handled via callback
         * @param 	 kmer
         */
        void sendQuery(const KmerType & kmer) {
          index.asyncLookup(kmer);
        }

        /**
         * @brief flush all pending the queries to remote nodes.
         * @return
         */
        void flushQuery() {
          index.flush();
        }

        /**
         * @brief send a set of queries in a collective/blocking manner immediately.
         * @param kmers
         * @return
         */
        void query(const std::vector<KmerType>& kmers) {
          // TODO: for now, use the async api
          for (auto & kmer : kmers) {
            index.asyncLookup(kmer);
          }
          index.flush();
          // TODO: later, use MPI collective calls.  (require that a single thread performs this.
        }

        // TODO: filter API.

        // TODO: histogram API


      protected:
        /**
         * @brief processes a single L1Block to build the index.
         * @details  parses a L1Block as L2Blocks, which contain sequences (e.g. for FASTQ, reads)
         *           and then call buildForSequence to process each sequence.
         *
         * @note  fastq specific. fasta would not have "SeqIterType"
         *
         * @param loader
         * @param index
         * @param rank
         * @param nthreads
         * @param chunkSize
         */
        void buildForL1Block(FileLoaderType &loader, MapType &index, const int& rank,
                             const int& nthreads, const int& chunkSize)
        {
          // get L2Block from L1Block, spread work to all threads.
#pragma omp parallel num_threads(nthreads) shared(loader, nthreads, index, rank) OMP_SHARE_DEFAULT
          {
            //== instantiate a local parser in each thread
            ParserType parser;

            int tid = 0;
#ifdef USE_OPENMP
            tid = omp_get_thread_num();
#endif
            //== local variables for loop
            typename FileLoaderType::L2BlockType chunk;

            //== process L2Blocks in a loop.  loader.getNextL2Block is atomic.  the work is shared by all threads.
            chunk = loader.getNextL2Block(tid);
            while (chunk.getRange().size() > 0) {  // range size = 0 when finished traversal
              buildForL2Block(chunk, parser, index);

              //== get read for next loop iteration.
              chunk = loader.getNextL2Block(tid);
            }
          }  // compute threads parallel

        }

        /**
         * @brief processes a single L2Block to build the index.
         * @details  parses a L2Block into short sequences (e.g. for FASTQ, reads)
         *           and then call buildForSequence to process each sequence.
         *
         * @note  fastq specific. fasta qould not have "SeqIterType"
         * @param chunk
         * @param parser
         * @param index
         */
        virtual void buildForL2Block(typename FileLoaderType::L2BlockType &chunk, ParserType& parser, MapType& index) {
          if (chunk.getRange().size() == 0) return;

          //== process the chunk of data
          SeqType read;

          //==  and wrap the chunk inside an iterator that emits Reads.
          SeqIterType seqs_start(parser, chunk.begin(), chunk.end(), chunk.getRange().start);
          SeqIterType seqs_end(chunk.end());

          //== loop over the reads
          for (; seqs_start != seqs_end; ++seqs_start)
          {
            // first get read
            read = *seqs_start;

            // then compute and store into index (this will generate kmers and insert into index)
            buildForSequence(read, index);
          }
        }

        /**
         * @brief processes a single FASTQ read or sequence into index, and insert (atomic)
         * @param read
         * @param index
         */
        virtual void buildForSequence(SeqType &read, MapType& index) = 0;

    };


    /**
     * @class    bliss::index::KmerCountIndex
     * @brief    Index for counting Kmers.
     * @details  extends from base KmerIndex class.
     *			for counting kmers.
     */
    template<unsigned int Kmer_Size, typename Alphabet, typename WordType, template <unsigned int, typename, typename> class KmerT>
    class KmerCountIndex< KmerT<Kmer_Size, Alphabet, WordType>, bliss::io::FASTQ>
    : public KmerIndex<
      bliss::index::distributed_counting_map<
        KmerT<Kmer_Size, Alphabet, WordType>,
        bliss::io::CommunicationLayer<true>,
        bliss::hash::kmer::hash<KmerT<Kmer_Size, Alphabet, WordType>, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
        bliss::hash::kmer::hash<KmerT<Kmer_Size, Alphabet, WordType>, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
        bliss::hash::kmer::hash<KmerT<Kmer_Size, Alphabet, WordType>, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>
      >,
      bliss::io::FASTQ,
      void>
    {
      public:
        /// DEFINE kmer index key type, also used for reverse complement
        using KmerType = KmerT<Kmer_Size, Alphabet, WordType>;

        /// DEFINE the index storage type.  using an infix of kmer to map to threads,
        /// since prefix is used for distributing to processors.
        using MapType = bliss::index::distributed_counting_map<
            KmerType,
            bliss::io::CommunicationLayer<true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>
        >;

        /// DEFINE kmer index value type (std::pair)
        using ValueType = typename MapType::value_type;

        /// define count type for Kmers
        using CountType = typename ValueType::second_type;

        /// DEFINE kmer index type
        using BaseIndexType = KmerIndex<MapType, bliss::io::FASTQ , void>;

        // once have ParserType, then get the sequence type and sequence id type
        using SeqType = typename BaseIndexType::SeqType;

        //==========below are to be redefined for each index type
        ///////////// INDEX TYPES
        /// converter from ascii to alphabet values
        using BaseCharIterator = bliss::iterator::transform_iterator<typename SeqType::IteratorType, bliss::common::ASCII2<Alphabet> >;

        /// kmer generation iterator
        typedef bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>             KmerIterType;
      protected:

        /// kmer iterator is the index element iterator type here.
        using KmerIndexIterType = KmerIterType;

      public:
        /**
         * @brief 		initializes the count index
         * @param comm
         * @param comm_size
         * @param callbackFunction  for query result handling.
         * @param num_threads
         */
        KmerCountIndex(MPI_Comm _comm, int _comm_size,
                       const std::function<void(std::pair<KmerType, bliss::index::count_t>*, std::size_t)> &callbackFunction,
                       int num_threads = 1) :
                         BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
      {
          this->index.init();
      };
        /// default destructor
		virtual ~KmerCountIndex() {};


        /**
         *  @brief 	default Count Index Lookup Result callback function
         */
        static void defaultReceiveAnswerCallback(std::pair<KmerType, CountType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
        {

          std::chrono::high_resolution_clock::time_point t1, t2;
          std::chrono::duration<double> time_span;

          t1 = std::chrono::high_resolution_clock::now();

          size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
          KmerType key;
          CountType val;

#pragma omp for
          for (size_t i = 0; i < answer_count; ++i) {
        	  std::tie(key, val) = answers[i];

            ++entries;
            if ((entries % 1000000) == 0)
            	INFOF("count result:  %s <=> %d", key.toString().c_str(), val);
          }
          }
          total_entries += entries;

          t2 = std::chrono::high_resolution_clock::now();
            time_span =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    t2 - t1);

          time += time_span.count();
        };



      protected:


        /**
         * @brief overriden method to process sequence into kmers and insert
         * @param read
         * @param index
         */
        virtual void buildForSequence(SeqType &read, MapType& index) {

          if (read.seq_begin == read.seq_end) return;

          //== transform ascii to coded value
          BaseCharIterator charStart(read.seq_begin, bliss::common::ASCII2<Alphabet>());
          BaseCharIterator charEnd(read.seq_end, bliss::common::ASCII2<Alphabet>());

          //== set up the kmer generating iterators.
          KmerIterType start(charStart, true);
          KmerIterType end(charEnd, false);

          // NOTE: need to call *start to actually evaluate.  question is whether ++ should be doing computation.
          for (; start != end; ++start)
          {
            index.insert(*start);  // right side either RVO assignment (if iterator/functor created the object) or copy (if returning a cached object)
            // then copy assign
          }
        }
    };



    /**
     * @class    bliss::index::KmerPositionIndex
     * @brief    Index for Kmer positions in source sequence.
     * @details  extends from base KmerIndex class.
     *			 positions are in file offsets to the beginning of sequence, then offset within the read.
     */
    template<unsigned int Kmer_Size, typename Alphabet, typename FileFormat = bliss::io::FASTQ, typename KmerType = bliss::common::Kmer<Kmer_Size, Alphabet> >
    class KmerPositionIndex : public KmerIndex<bliss::index::distributed_multimap<KmerType,
        bliss::common::ShortSequenceKmerId,
        bliss::io::CommunicationLayer<true>,
    bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
    bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
    bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true> >,
    FileFormat, void>
    {
      public:
        /// DEFINE kmer index type, also used for reverse complement
        //using KmerType = bliss::common::Kmer<Kmer_Size, Alphabet>;

        /// DEFINE the index storage type.  using an infix of kmer to map to threads,
        /// since prefix is used for distributing to processors.
        using MapType = bliss::index::distributed_multimap<KmerType,
            bliss::common::ShortSequenceKmerId,
            bliss::io::CommunicationLayer<true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true> >;

        /// DEFINE kmer index value type (std::pair)
        using ValueType = typename MapType::value_type;

        /// define position/id type for Kmers
        using IdType = typename ValueType::second_type;

        /// DEFINE kmer index type
        using BaseIndexType = KmerIndex<MapType, FileFormat, void>;

        // once have ParserType, then get the sequence type and sequence id type
        using SeqType = typename BaseIndexType::SeqType;

        //==========below are to be redefined for each index type
        ///////////// INDEX TYPES
        /// converter from ascii to alphabet values
        using BaseCharIterator = bliss::iterator::transform_iterator<typename SeqType::IteratorType, bliss::common::ASCII2<Alphabet> >;

        /// kmer generation iterator
        typedef bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>             KmerIterType;
      protected:

        /// kmer position iterator type
        using IdIterType = bliss::iterator::SequenceIdIterator<IdType>;

        /// combine kmer iterator and position iterator to create an index iterator type.
        using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIterType, IdIterType>;

      public:
        /**
         * @brief 		initializes the position index
         * @param comm
         * @param comm_size
         * @param callbackFunction  for query result handling.
         * @param num_threads
         *
         */
        KmerPositionIndex(MPI_Comm _comm, int _comm_size,
                          const std::function<void(std::pair<KmerType, IdType>*, std::size_t)> & callbackFunction,
                          int num_threads = 1)  :
                            BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
      {
        	this->index.init();
      };
        /// default destructor
        virtual ~KmerPositionIndex() {};


        /**
		 *  @brief 	default Position Index Lookup Result callback function
		 */
        static void defaultReceiveAnswerCallback(std::pair<KmerType, IdType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
        {
          std::chrono::high_resolution_clock::time_point t1, t2;
          std::chrono::duration<double> time_span;

          t1 = std::chrono::high_resolution_clock::now();

          size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
          KmerType key;
          IdType val;

#pragma omp for
			  for (size_t i = 0; i < answer_count; ++i) {

				std::tie(key, val) = answers[i];

				++entries;
				if ((entries % 1000000) == 0)
					INFOF("position result:  %s <=> [%d %lu %lu]", key.toString().c_str(), val.get_file_id(), val.get_id(), val.get_pos());
			  }
          }

          total_entries += entries;

          t2 = std::chrono::high_resolution_clock::now();
            time_span =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    t2 - t1);

          time += time_span.count();
        }


      protected:


        /**
         * @brief processes a single FASTQ read or sequence into index, and insert
         * @param read
         * @param index
         */
        virtual void buildForSequence(SeqType &read, MapType& index) {

          if (read.seq_begin == read.seq_end) return;

          //== set up the kmer generating iterators.
          KmerIterType start(BaseCharIterator(read.seq_begin, bliss::common::ASCII2<Alphabet>()), true);
          KmerIterType end(BaseCharIterator(read.seq_end, bliss::common::ASCII2<Alphabet>()), false);

          //== set up the position iterators
          IdIterType id_start(read.id);
          IdIterType id_end(read.id);

          // ==== set up the zip iterators
          KmerIndexIterType index_start(start, id_start);
          KmerIndexIterType index_end(end, id_end);

          for (; index_start != index_end; ++index_start)
          {
            index.insert(*index_start);  // right side either RVO assignment (if iterator/functor created the object) or copy (if returning a cached object)
            // then copy assign
          }
        }


    };


    /**
     * @class    bliss::index::KmerPositionAndQualityIndex
     * @brief    Index for Kmer positions in source sequence and quality scores
     * @details  extends from base KmerIndex class.
     *			 positions are in file offsets to the beginning of sequence, then offset within the read.
     *			 quality score is concatenation of base quality scores.
     */
    template<unsigned int Kmer_Size, typename Alphabet, typename FileFormat = bliss::io::FASTQ, typename KmerType = bliss::common::Kmer<Kmer_Size, Alphabet> >
    class KmerPositionAndQualityIndex
        : public KmerIndex<
            bliss::index::distributed_multimap<
              KmerType,
              std::pair<bliss::common::ShortSequenceKmerId, float>,
              bliss::io::CommunicationLayer<true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>
            >,
            FileFormat,
            float>
    {
      public:
        /// DEFINE kmer index type, also used for reverse complement
        //using KmerType = bliss::common::Kmer<Kmer_Size, Alphabet>;

        /// define the index storage type.  using an infix of kmer to map to threads
        /// since prefix is used for distributing to processors.
        using MapType = bliss::index::distributed_multimap<KmerType,
            std::pair<bliss::common::ShortSequenceKmerId, float>,
            bliss::io::CommunicationLayer<true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true> >;

        /// DEFINE kmer index value type (std::pair)
        using ValueType = typename MapType::value_type;

        /// define position/id type for Kmers
        using IdType = typename ValueType::second_type::first_type;
        /// define position/id type for Kmers
        using QualityType = typename ValueType::second_type::second_type;

        /// DEFINE kmer index type
        using BaseIndexType = KmerIndex<MapType, FileFormat, float>;

        // once have ParserType, then get the sequence type and sequence id type
        using SeqType = typename BaseIndexType::SeqType;

        //==========below are to be redefined for each index type
        ///////////// INDEX TYPES
        /// converter from ascii to alphabet values
        using BaseCharIterator = bliss::iterator::transform_iterator<typename SeqType::IteratorType, bliss::common::ASCII2<Alphabet> >;

        /// kmer generation iterator
        typedef bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>             KmerIterType;

      protected:
        /// kmer position iterator type
        using IdIterType = bliss::iterator::SequenceIdIterator<IdType>;

        /// kmer quality score iterator type
        using QualIterType = bliss::index::QualityScoreGenerationIterator<typename SeqType::IteratorType, Kmer_Size, bliss::index::Illumina18QualityScoreCodec<QualityType> >;

        /// iterator to pair position and quality score iterators
        using KmerInfoIterType = bliss::iterator::ZipIterator<IdIterType, QualIterType>;

        /// the index value type.
        using KmerInfoType = typename ValueType::second_type;

        /// combine kmer iterator and position iterator to create an index iterator type.
        using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIterType, KmerInfoIterType>;


      public:
        /**
         * initializes the position + quality score index
         * @param comm
         * @param comm_size
         * @param callbackFunction  for query result handling.
         * @param num_threads
         *
         */
        KmerPositionAndQualityIndex(MPI_Comm _comm, int _comm_size,
                                    const std::function<void(std::pair<KmerType, KmerInfoType>*, std::size_t)>& callbackFunction,
                                    int num_threads = 1) :
                                      BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
      {
          this->index.init();
      };
        /// default destructor
        virtual ~KmerPositionAndQualityIndex() {};


        /**
		 *  @brief 	default Position and quality score Index Lookup Result callback function
		 */
        static void defaultReceiveAnswerCallback(std::pair<KmerType, KmerInfoType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
        {
          std::chrono::high_resolution_clock::time_point t1, t2;
            std::chrono::duration<double> time_span;
            t1 = std::chrono::high_resolution_clock::now();


          size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
              KmerType key;
              KmerInfoType val;

#pragma omp for
          for (size_t i = 0; i < answer_count; ++i) {

              std::tie(key, val) = answers[i];

            ++entries;
            if ((entries % 1000000) == 0)
            	INFOF("position + quality result: %s <=> [%d %lu %lu] %f", key.toString().c_str(), val.first.get_file_id(), val.first.get_id(), val.first.get_pos(), val.second);
          }
          }
          total_entries += entries;

          t2 = std::chrono::high_resolution_clock::now();
            time_span =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    t2 - t1);

          time += time_span.count();
        }


      protected:

        /**
         * @brief processes a single FASTQ read or sequence into index, and insert (atomic)
         * @param read
         * @param index
         */
        virtual void buildForSequence(SeqType &read, MapType& index) {

          if (read.seq_begin == read.seq_end || read.qual_begin == read.qual_end) return;

          //== set up the kmer generating iterators.
          KmerIterType start(BaseCharIterator(read.seq_begin, bliss::common::ASCII2<Alphabet>()), true);
          KmerIterType end(BaseCharIterator(read.seq_end, bliss::common::ASCII2<Alphabet>()), false);

          //== set up the position iterators
          IdIterType id_start(read.id);
          IdIterType id_end(read.id);

          // set up the quality iterator
          QualIterType qual_start(read.qual_begin);
          QualIterType qual_end(read.qual_end);

          KmerInfoIterType info_start(id_start, qual_start);
          KmerInfoIterType info_end(id_end, qual_end);

          // ==== set up the zip iterators
          KmerIndexIterType index_start(start, info_start);
          KmerIndexIterType index_end(end, info_end);

          for (; index_start != index_end; ++index_start)
          {
            index.insert(*index_start);  // right side either RVO assignment (if iterator/functor created the object) or copy (if returning a cached object)
            // then copy assign
          }
        }


    };


    namespace retired {  // versions of kmer index that will be retired soon

    /**
     * @class    bliss::index::KmerCountIndexOld
     * @brief    Index for counting Kmers.
     * @details  extends from base KmerIndex class.
     *			for counting kmers.
     * @note	   build kmolecule first with custom kmer iterator.  will be replaced.
     */
      template<unsigned int Kmer_Size, typename Alphabet, typename FileFormat = bliss::io::FASTQ, typename KmerType = bliss::common::Kmer<Kmer_Size, Alphabet> >
      class KmerCountIndexOld : public KmerIndex<bliss::index::distributed_counting_map<KmerType,
      bliss::io::CommunicationLayer<true>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true> >,
      FileFormat, void>
      {

        public:
    	  /// DEFINE kmer index key type, also used for reverse complement
          //using KmerType = bliss::common::Kmer<Kmer_Size, Alphabet>;

          /// DEFINE the index storage type.  using an infix of kmer to map to threads,
		  /// since prefix is used for distributing to processors.
          using MapType = bliss::index::distributed_counting_map<KmerType,
              bliss::io::CommunicationLayer<true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true> >;

          /// DEFINE kmer index value type (std::pair)
          using ValueType = typename MapType::value_type;

          /// define count type for Kmers
          using CountType = typename ValueType::second_type;

          /// DEFINE kmer index type
          using BaseIndexType = KmerIndex<MapType, FileFormat, void>;

          // once have ParserType, then get the sequence type and sequence id type
          using SeqType = typename BaseIndexType::SeqType;

          //==========below are to be redefined for each index type
          ///////////// INDEX TYPES
          /// define kmer iterator, and the functors for it.
          typedef bliss::index::generate_kmer_simple<SeqType, Alphabet, KmerType>    KmoleculeOpType;
          /// trasforms from ASCII to kmoleculess
          typedef bliss::iterator::buffered_transform_iterator<KmoleculeOpType, typename KmoleculeOpType::BaseIterType> KmoleculeIterType;
          /// transform from kmolecule to kmer
          typedef bliss::iterator::transform_iterator<KmoleculeIterType, bliss::utils::KmoleculeToCanonicalKmerFunctor<KmerType> >       KmerIterType;

        protected:
          /// kmer iterator is the index element iterator type here.
          typedef KmerIterType KmerIndexIterType;

        public:
          /**
           * @brief initializes the count index
           * @param comm
           * @param comm_size
           * @param callbackFunction  for query result handling.
           * @param num_threads
           *
           */
          KmerCountIndexOld(MPI_Comm _comm, int _comm_size,
                            const std::function<void(std::pair<KmerType, bliss::index::count_t>*, std::size_t)> &callbackFunction,
                             int num_threads = 1) :
                               BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
        {
            this->index.init();
        };
          /// default destructor
          virtual ~KmerCountIndexOld() {};

          /**
		   *  @brief 	default Count Index Lookup Result callback function
		   */
          static void defaultReceiveAnswerCallback(std::pair<KmerType, CountType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
          {
            std::chrono::high_resolution_clock::time_point t1, t2;
              std::chrono::duration<double> time_span;

              t1 = std::chrono::high_resolution_clock::now();

            size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
                KmerType key;
                CountType val;

#pragma omp for
            for (size_t i = 0; i < answer_count; ++i) {

                std::tie(key, val) = answers[i];

              ++entries;
              if ((entries % 1000000) == 0) INFOF("count result:  %s <=> %d", key.toString().c_str(), val);
            }
          }
            total_entries += entries;

            t2 = std::chrono::high_resolution_clock::now();
              time_span =
                  std::chrono::duration_cast<std::chrono::duration<double>>(
                      t2 - t1);

            time += time_span.count();
          };

        protected:

          /**
           * @brief overriden method to process sequence into kmers and insert
           * @param read
           * @param index
           */
          virtual void buildForSequence(SeqType &read, MapType& index) {

            if (read.seq_begin == read.seq_end) return;

            //== set up the kmer generating iterators.
            KmoleculeOpType kmer_op;
            bliss::utils::KmoleculeToCanonicalKmerFunctor<KmerType> transform;
            KmerIndexIterType start(KmoleculeIterType(read.seq_begin, kmer_op), transform);
            KmerIndexIterType end(KmoleculeIterType(read.seq_end, kmer_op), transform);


            // NOTE: need to call *start to actually evaluate.  question is whether ++ should be doing computation.
            unsigned int i = 0;
            for (; i < (Kmer_Size - 1) && start != end; ++start, ++i);  // compute but discard the first K - 1.

            for (; start != end; ++start, ++i)
            {
              index.insert(*start);  // right side either RVO assignment (if iterator/functor created the object) or copy (if returning a cached object)
              // then copy assign
            }
          }

      };





      /**
       * @class    bliss::index::KmerPositionIndex
       * @brief    Index for Kmer positions in source sequence.
       * @details  extends from base KmerIndex class.
       *			 positions are in file offsets to the beginning of sequence, then offset within the read.
       * @note	   build kmolecule first with custom kmer iterator.  will be replaced.
       */
      template<unsigned int Kmer_Size, typename Alphabet, typename FileFormat = bliss::io::FASTQ, typename KmerType = bliss::common::Kmer<Kmer_Size, Alphabet> >
      class KmerPositionIndexOld : public KmerIndex<bliss::index::distributed_multimap<KmerType,
      bliss::common::ShortSequenceKmerId,
      bliss::io::CommunicationLayer<true>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true> >,
      FileFormat, void>
      {

        public:
    	  /// DEFINE kmer index type, also used for reverse complement
         // using KmerType = bliss::common::Kmer<Kmer_Size, Alphabet>;

          /// DEFINE the index storage type.  using an infix of kmer to map to threads,
                 /// since prefix is used for distributing to processors.
          using MapType = bliss::index::distributed_multimap<KmerType,
              bliss::common::ShortSequenceKmerId,
              bliss::io::CommunicationLayer<true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true> >;

          /// DEFINE kmer index value type (std::pair)
          using ValueType = typename MapType::value_type;

          /// define position/id type for Kmers
          using IdType = typename ValueType::second_type;

          /// DEFINE kmer index type
          using BaseIndexType = KmerIndex<MapType, FileFormat, void>;

          // once have ParserType, then get the sequence type and sequence id type
          using SeqType = typename BaseIndexType::SeqType;

          //==========below are to be redefined for each index type
          ///////////// INDEX TYPES
          /// define kmer iterator, and the functors for it.
          typedef bliss::index::generate_kmer_simple<SeqType, Alphabet, KmerType>  KmoleculeOpType;
          /// trasforms from ASCII to kmoleculess
          typedef bliss::iterator::buffered_transform_iterator<KmoleculeOpType, typename KmoleculeOpType::BaseIterType> KmoleculeIterType;
          /// transform from kmolecule to kmer
          typedef bliss::iterator::transform_iterator<KmoleculeIterType, bliss::utils::KmoleculeToCanonicalKmerFunctor<KmerType> >       KmerIterType;
        protected:

          /// kmer position iterator type
          using IdIterType = bliss::iterator::SequenceIdIterator<IdType>;

          /// combine kmer iterator and position iterator to create an index iterator type.
          using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIterType, IdIterType>;

        public:
          /**
           * @brief 		initializes the position index
           * @param comm
           * @param comm_size
           * @param callbackFunction  for query result handling.
           * @param num_threads
           *
           */
          KmerPositionIndexOld(MPI_Comm _comm, int _comm_size,
                               const std::function<void(std::pair<KmerType, IdType>*, std::size_t)> & callbackFunction,
                               int num_threads = 1)  :
                                 BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
        {

            this->index.init();
        };
          /// default destructor
          virtual ~KmerPositionIndexOld() {};

          /**
  		 *  @brief 	default Position Index Lookup Result callback function
  		 */
          static void defaultReceiveAnswerCallback(std::pair<KmerType, IdType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
          {
            std::chrono::high_resolution_clock::time_point t1, t2;
              std::chrono::duration<double> time_span;

              t1 = std::chrono::high_resolution_clock::now();

            size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
                KmerType key;
                IdType val;

#pragma omp for
            for (size_t i = 0; i < answer_count; ++i) {
            	std::tie(key,val) = answers[i];
              ++entries;
              if ((entries % 1000000) == 0)
            	  INFOF("position result:  %s <=> [%d %lu %lu]", key.toString().c_str(), val.get_file_id(), val.get_id(), val.get_pos());
            }
          }
            total_entries += entries;

            t2 = std::chrono::high_resolution_clock::now();
              time_span =
                  std::chrono::duration_cast<std::chrono::duration<double>>(
                      t2 - t1);

            time += time_span.count();
          }


        protected:


          /**
           * @brief overriden method to process sequence into kmers and insert
           * @param read
           * @param index
           */
          virtual void buildForSequence(SeqType &read, MapType& index) {

            if (read.seq_begin == read.seq_end) return;

            //== set up the kmer generating iterators.
            KmoleculeOpType kmer_op;
            bliss::utils::KmoleculeToCanonicalKmerFunctor<KmerType> transform;
            KmerIterType start(KmoleculeIterType(read.seq_begin, kmer_op), transform);
            KmerIterType end(KmoleculeIterType(read.seq_end, kmer_op), transform);

            //== set up the position iterators
            IdIterType id_start(read.id);
            IdIterType id_end(read.id);

            // ==== set up the zip iterators
            KmerIndexIterType index_start(start, id_start);
            KmerIndexIterType index_end(end, id_end);


            unsigned int i = 0;
            for (; i < (Kmer_Size - 1) && index_start != index_end; ++index_start, ++i);  // compute but discard the first K - 1.

            for (; index_start != index_end; ++index_start, ++i)
            {
              index.insert(*index_start);  // right side either RVO assignment (if iterator/functor created the object) or copy (if returning a cached object)
              // then copy assign
            }
          }


      };


      /**
       * @class    bliss::index::KmerPositionAndQualityIndex
       * @brief    Index for Kmer positions in source sequence and quality scores
       * @details  extends from base KmerIndex class.
       *			 positions are in file offsets to the beginning of sequence, then offset within the read.
       *			 quality score is concatenation of base quality scores.
       *
       * @note	   build kmolecule first with custom kmer iterator.  will be replaced.
       */
      template<unsigned int Kmer_Size, typename Alphabet, typename FileFormat = bliss::io::FASTQ, typename KmerType = bliss::common::Kmer<Kmer_Size, Alphabet> >
      class KmerPositionAndQualityIndexOld : public KmerIndex<bliss::index::distributed_multimap<KmerType,
      std::pair<bliss::common::ShortSequenceKmerId, float>,
      bliss::io::CommunicationLayer<true>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true>,
      bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true> >,
      FileFormat, float>
      {

        public:
    	  /// DEFINE kmer index type, also used for reverse complement
          //using KmerType = bliss::common::Kmer<Kmer_Size, Alphabet>;

          /// define the index storage type.  using an infix of kmer to map to threads
          /// since prefix is used for distributing to processors.
          using MapType = bliss::index::distributed_multimap<KmerType,
              std::pair<bliss::common::ShortSequenceKmerId, float>,
              bliss::io::CommunicationLayer<true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true>,
              bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::IdentityCombiner, true> >;

          /// DEFINE kmer index value type (std::pair)
          using ValueType = typename MapType::value_type;

          /// define position/id type for Kmers
          using IdType = typename ValueType::second_type::first_type;
          /// define position/id type for Kmers
          using QualityType = typename ValueType::second_type::second_type;

          /// DEFINE kmer index type
          using BaseIndexType = KmerIndex<MapType, FileFormat, float>;

          // once have ParserType, then get the sequence type and sequence id type
          using SeqType = typename BaseIndexType::SeqType;

          //==========below are to be redefined for each index type

          ///////////// INDEX TYPES
          /// define kmer iterator, and the functors for it.
          typedef bliss::index::generate_kmer_simple<SeqType, Alphabet, KmerType> KmoleculeOpType;
          /// trasforms from ASCII to kmoleculess
          typedef bliss::iterator::buffered_transform_iterator<KmoleculeOpType, typename KmoleculeOpType::BaseIterType> KmoleculeIterType;
          /// transform from kmolecule to kmer
          typedef bliss::iterator::transform_iterator<KmoleculeIterType, bliss::utils::KmoleculeToCanonicalKmerFunctor<KmerType> >       KmerIterType;


          /// kmer position iterator type
          using IdIterType = bliss::iterator::SequenceIdIterator<IdType>;

          /// kmer quality score iterator type
          using QualIterType = bliss::index::QualityScoreGenerationIterator<typename SeqType::IteratorType, Kmer_Size, bliss::index::Illumina18QualityScoreCodec<float> >;

          /// iterator to pair position and quality score iterators
          using KmerInfoIterType = bliss::iterator::ZipIterator<IdIterType, QualIterType>;

          /// the index value type.
          using KmerInfoType = typename ValueType::second_type;

          /// combine kmer iterator and position iterator to create an index iterator type.
          using KmerIndexIterType = bliss::iterator::ZipIterator<KmerIterType, KmerInfoIterType>;


        public:
          /**
           * initializes the position + quality score index
           * @param comm
           * @param comm_size
           * @param callbackFunction  for query result handling.
           * @param num_threads
           *
           */
          KmerPositionAndQualityIndexOld(MPI_Comm _comm, int _comm_size,
                                         const std::function<void(std::pair<KmerType, KmerInfoType>*, std::size_t)>& callbackFunction,
                                         int num_threads = 1) :
                                           BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
        {
            this->index.init();
        };
          /// default destructor
          virtual ~KmerPositionAndQualityIndexOld() {};

          /**
  		 *  @brief 	default Position and quality score Index Lookup Result callback function
  		 */
          static void defaultReceiveAnswerCallback(std::pair<KmerType, KmerInfoType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
          {
            std::chrono::high_resolution_clock::time_point t1, t2;
              std::chrono::duration<double> time_span;

              t1 = std::chrono::high_resolution_clock::now();

            size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
                KmerType key;
                KmerInfoType val;

#pragma omp for
            for (size_t i = 0; i < answer_count; ++i) {
                std::tie(key, val) = answers[i];

              ++entries;
              if ((entries % 1000000) == 0)
            	  INFOF("position + quality result: %s <=> [%d %lu %lu] %f", key.toString().c_str(), val.first.get_file_id(), val.first.get_id(), val.first.get_pos(), val.second);
            }
          }
            total_entries += entries;

            t2 = std::chrono::high_resolution_clock::now();
              time_span =
                  std::chrono::duration_cast<std::chrono::duration<double>>(
                      t2 - t1);

            time += time_span.count();
          }

        protected:


          /**
           * @brief processes a single FASTQ read or sequence into index, and insert (atomic)
           * @param read
           * @param index
           */
          virtual void buildForSequence(SeqType &read, MapType& index) {

            if (read.seq_begin == read.seq_end || read.qual_begin == read.qual_end) return;

            //== set up the kmer generating iterators.
            KmoleculeOpType kmer_op;
            bliss::utils::KmoleculeToCanonicalKmerFunctor<KmerType> transform;
            KmerIterType start(KmoleculeIterType(read.seq_begin, kmer_op), transform);
            KmerIterType end(KmoleculeIterType(read.seq_end, kmer_op), transform);

            //== set up the position iterators
            IdIterType id_start(read.id);
            IdIterType id_end(read.id);

            // set up the quality iterator
            QualIterType qual_start(read.qual_begin);
            QualIterType qual_end(read.qual_end);

            KmerInfoIterType info_start(id_start, qual_start);
            KmerInfoIterType info_end(id_end, qual_end);


            // ==== set up the zip iterators
            KmerIndexIterType index_start(start, info_start);
            KmerIndexIterType index_end(end, info_end);

            // NOTE: need to call *start to actually evaluate.  question is whether ++ should be doing computation.
            unsigned int i = 0;
            for (; i < (Kmer_Size - 1) && index_start != index_end; ++index_start, ++i);  // compute but discard the first K - 1.

            for (; index_start != index_end; ++index_start, ++i)
            {
              index.insert(*index_start);  // right side either RVO assignment (if iterator/functor created the object) or copy (if returning a cached object)
              // then copy assign
            }
          }


      };


    } /* namespace retired */
#if 0
    /**
     * @class    bliss::index::KmerIndex
     * @brief    base type for All Kmer Index
     * @details  this is a base classe that provides basic file reading and sequence iteration
     * 			capabilities.  Subclasses only need to override the buildSequence or buildL2Block
     * 			methods to provide custom Kmer indexing such as Count index, position index,
     * 			and position + quality index.
     *
     */
    template<typename MapType>
    class KmerIndex<MapType, bliss::io::FASTA, void> {
      public:
        //==================== COMMON TYPES

        /// DEFINE file loader.  this only provides the L1 and L2 blocks, not reads.
        //using FileLoaderType = bliss::io::FASTQLoader<CharType, std::is_void<Quality>::value, false, true>; // raw data type :  use CharType
        //Using block partioner for L1 level decomposition of the input file

        //Define file loader
        typedef bliss::io::FileLoader<CharType, 0, bliss::io::BaseFileParser, false, true> FileLoaderType;

        // from FileLoader type, get the block iter type and range type
        using FileBlockIterType = typename FileLoaderType::L2BlockType::iterator;

        /// DEFINE Kmer type, extracted from LocalContainer's key type
        using KmerType = typename MapType::value_type::first_type;
        /// DEFINE Value type, extracted from LocalContainer's value type
        using ValueType = typename MapType::value_type::second_type;

        /// index instance to store data
        MapType index;

        /// MPI communicator used by index and fileloader.
        MPI_Comm comm;

        /// MPI rank within the communicator
        int rank;

        /// size of communicator
        int commSize;

      protected:

        //FASTA preprocessor class 
        typedef bliss::io::FASTALoader<FileBlockIterType, KmerType> FASTALoaderType;

        //Iterator for FASTA file format
        using ParserType =  bliss::io::FASTAParser<FileBlockIterType, KmerType>; 

        //Type of vector returned by FASTA Loader
        using FASTAHeaderVectorType = typename FASTALoaderType::vectorType;

        /// DEFINE the iterator for iterating over kmers in FASTA file.
        using SeqIterType = typename ParserType::corrected_offset_Kmer_zip;


      public:
        /**
         * @brief initializes the index
         * @param comm				MPI communicator
         * @param comm_size
         * @param callbackFunction  for query result handling.
         * @param num_threads
         */
        KmerIndex(MPI_Comm _comm, int _comm_size,
                  const std::function<void(std::pair<KmerType, ValueType>*, std::size_t)> &callbackFunction,
                  int num_threads = 1) :
          index(_comm, _comm_size, num_threads, 
                typename MapType::ThreadHashType(ceilLog2(num_threads * _comm_size)),
                typename MapType::ProcessHashType(ceilLog2(_comm_size))
          ),
          comm(_comm), commSize(_comm_size) {
          MPI_Comm_rank(comm, &rank);

          this->index.setLookupAnswerCallback(callbackFunction);
        };

        /// default destructor
        virtual ~KmerIndex() {
          index.finish();
        };

        /// Finalize terminates communication, thus preventing further index modification and query.
        void finalize() {
          index.finish();
        }

        /// get size of the local portion of the distributed map.  primarily for debugging
        size_t local_size() const {
          return index.local_size();
        }

        /// get size of the local portion of the distributed map.  primarily for debugging
        std::vector<size_t> local_sizes() const {
          return index.local_sizes();
        }

        /// access the local portion of the distributed map.
        MapType& getLocalIndex() {
          return index;
        }

        /**
         * @brief   build index from file using specified number of threads
         * @note    default to chunksize = system PAGE SIZE
         * @param filename
         * @param nthreads
         */
        void build(const std::string &filename, const int &nthreads) {
          build(filename, nthreads, sysconf(_SC_PAGE_SIZE));
        }

        /**
         * @brief build index from a file using the specified number of threads.
         * @param filename
         * @param nthreads
         * @param chunkSize
         */
        void build(const std::string &filename, const int &nthreads, const int chunkSize) {
          // scoped so FileLoader is deleted after.
          {
            //==== create file Loader
            FileLoaderType loader(filename, comm, nthreads, chunkSize);  // this handle is alive through the entire building process.

            // modifying the local index directly here causes a thread safety issue, since callback thread is already running.
            // index reserve internally sends a message to itself.
            index.reserve((loader.getFileRange().size() + commSize - 1) / commSize);

            //====  now process the file, one L1 block (block partition by MPI Rank) at a time
            typename FileLoaderType::L1BlockType partition = loader.getNextL1Block();

            //====  Run fasta preprocessor 
            FASTAHeaderVectorType vectorReturned;

            FASTALoaderType obj(comm);
            obj.countSequenceStarts(partition.begin(), loader.getFileRange() , partition.getRange() , vectorReturned);

            //Assuming block-level decomposition at L1, we need to build the L1 block only once
            buildForL1Block(loader, index, rank, nthreads, chunkSize, vectorReturned);

            // make index communicate all pending messages to other processes.
            index.flush();
          }  // scope to ensure file loader is destroyed.
        }

        //============== Query API.  sendQuery allows overlapped IO and computation

        /**
         * @brief    Add a new query to buffer for asynchronous communication.
         * @details  when the buffer is full, then the query is executed.  result is handled via callback
         * @param 	 kmer
         */
        void sendQuery(const KmerType & kmer) {
          index.asyncLookup(kmer);
        }

        /**
         * @brief flush all pending the queries to remote nodes.
         * @return
         */
        void flushQuery() {
          index.flush();
        }

        /**
         * @brief send a set of queries in a collective/blocking manner immediately.
         * @param kmers
         * @return
         */
        void query(const std::vector<KmerType>& kmers) {
          // TODO: for now, use the async api
          for (auto kmer : kmers) {
            index.asyncLookup(kmer);
          }
          index.flush();
          // TODO: later, use MPI collective calls.  (require that a single thread performs this.
        }

        // TODO: filter API.

        // TODO: histogram API


      protected:
        /**
         * @brief processes a single L1Block to build the index.
         * @details  parses a L1Block as L2Blocks, which contain sequences (e.g. for FASTQ, reads)
         *           and then call buildForSequence to process each sequence.
         *
         * @note  fastq specific. fasta would not have "SeqIterType"
         *
         * @param loader
         * @param index
         * @param rank
         * @param nthreads
         * @param chunkSize
         */
        void buildForL1Block(FileLoaderType &loader, MapType &index, const int& rank,
                             const int& nthreads, const int& chunkSize,
                             const FASTAHeaderVectorType& FASTA_HeaderVector)
        {
          // get L2Block from L1Block, spread work to all threads.
#pragma omp parallel num_threads(nthreads) shared(loader, nthreads, index, rank) OMP_SHARE_DEFAULT
          {
            int tid = 0;
#ifdef USE_OPENMP
            tid = omp_get_thread_num();
#endif
            //== local variables for loop
            typename FileLoaderType::L2BlockType chunk;

            //== process L2Blocks in a loop.  loader.getNextL2Block is atomic.  the work is shared by all threads.
            chunk = loader.getNextL2Block(tid);

            //== instantiate a local parser in each thread
            //ParserType parser(chunk.begin(), chunk.end(), chunk.getRange(), loader.getFileRange(), FASTA_HeaderVector);

            while (chunk.getRange().size() > 0)   // range size = 0 when finished traversal
            {
              ParserType parser(chunk.begin(), chunk.end(), chunk.getRange(), loader.getFileRange(), FASTA_HeaderVector);
              buildForL2Block(chunk, parser, index);

              //== get read for next loop iteration.
              chunk = loader.getNextL2Block(tid);
            }
          }  // compute threads parallel

        }

        /**
         * @brief processes a single L2Block to build the index.
         * @details  parses a L2Block into short sequences (e.g. for FASTQ, reads)
         *           and then call buildForSequence to process each sequence.
         *
         * @note  fastq specific. fasta qould not have "SeqIterType"
         * @param chunk
         * @param parser
         * @param index
         */
        virtual void buildForL2Block(typename FileLoaderType::L2BlockType &chunk, ParserType& parser, MapType& index) {
          if (chunk.getRange().size() == 0) return;

          //== loop over the reads
          for (auto iter=parser.begin(); iter != parser.end(); iter++)
          {
            // then compute and store into index (this will generate kmers and insert into index)
            buildForSequence(iter, index);
          }
        }

        /**
         * @brief processes a single FASTQ read or sequence into index, and insert (atomic)
         * @param read
         * @param index
         */
        virtual void buildForSequence(SeqIterType &iter, MapType& index) = 0;

    };

    /**
     * @class    bliss::index::KmerCountIndex
     * @brief    Index for counting Kmers.
     * @details  extends from base KmerIndex class.
     *			for counting kmers.
     */
    template<unsigned int Kmer_Size, typename Alphabet, typename WordType, template <unsigned int, typename, typename> class KmerT>
    class KmerCountIndex< KmerT<Kmer_Size, Alphabet, WordType>, bliss::io::FASTA>
    : public KmerIndex<
       bliss::index::distributed_counting_map<
       KmerT<Kmer_Size, Alphabet, WordType>,
       bliss::io::CommunicationLayer<true>,
       bliss::hash::kmer::hash<KmerT<Kmer_Size, Alphabet, WordType>, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
       bliss::hash::kmer::hash<KmerT<Kmer_Size, Alphabet, WordType>, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
       bliss::hash::kmer::hash<KmerT<Kmer_Size, Alphabet, WordType>, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>
    >,
    bliss::io::FASTA, void>
    {
      public:
        /// DEFINE kmer index key type, also used for reverse complement
        using KmerType = KmerT<Kmer_Size, Alphabet, WordType>;

        /// DEFINE the index storage type.  using an infix of kmer to map to threads,
        /// since prefix is used for distributing to processors.
        using MapType = bliss::index::distributed_counting_map<KmerType,
            bliss::io::CommunicationLayer<true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true>,
            bliss::hash::kmer::hash<KmerType, bliss::hash::kmer::detail::farm::hash, bliss::hash::kmer::LexicographicLessCombiner, true> >;

        /// DEFINE kmer index value type (std::pair)
        using ValueType = typename MapType::value_type;

        /// define count type for Kmers
        using CountType = typename ValueType::second_type;

        /// DEFINE kmer index type
        using BaseIndexType = KmerIndex<MapType, bliss::io::FASTA, void>;

        // once have ParserType, then get the sequence type and sequence id type
        using SeqIterType = typename BaseIndexType::SeqIterType;

        //==========below are to be redefined for each index type
        ///////////// INDEX TYPES
        /// converter from ascii to alphabet values
        //using BaseCharIterator = bliss::iterator::transform_iterator<typename SeqType::IteratorType, bliss::common::ASCII2<Alphabet> >;

        /// kmer generation iterator
        //typedef bliss::common::KmerGenerationIterator<BaseCharIterator, KmerType>             KmerIterType;
      protected:

        /// kmer iterator is the index element iterator type here.
        //using KmerIndexIterType = KmerIterType;

      public:
        /**
         * @brief 		initializes the count index
         * @param comm
         * @param comm_size
         * @param callbackFunction  for query result handling.
         * @param num_threads
         */
        KmerCountIndex(MPI_Comm _comm, int _comm_size,
            const std::function<void(std::pair<KmerType, bliss::index::count_t>*, std::size_t)> &callbackFunction,
            int num_threads = 1) :
          BaseIndexType(_comm, _comm_size, callbackFunction, num_threads)
        {
          this->index.init();
        };

        /// default destructor
        virtual ~KmerCountIndex() {};


        /**
         *  @brief 	default Count Index Lookup Result callback function
         */
        static void defaultReceiveAnswerCallback(std::pair<KmerType, CountType>* answers, std::size_t answer_count, int nthreads, size_t& total_entries, double &time)
        {
          std::chrono::high_resolution_clock::time_point t1, t2;
            std::chrono::duration<double> time_span;

            t1 = std::chrono::high_resolution_clock::now();



          size_t entries = 0;

#pragma omp parallel num_threads(nthreads) OMP_SHARE_DEFAULT shared(answers, answer_count) reduction(+: entries)
          {
                KmerType key;
                CountType val;

#pragma omp for
          for (size_t i = 0; i < answer_count; ++i) {
            std::tie(key, val) = answers[i];

            ++entries;
            if ((entries % 1000000) == 0) INFOF("count result:  %s <=> %d", key.toString().c_str(), val);
          }
          }
          total_entries += entries;


          t2 = std::chrono::high_resolution_clock::now();
            time_span =
                std::chrono::duration_cast<std::chrono::duration<double>>(
                    t2 - t1);

          time += time_span.count();

        };



      protected:


        /**
         * @brief overriden method to process sequence into kmers and insert
         * @param read
         * @param index
         */
        virtual void buildForSequence(SeqIterType &iter, MapType& index) {

          //get kmer value from iterator
          index.insert(std::get<1>(*iter));
          //std::cout << bliss::utils::KmerUtils::toASCIIString(std::get<1>(*iter)) << "\n";
        }
    };

#endif

  } /* namespace index */
} /* namespace bliss */

#endif /* KMERINDEX_HPP_ */
