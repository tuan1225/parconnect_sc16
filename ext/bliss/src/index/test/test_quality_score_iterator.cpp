/**
 * @file    test_quality_score.cpp
 * @ingroup
 * @author  Tony Pan <tpan7@gatech.edu>
 * @brief
 * @details
 *
 * Copyright (c) 2015 Georgia Institute of Technology.  All Rights Reserved.
 *
 * TODO add License
 */


// include google test
#include <gtest/gtest.h>
//#include <boost/concept_check.hpp>

// include classes to test
#include "index/quality_score_iterator.hpp"
#include <vector>
#include <algorithm>
#include <cassert>
#include "utils/logging.h"

//// Usable AlmostEqual function
//// from http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
//// set maxUlps to be 1M or less.
//bool AlmostEqual2sComplement(float A, float B, int maxUlps)
//{
//    // Make sure maxUlps is non-negative and small enough that the
//    // default NAN won't compare as equal to anything.
//
//    assert(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
//
//    int aInt = *(int*)&A;
//
//    // Make aInt lexicographically ordered as a twos-complement int
//    if (aInt < 0)
//        aInt = 0x80000000 - aInt;
//
//    // Make bInt lexicographically ordered as a twos-complement int
//    int bInt = *(int*)&B;
//
//    if (bInt < 0)
//        bInt = 0x80000000 - bInt;
//
//    int intDiff = abs(aInt - bInt);
//
//    if (intDiff <= maxUlps)
//        return true;
//
//    return false;
//}
//
//bool AlmostEqual2sComplement(double A, double B, int maxUlps)
//{
//    // Make sure maxUlps is non-negative and small enough that the
//    // default NAN won't compare as equal to anything.
//
//    assert(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
//
//    long int aInt = *(long int*)&A;
//
//    // Make aInt lexicographically ordered as a twos-complement int
//    if (aInt < 0)
//        aInt = 0x8000000000000000 - aInt;
//
//    // Make bInt lexicographically ordered as a twos-complement int
//    long int bInt = *(long int*)&B;
//
//    if (bInt < 0)
//        bInt = 0x8000000000000000 - bInt;
//
//    long int intDiff = abs(aInt - bInt);
//
//    if (intDiff <= maxUlps)
//        return true;
//
//    return false;
//}


template <typename T>
typename std::enable_if<std::is_same<T, double>::value, bool>::type compare_vectors(const std::vector<T> & first, const std::vector<T> & second) {
  if (first == second) {
    return true;   // if same object
  }
  if (first.size() != second.size()) {
    BL_ERROR( "size not the same" );
    return false;  // if size differ
  }

  if (std::equal(first.cbegin(), first.cend(), second.cbegin(), [](T x, T y) {
    return fabs(x - y) < 1.0e-14; }
  )) return true;
  else {
    BL_ERROR( "Vectors not the same" );
    for (size_t i = 0; i < first.size(); ++i) {
      std::cout<< (first[i] - second[i]) << ",";
    }
    std::cout << std::endl;
    return false;
  }
}

template <typename T>
typename std::enable_if<std::is_same<T, float>::value, bool>::type compare_vectors(const std::vector<T> & first, const std::vector<T> & second) {
  if (first == second) {
    return true;   // if same object
  }
  if (first.size() != second.size()) {
    BL_ERROR( "size not the same" );
    return false;  // if size differ
  }

  if (std::equal(first.cbegin(), first.cend(), second.cbegin(), [](T x, T y) {
    return fabs(x - y) < 1.0e-6; }
  )) return true;
  else {
    BL_ERROR( "Vectors not the same" );
    for (size_t i = 0; i < first.size(); ++i) {
      std::cout << (first[i] - second[i]) << ",";
    }
    std::cout << std::endl;
    return false;
  }
}

template <typename T>
typename std::enable_if<!std::is_floating_point<T>::value, bool>::type compare_vectors(const std::vector<T> & first, const std::vector<T> & second) {
  if (first == second) return true;   // if same object
  if (first.size() != second.size()) return false;  // if size differ

  if (std::equal(first.cbegin(), first.cend(), second.cbegin())) return true;
  else {

    BL_ERROR( "Vectors not the same" );
    for (int i = 0; i < first.size(); ++i) {
      std::cout << (first[i] - second[i] ) << ",";
    }
    std::cout << std::endl;
    return false;
  }
}


