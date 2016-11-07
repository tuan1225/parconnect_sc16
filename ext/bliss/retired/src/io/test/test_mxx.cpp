
// include google test
#include <gtest/gtest.h>
#include "io/mxx_support.hpp"

#include <vector>
#include <random>
#include <iterator>  // for ostream iterator.
#include <algorithm>  // for transform.
#include <unordered_set>
#include <unordered_map>

#include "common/alphabets.hpp"

#include "utils/timer.hpp"

// include files to test
#include "utils/logging.h"

#if 0
/*
 * test class holding some information.  Also, needed for the typed tests
 */
class Mxx2Test : public ::testing::TestWithParam<::std::pair<int, int> >
{
  protected:
    typedef long int T;
    ::std::vector<T> input;
    ::std::vector<T> gold;
    ::std::vector<T> test;

    typedef ::std::pair<T, int> P;

    ::std::vector<P > reduc_input;
    ::std::vector<P > reduc_gold;
    ::std::vector<P > reduc_test;

    int p;

    virtual void SetUp()
    { // generate some inputs
      p = GetParam().second;


      std::default_random_engine generator;
      std::uniform_int_distribution<T> distribution(0, p-1);

      T v;
      for (int i = 0; i < GetParam().first; ++i) {
        v = distribution(generator);
        input.emplace_back(v);
        reduc_input.emplace_back(v, 1);
      }
    }

};


TEST_P(Mxx2Test, bucketing)
{


  auto pp = this->p;

  typedef typename Mxx2Test::T TypeParam;


  std::vector<int> gold_counts = ::mxx2::bucketing_copy<int>(this->input, this->gold, [&pp](TypeParam const & x) { return x % pp; }, pp);

  this->test = this->input;

  std::vector<int> test_counts = ::mxx2::bucketing<int>(this->test, [&pp](TypeParam const & x) { return x % pp; }, pp);

//  printf("bucket: count %d, buckets %d\n", this->GetParam().first, this->GetParam().second);

  bool same_counts = ::std::equal(gold_counts.begin(), gold_counts.end(), test_counts.begin());
  if (!same_counts)
  {
    ::std::ostream_iterator<int> oit(::std::cout, ", ");
    ::std::cout << "gold counts: " << ::std::endl;
    ::std::copy(gold_counts.begin(), gold_counts.end(), oit);
    ::std::cout << "test counts: " << ::std::endl;
    ::std::copy(test_counts.begin(), test_counts.end(), oit);
  }
  EXPECT_TRUE(same_counts);

  bool same_contents = true;


  auto start = this->test.begin();
  auto start_gold = this->gold.begin();
  {
  ::std::ostream_iterator<TypeParam> oit(::std::cout, ", ");
  for (size_t i = 0; i < test_counts.size(); ++i) {
    ::std::sort(start, start + test_counts[i]);
    ::std::sort(start_gold, start_gold + gold_counts[i]);
    same_contents = ::std::equal(start_gold, start_gold + gold_counts[i], start);

    if (!same_contents) {
      ::std::cout << "test: ";
      ::std::copy(start, start + test_counts[i], oit);
      ::std::cout << ::std::endl;
      ::std::cout << "gold: ";
      ::std::copy(start_gold, start_gold + gold_counts[i], oit);
      ::std::cout << ::std::endl;
    }

    start += test_counts[i];
    start_gold += gold_counts[i];


    EXPECT_TRUE(same_contents);


  }
  }
}


