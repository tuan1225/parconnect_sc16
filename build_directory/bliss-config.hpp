/**
file comment
*/

#ifndef CONFIG_H
#define CONFIG_H

#ifndef __cplusplus
#error A C++ compiler is required!
#endif

// CMakeLists.txt conditionally sets MPI_DEFINE
#define USE_MPI

// CMakeLists.txt conditionally sets OPENMP_DEFINE



// source location
#define PROJ_SRC_DIR "/home/tuan/parconnect_SCC16/ext/bliss"

// binary location
#define PROJ_BIN_DIR "/home/tuan/parconnect_SCC16/build_directory/ext/bliss"

// OpenMP pragma "default(none)" clause, for debugging use.
#if (defined(OMP_DEBUG) && !defined(__clang__))
#define OMP_SHARE_DEFAULT default(none)
#else
#define OMP_SHARE_DEFAULT
#endif


#define BLISS_UNUSED(x) do { (void)(x); } while(0)
// gcc before 4.8.1 doesn't have alignas, and can use form below, but gcc needs decltype from 4.8.1 anyways.
#if defined(__ICC) && (__ICC < 15)
#define BLISS_ALIGNED_ARRAY(name, count, alignsize) name[count] __attribute__((aligned(alignsize)))
#define BLISS_ALIGNED_VAR(name, alignsize)  name __attribute__((aligned(alignsize)))
#else 
#define BLISS_ALIGNED_ARRAY(name, count, alignsize) name alignas(alignsize) [count]
#define BLISS_ALIGNED_VAR(name, alignsize) name alignas(alignsize)
#endif 


#endif /* CONFIG_H */