// templated test function
template<typename CODEC, unsigned int K>
void iter_decode(const std::vector<unsigned char>& data, // quality score value
                           std::vector<typename CODEC::value_type>& output) {

  using QualIter = bliss::index::QualityScoreGenerationIterator<std::vector<unsigned char>::const_iterator,
        K, CODEC>;

  QualIter it(data.begin(), true);
  QualIter end(data.end(), false);

  output.clear();

  for (; it != end; ++it) {
    output.push_back(*it);
  }

}


// templated test function
template<typename CODEC, unsigned int K>
void codec_decode(const std::vector<unsigned char> & data, // quality score value
                           std::vector<typename CODEC::value_type>& output) {

  CODEC decoder;

//  BL_INFO( "decoder LUT SIZE: " << (int)CODEC::size );
//  for (int i = 0; i < CODEC::size; ++i) {
//    std::cout << CODEC::lut[i] << ", ";
//  }
//  std::cout << std::endl;

  output.clear();

  using OT = typename CODEC::value_type;

  int invalid = 0;
  OT sum = 0.0;
  for (unsigned int i = 0; i < K; ++i) {
    auto tmp = decoder.decode(data[i]);

    if (tmp > CODEC::DecodeLUT[0] && tmp < CODEC::DecodeLUT[95])
      sum += tmp;
    else
      ++invalid;
  }
  if (invalid > 0) output.push_back(0);
  else output.push_back(std::exp2(sum));

  for (unsigned int i = K; i < data.size(); ++i) {
    auto tmp = decoder.decode(data[i-K]);
    if (tmp > CODEC::DecodeLUT[0] && tmp < CODEC::DecodeLUT[95])
      sum -= tmp;
    else
      --invalid;

    tmp = decoder.decode(data[i]);
    if (tmp > CODEC::DecodeLUT[0] && tmp < CODEC::DecodeLUT[95])
      sum += tmp;
    else
      ++invalid;

    if (invalid > 0) output.push_back(0);
    else output.push_back(std::exp2(sum));
  }

}




template<typename OT, unsigned char MinInput, unsigned char MaxInput, char MinScore, unsigned int K>
void direct_decode(const std::vector<unsigned char> & data,
                   std::vector<OT>& output) {

  output.clear();

  double sum = 0;
  double val = 0;
  int invalid= 0;
  for (unsigned int i = 0; i < K; ++i) {
    auto tmp = data[i] - MinInput;

    if (tmp == 0 || tmp < MinScore)
      ++invalid;
    else {
      val = std::log2(1.0L - std::exp2(static_cast<long double>(tmp) * log2(10.0L) / -10.0L));
      sum += val;
    }

  }
  if (invalid > 0) output.push_back(0);
  else output.push_back(std::exp2(sum));

  for (unsigned int i = K; i < data.size(); ++i) {
    auto tmp = data[i-K] - MinInput;

    if (tmp == 0 || tmp < MinScore)
      --invalid;
    else {
      val = std::log2(1.0L - std::exp2(static_cast<long double>(tmp) * log2(10.0L) / -10.0L));
      sum -= val;

    }

    tmp = data[i] - MinInput;
    if (tmp == 0 || tmp < MinScore)
      ++invalid;
    else {
      val = std::log2(1.0L - std::exp2(static_cast<long double>(tmp) * log2(10.0L) / -10.0L));
      sum += val;
    }
    if (invalid > 0) output.push_back(0);
    else output.push_back(std::exp2(sum));
  }

}


template <unsigned char MinInput, char MinScore>
std::vector<unsigned char> convertForGold(const std::string &strdata) {
	std::vector<unsigned char> res(strdata.begin(), strdata.end());
	for (size_t i = 0; i < res.size(); ++i) {
		if ((res[i] - MinInput) < MinScore) res[i] = MinInput + MinScore - 1;
	}
	return res;
}