TEST_P(Mxx2Test, unique)
{


  auto pp = this->p;
  typedef typename Mxx2Test::T TypeParam;


  this->gold = this->input;

  ::std::sort(this->gold.begin(), this->gold.end());
  auto end = ::std::unique(this->gold.begin(), this->gold.end());
  this->gold.erase(end, this->gold.end());
  ::std::vector<int> gold_counts = ::mxx2::bucketing<int>(this->gold, [&pp](TypeParam const & x) { return x % pp; }, pp);

  {
  auto global_reduc = this->input;

  ::std::unordered_set<TypeParam> set(global_reduc.begin(), global_reduc.end(), global_reduc.size());
  auto newend = ::std::copy(set.begin(), set.end(), global_reduc.begin());
  global_reduc.erase(newend, global_reduc.end());

  ::std::vector<int> global_counts = ::mxx2::bucketing<int>(global_reduc, [&](TypeParam const & x) { return x % pp; }, pp);
  }



  this->test = this->input;

  std::vector<int> test_counts = ::mxx2::bucketing<int>(this->test, [&pp](TypeParam const & x) { return x % pp; }, pp);
  ::mxx2::bucket_unique<::std::unordered_set<TypeParam>, ::std::equal_to<TypeParam> >(this->test, test_counts);

//  printf("unique: count %d, buckets %d\n", this->GetParam().first, this->GetParam().second);


  bool same_counts = ::std::equal(gold_counts.begin(), gold_counts.end(), test_counts.begin());
  if (!same_counts)
  {
    ::std::ostream_iterator<int> oit(::std::cout, ", ");
    ::std::cout << "gold counts: " << ::std::endl;
    ::std::copy(gold_counts.begin(), gold_counts.end(), oit);
    ::std::cout << ::std::endl;
    ::std::cout << "test counts: " << ::std::endl;
    ::std::copy(test_counts.begin(), test_counts.end(), oit);
    ::std::cout << ::std::endl;
  }
  EXPECT_TRUE(same_counts);

  bool same_contents = true;

  auto start = this->test.begin();
  auto start_gold = this->gold.begin();
  ::std::ostream_iterator<TypeParam> oit(::std::cout, ", ");
  for (size_t i = 0; i < test_counts.size(); ++i) {
    ::std::sort(start, start + test_counts[i]);
    ::std::sort(start_gold, start_gold + gold_counts[i]);
    same_contents = ::std::equal(start_gold, start_gold + gold_counts[i], start);

    if (!same_contents) {
      ::std::cout << "test: ";
      ::std::copy(start, start + test_counts[i], oit);
      ::std::cout << ::std::endl;
      ::std::cout << "gold: ";
      ::std::copy(start_gold, start_gold + gold_counts[i], oit);
      ::std::cout << ::std::endl;
    }
    start += test_counts[i];
    start_gold += gold_counts[i];
    EXPECT_TRUE(same_contents);

  }

}

TEST_P(Mxx2Test, reduce)
{


  auto pp = this->p;
  typedef typename Mxx2Test::T Key;
  typedef typename Mxx2Test::P TypeParam;


  this->reduc_gold = this->reduc_input;

  ::std::sort(this->reduc_gold.begin(), this->reduc_gold.end(),
              [](::std::pair<Key, int> const & x,
                 ::std::pair<Key, int> const & y) { return x.first < y.first; });

  if (this->reduc_gold.size() > 0) {
    auto curr = this->reduc_gold.begin();
    auto it = this->reduc_gold.begin(); ++it;
    for (auto max = this->reduc_gold.end(); it != max; ++it) {
      if (it->first == curr->first) {
        curr->second += it->second;
      } else {
        ++curr;
        *curr = *it;
      }
    }
    ++curr;  // very end, move to next entry.

    this->reduc_gold.erase(curr, this->reduc_gold.end());
  }

  ::std::vector<int> gold_counts = ::mxx2::bucketing<int>(this->reduc_gold, [&pp](TypeParam const & x) { return x.first % pp; }, pp);

  {
  auto global_reduc = this->reduc_input;

  Key key;
  int val;
  ::std::unordered_map<Key, int> map(global_reduc.size());
  for (auto it = global_reduc.begin(); it != global_reduc.end(); ++it) {
    key = it->first;
    val = it->second;
    if (map.count(key) == 0) map[key] = val;  // don't rely on initialization to set T to 0.
    else map[key] += val;
  }
  auto newend = ::std::copy(map.begin(), map.end(), global_reduc.begin());
  global_reduc.erase(newend, global_reduc.end());

  ::std::vector<int> global_counts = ::mxx2::bucketing<int>(global_reduc, [&](TypeParam const & x) { return x.first % pp; }, pp);
  }

  this->reduc_test = this->reduc_input;

  std::vector<int> test_counts = ::mxx2::bucketing<int>(this->reduc_test, [&pp](TypeParam const & x) { return x.first % pp; }, pp);
  ::mxx2::bucket_reduce<::std::unordered_map<Key, int>, ::std::plus<int> >(this->reduc_test, test_counts);

//  printf("unique: count %d, buckets %d\n", this->GetParam().first, this->GetParam().second);


  bool same_counts = ::std::equal(gold_counts.begin(), gold_counts.end(), test_counts.begin());
  if (!same_counts)
  {
    ::std::ostream_iterator<int> oit(::std::cout, ", ");
    ::std::cout << "gold counts: " << ::std::endl;
    ::std::copy(gold_counts.begin(), gold_counts.end(), oit);
    ::std::cout << ::std::endl;
    ::std::cout << "test counts: " << ::std::endl;
    ::std::copy(test_counts.begin(), test_counts.end(), oit);
    ::std::cout << ::std::endl;
  }
  EXPECT_TRUE(same_counts);

  bool same_contents = true;

  auto start = this->reduc_test.begin();
  auto start_gold = this->reduc_gold.begin();
  for (size_t i = 0; i < test_counts.size(); ++i) {
    ::std::sort(start, start + test_counts[i]);
    ::std::sort(start_gold, start_gold + gold_counts[i]);
    same_contents = ::std::equal(start_gold, start_gold + gold_counts[i], start);

    if (!same_contents) {
      ::std::cout << "test: ";
      for (int j = 0; j < test_counts[i]; ++start, ++j) {
        ::std::cout << "[" << start->first << ":" << start->second << "],";
      }
      ::std::cout << ::std::endl;
      ::std::cout << "gold: ";
      for (int j = 0; j < test_counts[i]; ++start, ++j) {
        ::std::cout << "[" << start_gold->first << ":" << start_gold->second << "],";
      }
      ::std::cout << ::std::endl;
    }
    start += test_counts[i];
    start_gold += gold_counts[i];
    EXPECT_TRUE(same_contents);

  }

}




