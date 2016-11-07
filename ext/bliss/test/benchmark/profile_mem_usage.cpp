/**
 * @file		quicktest.cpp
 * @ingroup
 * @author	Tony Pan <tpan7@gatech.edu>
 * @brief
 * @details
 *
 * Copyright (c) 2014 Georgia Institute of Technology.  All Rights Reserved.
 *
 * TODO add License
 */


#include <vector>
#include <limits>
#include <cstdlib>
#include <unistd.h>

#include "sys/types.h"
#include "sys/sysinfo.h"

#include "common/kmer.hpp"
#include "common/alphabets.hpp"
#include "common/alphabet_traits.hpp"

#include "io/sequence_iterator.hpp"
#include "common/sequence.hpp"
#include "io/fastq_loader.hpp"


using namespace std;
using namespace bliss::io;


typedef bliss::common::Kmer<21, bliss::common::DNA, uint64_t> KmerType;


typedef KmerType KmerIndexType1;
typedef std::pair<KmerType, bliss::common::ShortSequenceKmerId > KmerIndexType2;
typedef std::pair<KmerType, std::pair<bliss::common::ShortSequenceKmerId, float> > KmerIndexType3;
typedef std::pair<KmerType, std::pair<bliss::common::ShortSequenceKmerId, double> > KmerIndexType4;


void checkMemUsed(long long &phyMemUsed, long long &swapUsed, bool print) {
  //from http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
  struct sysinfo memInfo;

  sysinfo (&memInfo);
  phyMemUsed = (memInfo.totalram - memInfo.freeram) * memInfo.mem_unit;
  swapUsed = (memInfo.totalswap - memInfo.freeswap) * memInfo.mem_unit;
  if (print)
    BL_INFOF("physical mem used %lld, swap used %lld", phyMemUsed, swapUsed);
}

void memUsedvsBaseline(const long long &phyMemUsed, const long long &swapUsed, long long &phyMemUsed2, long long &swapUsed2) {
  checkMemUsed(phyMemUsed2, swapUsed2, false);

  BL_INFOF("physical mem used new %lld, swap used new %lld", phyMemUsed2 - phyMemUsed, swapUsed2 - swapUsed);
}