template<typename OT, unsigned char MinInput, unsigned char MaxInput, char MinScore, unsigned int K>
void testQualityIterator(const std::string & strdata) {

  std::vector<unsigned char> gold = convertForGold<MinInput, MinScore>(strdata);

  using Encoder = bliss::index::QualityScoreCodec<OT, MinInput, MaxInput, MinScore>;


  // test process.
  std::vector<OT> goldDecoded;
  direct_decode<OT, MinInput, MaxInput, MinScore, K>(gold, goldDecoded);



  // test codec decoding.
  std::vector<OT> codecDecoded;
  codec_decode< Encoder, K >(gold, codecDecoded);
  bool same = compare_vectors<OT>(codecDecoded, goldDecoded);

  if (!same) {
    BL_ERROR( "codec decode: result not same" << std::endl );

    BL_ERROR( "GOLD decoded: size: " << goldDecoded.size());
    std::copy(goldDecoded.begin() , goldDecoded.end(), std::ostream_iterator<OT>(std::cout, ","));
    std::cout << std::endl;
    BL_ERROR( "codec decoded: size: " << codecDecoded.size());
    std::copy(codecDecoded.begin() , codecDecoded.end(), std::ostream_iterator<OT>(std::cout, ","));
    std::cout << std::endl;
  }

  EXPECT_TRUE(same);


  std::vector<OT> iterDecoded;
  iter_decode< Encoder, K >(gold, iterDecoded);
  same = compare_vectors<OT>(iterDecoded, goldDecoded);

  if (!same) {
    BL_ERROR( "iter decode: result not same" << std::endl );

    BL_ERROR( "GOLD decoded: size: " << goldDecoded.size() );
    std::copy(goldDecoded.begin() , goldDecoded.end(), std::ostream_iterator<OT>(std::cout, ","));
    std::cout << std::endl;
    BL_ERROR( "iter decoded: size: " << iterDecoded.size());
    std::copy(iterDecoded.begin() , iterDecoded.end(), std::ostream_iterator<OT>(std::cout, ","));
    std::cout << std::endl;
  }

  EXPECT_TRUE(same);


}


/**
 * Test k-mer generation with 2 bits for each character
 */
TEST(QualityScoreGenerationIteratorTest, TestIllunima18QualityScoreDouble)
{
  // test sequence: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
  std::string strdata = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

  testQualityIterator<double, 33, 126, 0, 3 >(strdata);
  testQualityIterator<double, 33, 126, 0, 7 >(strdata);
  testQualityIterator<double, 33, 126, 0, 15>(strdata);
  testQualityIterator<double, 33, 126, 0, 31>(strdata);
}




/**
 * Test k-mer generation with 2 bits for each character
 */
TEST(QualityScoreGenerationIteratorTest, TestIllunima13QualityScoreDouble)
{
  // test sequence: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
  std::string strdata = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

  testQualityIterator<double, 64, 126, 0, 3 >(strdata);
  testQualityIterator<double, 64, 126, 0, 7 >(strdata);
  testQualityIterator<double, 64, 126, 0, 15>(strdata);
  testQualityIterator<double, 64, 126, 0, 31>(strdata);
}



/**
 * Test k-mer generation with 2 bits for each character
 */
TEST(QualityScoreGenerationIteratorTest, TestIllunima15QualityScoreDouble)
{
  // test sequence: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
  std::string strdata = "CDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

  testQualityIterator<double, 64, 126, 3, 3 >(strdata);
  testQualityIterator<double, 64, 126, 3, 7 >(strdata);
  testQualityIterator<double, 64, 126, 3, 15>(strdata);
  testQualityIterator<double, 64, 126, 3, 31>(strdata);

}


/**
 * Test k-mer generation with 2 bits for each character
 */
TEST(QualityScoreGenerationIteratorTest, TestIllunima18QualityScoreFloat)
{
  // test sequence: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
  std::string strdata = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

  testQualityIterator<float, 33, 126, 0, 3 >(strdata);
  testQualityIterator<float, 33, 126, 0, 7 >(strdata);
  testQualityIterator<float, 33, 126, 0, 15>(strdata);
  testQualityIterator<float, 33, 126, 0, 31>(strdata);

}


//
//
///**
// * Test k-mer generation with 2 bits for each character
// */
//TEST(QualityScoreGenerationIteratorTest, TestIllunima13QualityScoreFloat)
//{
//  // test sequence: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
//  std::string strdata = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
//
//  testQualityIterator<float, 64, 126, 0, 3 >(strdata);
//  testQualityIterator<float, 64, 126, 0, 7 >(strdata);
//  testQualityIterator<float, 64, 126, 0, 15>(strdata);
//  testQualityIterator<float, 64, 126, 0, 31>(strdata);
//
//}
//
//
//
///**
// * Test k-mer generation with 2 bits for each character
// */
//TEST(QualityScoreGenerationIteratorTest, TestIllunima15QualityScoreFloat)
//{
//  // test sequence: !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
//  std::string strdata = "CDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
//
//  testQualityIterator<float, 64, 126, 3, 3 >(strdata);
//  testQualityIterator<float, 64, 126, 3, 7 >(strdata);
//  testQualityIterator<float, 64, 126, 3, 15>(strdata);
//  testQualityIterator<float, 64, 126, 3, 31>(strdata);
//
//}
//