// now register the test cases
//REGISTER_TEST_CASE_P(Mxx2Test, bucketing, unique);


//////////////////// RUN the tests with different types.

INSTANTIATE_TEST_CASE_P(Bliss, Mxx2Test, ::testing::Values(::std::make_pair(100000, 16),
                                                           ::std::make_pair(100000, 32),
                                                           ::std::make_pair(100000, 64),
                                                           ::std::make_pair(100000, 128),
                                                           ::std::make_pair(100000, 256),
                                                           ::std::make_pair(100000, 512),
                                                           ::std::make_pair(100000, 1024),
                                                           ::std::make_pair(100000, 2048),
                                                           ::std::make_pair(200000, 512),
                                                           ::std::make_pair(400000, 512),
                                                           ::std::make_pair(800000, 512),
                                                           ::std::make_pair(1600000, 512)
                                                           ));
//INSTANTIATE_TEST_CASE_P(Bliss, Mxx2Test, ::testing::Values(::std::make_pair(100, 16),
//                                                           ::std::make_pair(100, 32)
//                                                           ));


/* below is not yet working.
*/
#include "common/kmer.hpp"
#include "containers/distributed_map_base.hpp"
#include "index/kmer_hash.hpp"

class Mxx2KmerTest : public ::testing::TestWithParam<::std::pair<int, int> >
{
  protected:
    typedef bliss::common::Kmer<21, bliss::common::DNA, uint64_t> Key;
    typedef bliss::kmer::transform::lex_less<Key> KeyTransform;
    typedef bliss::kmer::hash::farm<Key, false> Hash;

    struct TransformedHash {
        Hash h;
        KeyTransform trans;

        inline uint64_t operator()(Key const& k) const {
          return h(trans(k));
        }
        template<typename V>
        inline uint64_t operator()(::std::pair<Key, V> const& x) const {
          return this->operator()(x.first);
        }
        template<typename V>
        inline uint64_t operator()(::std::pair<const Key, V> const& x) const {
          return this->operator()(x.first);
        }
    };
    template <typename Comparator>
    struct TransformedComp {
        Comparator comp;
        KeyTransform trans;