int main(int argc, char** argv) {
  srand(1);

  long long phyMemUsed, swapUsed;
  long long phyMemUsed2, swapUsed2;



  int size = 1000000;

  BL_INFOF("");
  checkMemUsed(phyMemUsed, swapUsed, true);
  {
    BL_INFOF("KmerIndexElement copy.  element size %lu", sizeof(KmerIndexType1));
    std::vector<KmerIndexType1 > test;
    BL_INFOF("\tcreated vector");

    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

    for (int i = 0; i < size; ++i) {
      KmerIndexType1 kmer;
//      kmer.first = rand() % std::numeric_limits<uint64_t>::max();
      kmer.nextFromChar(rand() % (std::numeric_limits<uint8_t>::max() + 1));
      test.push_back(kmer);
    }
    BL_INFOF("\tpopulated vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);


    test.clear();
    BL_INFOF("\tcleared vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

  }
  BL_INFOF("\tdeallocated vector");
  memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);
  sleep(2);

  BL_INFOF("");
  checkMemUsed(phyMemUsed, swapUsed, true);
  {
    BL_INFOF("KmerIndexElement with move.  element size %lu", sizeof(KmerIndexType1));
    std::vector<KmerIndexType1 > test;
    BL_INFOF("\tcreated vector");

    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

    for (int i = 0; i < size; ++i) {
      KmerIndexType1 kmer;
//      kmer.first = rand() % std::numeric_limits<uint64_t>::max();
      kmer.nextFromChar(rand() % (std::numeric_limits<uint8_t>::max() + 1));
      test.push_back(std::move(kmer));
    }
    BL_INFOF("\tpopulated vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);


    test.clear();
    BL_INFOF("\tcleared vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

  }
  BL_INFOF("\tdeallocated vector");
  memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);
  sleep(2);


  BL_INFOF("");
  checkMemUsed(phyMemUsed, swapUsed, true);
  {
    BL_INFOF("Shared KmerIndexElement with move.  element size %lu", sizeof(KmerIndexType1));
    std::vector<KmerIndexType1 > test;
    BL_INFOF("\tcreated vector");

    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

    KmerIndexType1 kmer;
    for (int i = 0; i < size; ++i) {
//      kmer.first = rand() % std::numeric_limits<uint64_t>::max();
      kmer.nextFromChar(rand() % (std::numeric_limits<uint8_t>::max() + 1));
      test.push_back(std::move(kmer));
    }
    BL_INFOF("\tpopulated vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);


    test.clear();
    BL_INFOF("\tcleared vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

  }
  BL_INFOF("\tdeallocated vector");
  memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);
  sleep(2);





  BL_INFOF("");
  checkMemUsed(phyMemUsed, swapUsed, true);
  {
    BL_INFOF("KmerIndexElementWithId.  element size %lu", sizeof(KmerIndexType2));
    std::vector<KmerIndexType2 > test;
    BL_INFOF("\tcreated vector");

    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

    for (int i = 0; i < size; ++i) {
      KmerIndexType2 kmer;
//      kmer.first = rand() % std::numeric_limits<uint64_t>::max();
      kmer.first.nextFromChar(rand() % (std::numeric_limits<uint8_t>::max() + 1));
      kmer.second = bliss::common::ShortSequenceKmerId(rand() % std::numeric_limits<uint64_t>::max());

      test.push_back(std::move(kmer));
    }
    BL_INFOF("\tpopulated vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);


    test.clear();
    BL_INFOF("\tcleared vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

  }
  BL_INFOF("\tdeallocated vector");
  memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);
  sleep(2);


  BL_INFOF("");
  checkMemUsed(phyMemUsed, swapUsed, true);
  {
    BL_INFOF("KmerIndexElementWithIdAndQuality.  element size %lu", sizeof(KmerIndexType3));
    std::vector<KmerIndexType3 > test;
    BL_INFOF("\tcreated vector");

    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

    for (int i = 0; i < size; ++i) {
      KmerIndexType3 kmer;
//      kmer.first = rand() % std::numeric_limits<uint64_t>::max();
      kmer.first.nextFromChar(rand() % (std::numeric_limits<uint8_t>::max() + 1));
      kmer.second.first = bliss::common::ShortSequenceKmerId(rand() % std::numeric_limits<uint64_t>::max());
      kmer.second.second = float(rand()) / float(RAND_MAX);

      test.push_back(std::move(kmer));
    }
    BL_INFOF("\tpopulated vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);


    test.clear();
    BL_INFOF("\tcleared vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

  }
  BL_INFOF("\tdeallocated vector");
  memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);
  sleep(2);


  checkMemUsed(phyMemUsed, swapUsed, true);
  {
    BL_INFOF("KmerIndexElementWithIdAndQuality.  element size %lu", sizeof(KmerIndexType4));
    std::vector<KmerIndexType4 > test;
    BL_INFOF("\tcreated vector");

    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

    for (int i = 0; i < size; ++i) {
      KmerIndexType4 kmer;
//      kmer.first = rand() % std::numeric_limits<uint64_t>::max();
      kmer.first.nextFromChar(rand() % (std::numeric_limits<uint8_t>::max() + 1));
      kmer.second.first = bliss::common::ShortSequenceKmerId(rand() % std::numeric_limits<uint64_t>::max());
      kmer.second.second = float(rand()) / float(RAND_MAX);

      test.push_back(std::move(kmer));
    }
    BL_INFOF("\tpopulated vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);


    test.clear();
    BL_INFOF("\tcleared vector %lu", test.size());
    memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);

  }
  BL_INFOF("\tdeallocated vector");
  memUsedvsBaseline(phyMemUsed, swapUsed, phyMemUsed2, swapUsed2);
  sleep(2);


  return 0;
}
