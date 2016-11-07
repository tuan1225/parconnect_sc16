
#include <mpi.h>
#include <omp.h>
#include <unistd.h> // for sleep!

#include <iostream>
#include <functional>

#include "io/communication_layer.hpp"

int my_rank;
std::atomic<int> msgs_received(0);
std::atomic<int> lookup_received(0);
std::atomic<int> answers_received(0);
volatile bool after = false;
int elems;

template <bool ThreadLocal = true>
struct Tester
{
  const int ANSWER_TAG = 12;
  const int LOOKUP_TAG = 13;

  int generate_message(const int srcRank, const int dstRank)
  {
    return (srcRank + 1) * 100000 + (dstRank + 1);
  }


  void lookup_callback(uint8_t* msg, std::size_t count, int fromRank, uint64_t epoch)
  {
    // first: deserialize
    int* msgs = reinterpret_cast<int*>(msg);
    size_t msg_count = count / sizeof(int);

    int expected = generate_message(fromRank, my_rank);

    bool error = false;

    // for all received requests, send the value from the lookup
    std::vector<int> u(msg_count);
    for (size_t i = 0; i < msg_count; ++i)
    {

      if (after) {
          ERRORF("ERROR: Rank %d message received from %d after finish: %d, count %lu / %lu", my_rank, fromRank, msgs[i], i, msg_count);
          error = true;
      }

      // check that the message is as expected
      if(msgs[i] != expected)
      {
        ERRORF("ERROR: LOOKUP message not as expected: %d expected %d, count %lu / %lu", msgs[i], expected, i, msg_count);
        error = true;
      } else {
        // DEBUG("SUCCESS: message received");
        lookup_received.fetch_add(1);

      }
      u[i] = msgs[i] + 1000;


      if ((u[i] / 100000 != fromRank + 1) || (u[i] % 1000 != my_rank + 1)) ERRORF("ERROR: DEBUG: ANSWER message not correct: %d <- %d u= %d", fromRank, my_rank, u[i]);

      commLayer.sendMessage(&(u[i]), 1, fromRank, ANSWER_TAG);
    }
    if (error) exit(EXIT_FAILURE);
  }

  void answer_callback(uint8_t* msg, std::size_t count, int fromRank, uint64_t epoch)
  {
    // first: deserialize
    int* msgs = reinterpret_cast<int*>(msg);
    size_t msg_count = count / sizeof(int);

    int expected = generate_message( my_rank, fromRank) + 1000;

    bool error = false;
    for (size_t i = 0; i < msg_count; ++i)
    {
      if (after) {
        ERRORF("ERROR: Rank %d message received from %d after finish: %d, count %lu / %lu", my_rank, fromRank, msgs[i], i, msg_count);
        error = true;
      }

      // check that the message is as expected
      if(msgs[i] != expected)
      {
        ERRORF("ERROR: ANSWER message not as expected: %d expected %d, count %lu / %lu", msgs[i], expected, i, msg_count);
        error = true;
      }
      answers_received.fetch_add(1);
    }
    if (error) exit(EXIT_FAILURE);

  }