        inline bool operator()(Key const & x, Key const & y) const {
          return comp(trans(x), trans(y));
        }
        template<typename V>
        inline bool operator()(::std::pair<Key, V> const & x, Key const & y) const {
          return this->operator()(x.first, y);
        }
        template<typename V>
        inline bool operator()(::std::pair<const Key, V> const & x, Key const & y) const {
          return this->operator()(x.first, y);
        }
        template<typename V>
        inline bool operator()(Key const & x, ::std::pair<Key, V> const & y) const {
          return this->operator()(x, y.first);
        }
        template<typename V>
        inline bool operator()(Key const & x, ::std::pair<const Key, V> const & y) const {
          return this->operator()(x, y.first);
        }
        template<typename V>
        inline bool operator()(::std::pair<Key, V> const & x, ::std::pair<Key, V> const & y) const {
          return this->operator()(x.first, y.first);
        }
        template<typename V>
        inline bool operator()(::std::pair<const Key, V> const & x, ::std::pair<const Key, V> const & y) const {
          return this->operator()(x.first, y.first);
        }

    };

    typedef TransformedComp<::std::less<Key> > TransformedLess;
    typedef TransformedComp<::std::equal_to<Key> > TransformedEqual;

    typedef std::unordered_set<Key, TransformedHash, TransformedEqual> Set;
    typedef std::unordered_map<Key, int, TransformedHash, TransformedEqual> Map;


    ::std::vector<Key> input;
    ::std::vector<Key> gold;
    ::std::vector<Key> test;

    typedef ::std::pair<Key, int> P;

    ::std::vector<P > reduc_input;
    ::std::vector<P > reduc_gold;
    ::std::vector<P > reduc_test;

    int p;
    TransformedHash hash;

    virtual void SetUp()
    { // generate some inputs
      p = GetParam().second;


      std::default_random_engine generator;
      std::uniform_int_distribution<uint64_t> distribution(0, p-1);
      uint64_t v;
      uint64_t *vp = &v;
      for (int i = 0; i < GetParam().first; ++i) {
        v = distribution(generator);
        input.emplace_back(Key(vp));
        reduc_input.emplace_back(Key(vp), 1);
      }
    }

};


TEST_P(Mxx2KmerTest, bucketing)
{


  auto pp = this->p;
  auto h = this->hash;

  typedef typename Mxx2KmerTest::Key TypeParam;


  std::vector<int> gold_counts = ::mxx2::bucketing_copy<int>(this->input, this->gold, [&](TypeParam const & x) { return h(x) % pp; }, pp);

  this->test = this->input;

  std::vector<int> test_counts = ::mxx2::bucketing<int>(this->test, [&](TypeParam const & x) { return h(x) % pp; }, pp);

//  printf("bucket: count %d, buckets %d\n", this->GetParam().first, this->GetParam().second);

  bool same_counts = ::std::equal(gold_counts.begin(), gold_counts.end(), test_counts.begin());
  if (!same_counts)
  {
    ::std::ostream_iterator<int> oit(::std::cout, ", ");
    ::std::cout << "gold counts: " << ::std::endl;
    ::std::copy(gold_counts.begin(), gold_counts.end(), oit);
    ::std::cout << std::endl;
    ::std::cout << "test counts: " << ::std::endl;
    ::std::copy(test_counts.begin(), test_counts.end(), oit);
    ::std::cout << std::endl;
  }
  EXPECT_TRUE(same_counts);

  bool same_contents = true;


  auto start = this->test.begin();
  auto start_gold = this->gold.begin();
  {
  for (size_t i = 0; i < test_counts.size(); ++i) {
    ::std::sort(start, start + test_counts[i], Mxx2KmerTest::TransformedLess());
    ::std::sort(start_gold, start_gold + gold_counts[i], Mxx2KmerTest::TransformedLess());
    same_contents = ::std::equal(start_gold, start_gold + gold_counts[i], start, Mxx2KmerTest::TransformedEqual());

    if (!same_contents)
    {
      ::std::cout << i << " test: ";
      for (int j = 0; j < test_counts[i]; ++start, ++j) {
        ::std::cout << start->getConstData()[0] << "(" << (h(*start) %pp) << "),";
      }
      ::std::cout << ::std::endl;
      ::std::cout << i << " gold: ";
      for (int j = 0; j < gold_counts[i]; ++start_gold, ++j) {
        ::std::cout << start_gold->getConstData()[0] << "(" << (h(*start_gold) %pp) << "),";
      }
      ::std::cout << ::std::endl;
    } else {
      start += test_counts[i];
      start_gold += gold_counts[i];
    }

    EXPECT_TRUE(same_contents);

  }
  }
}


TEST_P(Mxx2KmerTest, unique)
{


  auto pp = this->p;
  auto h = this->hash;
  typedef typename Mxx2KmerTest::Key TypeParam;


  this->gold = this->input;

  ::std::sort(this->gold.begin(), this->gold.end(), Mxx2KmerTest::TransformedLess());
  auto end = ::std::unique(this->gold.begin(), this->gold.end(), Mxx2KmerTest::TransformedEqual());
  this->gold.erase(end, this->gold.end());
  ::std::vector<int> gold_counts = ::mxx2::bucketing<int>(this->gold, [&](TypeParam const & x) { return h(x) % pp; }, pp);


  {
  auto global_reduc = this->input;

  typename Mxx2KmerTest::Key key;
  typename Mxx2KmerTest::Set set(global_reduc.begin(), global_reduc.end(), global_reduc.size());
  auto newend = ::std::copy(set.begin(), set.end(), global_reduc.begin());
  global_reduc.erase(newend, global_reduc.end());

  ::std::vector<int> global_counts = ::mxx2::bucketing<int>(global_reduc, [&](TypeParam const & x) { return h(x) % pp; }, pp);
  }



  this->test = this->input;

  std::vector<int> test_counts = ::mxx2::bucketing<int>(this->test, [&](TypeParam const & x) { return h(x) % pp; }, pp);
  ::mxx2::bucket_unique<typename Mxx2KmerTest::Set, typename Mxx2KmerTest::TransformedEqual >(this->test, test_counts);

//  printf("unique: count %d, buckets %d\n", this->GetParam().first, this->GetParam().second);


  bool same_counts = ::std::equal(gold_counts.begin(), gold_counts.end(), test_counts.begin());
  if (!same_counts)
  {
    ::std::ostream_iterator<int> oit(::std::cout, ", ");
    ::std::cout << "gold counts: " << ::std::endl;
    ::std::copy(gold_counts.begin(), gold_counts.end(), oit);
    ::std::cout << ::std::endl;
    ::std::cout << "test counts: " << ::std::endl;
    ::std::copy(test_counts.begin(), test_counts.end(), oit);
    ::std::cout << ::std::endl;
  }
  EXPECT_TRUE(same_counts);

  bool same_contents = true;

  auto start = this->test.begin();
  auto start_gold = this->gold.begin();
  for (size_t i = 0; i < test_counts.size(); ++i) {
    ::std::sort(start, start + test_counts[i], Mxx2KmerTest::TransformedLess());
    ::std::sort(start_gold, start_gold + gold_counts[i], Mxx2KmerTest::TransformedLess());
    same_contents = ::std::equal(start_gold, start_gold + gold_counts[i], start, Mxx2KmerTest::TransformedEqual());

    if (!same_contents)
    {
      ::std::cout << i << " test: ";
      for (int j = 0; j < test_counts[i]; ++start, ++j) {
        ::std::cout << start->getConstData()[0] << "(" << (h(*start) %pp) << "),";
      }
      ::std::cout << ::std::endl;
      ::std::cout << i << " gold: ";
      for (int j = 0; j < gold_counts[i]; ++start_gold, ++j) {
        ::std::cout << start_gold->getConstData()[0] << "(" << (h(*start_gold) %pp) << "),";
      }
      ::std::cout << ::std::endl;
    } else {
      start += test_counts[i];
      start_gold += gold_counts[i];
    }
    EXPECT_TRUE(same_contents);

  }

}