  void test_comm_layer(int iters, int els)
  {
    DEBUG("Testing Comm Layer");
    DEBUG("Size: " << commLayer.getCommSize());
    DEBUG("Rank: " << commLayer.getCommRank());

    // set global rank
    my_rank = commLayer.getCommRank();
    using namespace std::placeholders;
    commLayer.addReceiveCallback(LOOKUP_TAG, std::bind(&Tester::lookup_callback, this, _1, _2, _3, _4));
    commLayer.addReceiveCallback(ANSWER_TAG, std::bind(&Tester::answer_callback, this, _1, _2, _3, _4));

    commLayer.initCommunication();

    int nthreads = numThreads;
    int it = 0;
//
//      // R: src rank
//      // T: thread id
//      // I: iteration
//      // D: dest rank
//      // t: tag
//      // i: message counter
//      // M: message
//      // L: recv message cont
//

    std::vector<int> msgs(commSize*els);
    /* phase 2 communication */
    for (; it < iters; ++it) {

      msgs.clear();

      // sending one message to each:
  #pragma omp parallel for OMP_SHARE_DEFAULT num_threads(nthreads) shared(msgs, els, my_rank, it, after, stdout)
      for (int i = 0; i < els; ++i)
      {
        size_t idx;

        for (int j = 0; j < commSize; ++j)
        {
          idx = i * commSize + j;
          msgs[idx] = generate_message(my_rank, j);

          commLayer.sendMessage(&(msgs[idx]), 1, j, LOOKUP_TAG);

          if (i == 0 || i == els - 1 || after)
            DEBUGF("W R %d,\tT %d,\tI %d,\tD %d,\tt %d,\ti %d/%d,\tM %d", my_rank, omp_get_thread_num(), it, j, LOOKUP_TAG, i, els, msgs[idx]);

          if ((msgs[idx] / 100000 != my_rank + 1) || (msgs[idx] % 1000 != j + 1)) ERRORF("ERROR: LOOKUP message not correct: %d -> %d u= %d", my_rank, j, msgs[idx]);


        }
      }

      // flush both tags
      DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d PREFLUSH", my_rank, it, LOOKUP_TAG, els, lookup_received.load());
      commLayer.flush(LOOKUP_TAG);
      DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d POSTFLUSH", my_rank, it, LOOKUP_TAG, els, lookup_received.load());


      DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d PREFLUSH", my_rank, it, ANSWER_TAG, els, lookup_received.load());
      commLayer.flush(ANSWER_TAG);
      DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d POSTFLUSH", my_rank, it, ANSWER_TAG, els, lookup_received.load());

    }

    // flush both tags
    DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d PREFINISH", my_rank, it, LOOKUP_TAG, els, lookup_received.load());
    commLayer.finish(LOOKUP_TAG);
    DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d POSTFINISH", my_rank, it, LOOKUP_TAG, els, lookup_received.load());

    DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d PREFINISH", my_rank, it, ANSWER_TAG, els, answers_received.load());
    commLayer.finish(ANSWER_TAG);
    DEBUGF("M R %d,\tT  ,\tI %d,\tD  ,\tt %d,\ti %d,\tM ,\tL%d POSTFINISH", my_rank, it, ANSWER_TAG, els, answers_received.load());

    after = true;

    //======== call flush twice helps to avoid error below.  HACK
    // check that all messages have been received correctly
    if (lookup_received != els * commSize * iters)
    {
      ERRORF("M R %d,\tT  ,\tI  ,\tD  ,\tt %d,\ti  ,\tM ,\tL%d, \tFAIL: expected %d", my_rank, LOOKUP_TAG, lookup_received.load(), els * commSize * iters);
//      exit(EXIT_FAILURE);
    }

    //======== call flush twice helps to avoid error below.  HACK
    // check that all messages have been received correctly
    if (answers_received != els * commSize * iters)
    {
      ERRORF("M R %d,\tT  ,\tI  ,\tD  ,\tt %d,\ti  ,\tM ,\tL%d, \tFAIL: expected %d", my_rank, ANSWER_TAG, answers_received.load(), els * commSize * iters);
//      exit(EXIT_FAILURE);
    }

    INFOF("M R %d, QUERY DONE. ", commRank);



    commLayer.finishCommunication();

    //INFO( "LOOKUP: " << lookup_received << " ANSWERS: " << answers_received );

    DEBUGF("This was a triumph.");
//    sleep(1);
    DEBUGF("I'm making a note here: HUGE SUCCESS.");
//    sleep(1);
    DEBUGF("It's hard to overstate my satisfaction.");

    INFOF("M R %d, ALL DONE. ", commRank);
  }

  Tester(MPI_Comm comm, int comm_size, int num_threads) :
    commLayer(comm, comm_size, num_threads), commSize(comm_size), numThreads(num_threads) {
    //commLayer.startThreads();
    MPI_Comm_rank(comm, &commRank);
  }

  bliss::io::CommunicationLayer<ThreadLocal> commLayer;

  int commSize;
  int commRank;
  int numThreads;
};

int main(int argc, char *argv[])
{
  int nthreads = 1;
  if (argc > 1) {
    nthreads = atoi(argv[1]);
  }

  int elems = 1536 * nthreads;
  if (argc > 2) {
    elems = atoi(argv[2]);
  }

  int iters = 10;
  if (argc > 3) {
    iters = atoi(argv[3]);
  }


  // set up MPI
  MPI_Init(&argc, &argv);

  // get communicator size and my rank
  MPI_Comm comm = MPI_COMM_WORLD;
  int p, rank;
  MPI_Comm_size(comm, &p);
  MPI_Comm_rank(comm, &rank);

  {
    char hostname[256];
    memset(hostname, 0, 256);
    gethostname(hostname, 256);
    INFOF("Rank %d hostname [%s]\n", rank, hostname);
  }
  MPI_Barrier(comm);


  /* code */
  {
    msgs_received.store(0);
    lookup_received.store(0);
    answers_received.store(0);

/// SWAPPING BUFFER PTRS IN MULTITHREADED ENVIRONMENT IS NOT SAFE
//#if defined(THREADLOCAL)
    Tester<true> tester(comm, p, nthreads);
//#else
//    Tester<false> tester(comm, p, nthreads);
//#endif
    tester.test_comm_layer(iters, elems);

    MPI_Barrier(comm);
  }

  // finalize MPI
  MPI_Finalize();
  return 0;
}