TEST_P(Mxx2KmerTest, reduce)
{


  auto pp = this->p;
  auto h = this->hash;

  typedef typename Mxx2KmerTest::P TypeParam;


  this->reduc_gold = this->reduc_input;

  ::std::sort(this->reduc_gold.begin(), this->reduc_gold.end(), Mxx2KmerTest::TransformedLess());

  if (this->reduc_gold.size() > 0) {
    auto curr = this->reduc_gold.begin();
    auto it = this->reduc_gold.begin(); ++it;
    for (auto max = this->reduc_gold.end(); it != max; ++it) {
      if (it->first == curr->first) {
        curr->second += it->second;
      } else {
        ++curr;
        *curr = *it;
      }
    }
    ++curr;  // very end, move to next entry.

    this->reduc_gold.erase(curr, this->reduc_gold.end());
  }

  ::std::vector<int> gold_counts = ::mxx2::bucketing<int>(this->reduc_gold, [&](TypeParam const & x) { return h(x) % pp; }, pp);


  {
  auto global_reduc = this->reduc_input;

  typename Mxx2KmerTest::Key key;
  int val;
  typename Mxx2KmerTest::Map map(global_reduc.size());
  for (auto it = global_reduc.begin(); it != global_reduc.end(); ++it) {
    key = it->first;
    val = it->second;
    if (map.count(key) == 0) map[key] = val;  // don't rely on initialization to set T to 0.
    else map[key] += val;
  }
  auto newend = ::std::copy(map.begin(), map.end(), global_reduc.begin());
  global_reduc.erase(newend, global_reduc.end());

  ::std::vector<int> global_counts = ::mxx2::bucketing<int>(global_reduc, [&](TypeParam const & x) { return h(x) % pp; }, pp);
  }


  this->reduc_test = this->reduc_input;

  std::vector<int> test_counts = ::mxx2::bucketing<int>(this->reduc_test, [&](TypeParam const & x) { return h(x) % pp; }, pp);
  ::mxx2::bucket_reduce<typename Mxx2KmerTest::Map, ::std::plus<int> >(this->reduc_test, test_counts);

//  printf("unique: count %d, buckets %d\n", this->GetParam().first, this->GetParam().second);


  bool same_counts = ::std::equal(gold_counts.begin(), gold_counts.end(), test_counts.begin());
  if (!same_counts)
  {
    ::std::ostream_iterator<int> oit(::std::cout, ", ");
    ::std::cout << "gold counts: " << ::std::endl;
    ::std::copy(gold_counts.begin(), gold_counts.end(), oit);
    ::std::cout << ::std::endl;
    ::std::cout << "test counts: " << ::std::endl;
    ::std::copy(test_counts.begin(), test_counts.end(), oit);
    ::std::cout << ::std::endl;
  }
  EXPECT_TRUE(same_counts);

  bool same_contents = true;

  auto start = this->reduc_test.begin();
  auto start_gold = this->reduc_gold.begin();
  for (size_t i = 0; i < test_counts.size(); ++i) {
    ::std::sort(start, start + test_counts[i], Mxx2KmerTest::TransformedLess());
    ::std::sort(start_gold, start_gold + gold_counts[i], Mxx2KmerTest::TransformedLess());
    same_contents = ::std::equal(start_gold, start_gold + gold_counts[i], start, Mxx2KmerTest::TransformedEqual());

    if (!same_contents) {
      ::std::cout << "test: ";
      for (int j = 0; j < test_counts[i]; ++start, ++j) {
        ::std::cout << "[" << start->first << ":" << start->second << "],";
      }
      ::std::cout << ::std::endl;
      ::std::cout << "gold: ";
      for (int j = 0; j < test_counts[i]; ++start, ++j) {
        ::std::cout << "[" << start_gold->first << ":" << start_gold->second << "],";
      }
      ::std::cout << ::std::endl;
    }
    start += test_counts[i];
    start_gold += gold_counts[i];
    EXPECT_TRUE(same_contents);

  }

}




// now register the test cases
//REGISTER_TEST_CASE_P(Mxx2KmerTest, bucketing, unique);


//////////////////// RUN the tests with different types.

//INSTANTIATE_TEST_CASE_P(Bliss, Mxx2KmerTest, ::testing::Values(::std::make_pair(100, 16),
//                                                           ::std::make_pair(100, 32)
//                                                           ));
INSTANTIATE_TEST_CASE_P(Bliss, Mxx2KmerTest, ::testing::Values(::std::make_pair(10000, 16),
                                                               ::std::make_pair(10000, 32),
                                                               ::std::make_pair(10000, 64),
                                                               ::std::make_pair(10000, 128),
                                                               ::std::make_pair(10000, 256),
                                                               ::std::make_pair(10000, 512),
                                                               ::std::make_pair(10000, 1024),
                                                               ::std::make_pair(10000, 2048),
                                                               ::std::make_pair(20000, 512),
                                                               ::std::make_pair(40000, 512),
                                                               ::std::make_pair(80000, 512),
                                                               ::std::make_pair(160000, 512)
                                                           ));
#endif

