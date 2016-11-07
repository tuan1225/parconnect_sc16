/*
 * Copyright 2015 Georgia Institute of Technology
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file    datatypes.hpp
 * @author  Patrick Flick <patrick.flick@gmail.com>
 * @brief   MPI Datatypes for C++ types.
 */

#ifndef MXX_DATATYPES_HPP
#define MXX_DATATYPES_HPP

// MPI include
#include <mpi.h>

// C++ includes
#include <vector>
#include <map>
#include <array>
#include <tuple>
#include <numeric>
#include <limits>
#include <type_traits>

#include "common.hpp"

namespace mxx
{

/*
 * Mapping of C/C++ types to MPI datatypes.
 *
 * Possible ways of implementation
 * 1) templated function get_mpi_dt<template T>(); (e.g. boost)
 *     -> doesn't allow partial template specialization
 *        (needed for std::pair, std::tuple etc)
 * 2) overloaded functions with type deduction
 *     -> can't properly clean up types (MPI_Type_free)
 * 3) templated class with static method (-> allows partial specialization)
 * 4) templated class with member method (allows proper C++ like Type_free)!!!
 *
 * Using option 4 due to best fit.
 *
 * TODO: other (maybe better) possibility:
 * 5) using templated class with static methods and cache all created datatypes
 *    in global static map (typeid(T) -> MPI_Datatype), freeing upon global
 *    destruction
 */

// TODO:
// - [ ] base class with MPI type modifiers (contiguous, vector, indexed, etc)
// - [x] compile time checker for builtin types
// - [ ] static function to return MPI_Datatype for builtin types!
// - [ ] put attr_map in here!
// - [ ] add to-string and caching
// - [ ] implement MPI type introspection (get envelope)
// - [ ] (see wrappers in official MPI C++ bindings)

template <typename T>
class is_builtin_type : public std::false_type {};

class datatype;

template <typename T>
datatype get_datatype();

template <typename T>
datatype get_datatype(const T&);

// MPI_Datatype wrapper
class datatype {
protected:
    datatype(MPI_Datatype mpidt, bool builtin) : mpitype(mpidt), builtin(builtin) {
    }
    template <typename T>
    friend datatype get_datatype();
    template <typename T>
    friend datatype get_datatype(const T&);
public:
    datatype() : mpitype(MPI_DATATYPE_NULL), builtin(true) {
    }

    // copy constructor
    datatype(const datatype& o) = delete;

    // move constructor
    datatype(datatype&& o) {
        builtin = o.builtin;
        mpitype = o.mpitype;
        o.mpitype = MPI_DATATYPE_NULL;
        o.builtin = true;
    }

    // copy assignment
    datatype& operator=(const datatype& o) = delete;

    // move assignment
    datatype& operator=(datatype&& o) {
        builtin = o.builtin;
        mpitype = o.mpitype;
        o.mpitype = MPI_DATATYPE_NULL;
        o.builtin = true;
        return *this;
    }

    MPI_Datatype type() const {
        return mpitype;
    }

    datatype vector(size_t count, size_t blocklen, size_t stride) const {
        // TODO what if any of the parameters > MAX_INT? -> use Type_create_struct instead!
        //  -> if blocklen >= MAX_INT then use contiguous first, then struct with count elements
        datatype result;
        result.builtin = false;
        MPI_Type_vector(count, blocklen, stride, this->mpitype, &result.mpitype);
        return result;
    }

    datatype contiguous(size_t count) const {
        datatype result;
        if (count <= mxx::max_int) {
            result.builtin = false;
            MPI_Type_contiguous(count, this->mpitype, &result.mpitype);
            MPI_Type_commit(&result.mpitype);
        } else {
            result.builtin = false;
            // create custom data types of blocks and remainder
            std::size_t intmax = mxx::max_int;
            std::size_t nblocks = count / intmax;
            std::size_t rem = count % intmax;

            // create block and remainder data types
            MPI_Datatype _block;
            MPI_Type_contiguous(mxx::max_int, this->mpitype, &_block);
            MPI_Datatype _blocks;
            // create two contiguous types for blocks and remainder
            MPI_Type_contiguous(nblocks, _block, &_blocks);

            if (rem > 0) {
                MPI_Datatype _remainder;
                MPI_Type_contiguous(rem, this->mpitype, &_remainder);

                // create struct for the concatenation of this type
                MPI_Aint lb, extent;
                MPI_Type_get_extent(this->mpitype, &lb, &extent);
                MPI_Aint displ = nblocks*intmax*extent;
                MPI_Aint displs[2] = {0, displ};
                int blocklen[2] = {1, 1};
                MPI_Datatype mpitypes[2] = {_blocks, _remainder};
                MPI_Type_create_struct(2, blocklen, displs, mpitypes, &result.mpitype);
                MPI_Type_commit(&result.mpitype);
                MPI_Type_free(&_remainder);
                MPI_Type_free(&_blocks);
            } else {
                result.mpitype = _blocks;
                MPI_Type_commit(&result.mpitype);
            }
        }
        return result;
    }

    // TODO: indexed/hindexed?

    virtual ~datatype() {
      // TODO: don't free, but decrease counter in type cache
        if (!builtin)
            MPI_Type_free(&mpitype);
    }
private:
    MPI_Datatype mpitype;
    bool builtin;
};


// defined generalized datatype builder
template <typename T>
struct datatype_builder {};

/*********************************************************************
 *                     Define built-in datatypes                     *
 *********************************************************************/

#define MXX_DATATYPE_MPI_BUILTIN(ctype, mpi_type)                           \
template <> struct datatype_builder<ctype> {                                \
    static MPI_Datatype get_type() {return mpi_type;}                       \
    static size_t num_basic_elements() { return 1;}                         \
};                                                                          \
                                                                            \
template <> class is_builtin_type<ctype> : public std::true_type {};        \

// char
MXX_DATATYPE_MPI_BUILTIN(char, MPI_CHAR);
MXX_DATATYPE_MPI_BUILTIN(unsigned char, MPI_UNSIGNED_CHAR);
MXX_DATATYPE_MPI_BUILTIN(signed char, MPI_SIGNED_CHAR);

// short
MXX_DATATYPE_MPI_BUILTIN(unsigned short, MPI_UNSIGNED_SHORT);
MXX_DATATYPE_MPI_BUILTIN(short, MPI_SHORT);

// int
MXX_DATATYPE_MPI_BUILTIN(unsigned int, MPI_UNSIGNED);
MXX_DATATYPE_MPI_BUILTIN(int, MPI_INT);

// long
MXX_DATATYPE_MPI_BUILTIN(unsigned long, MPI_UNSIGNED_LONG);
MXX_DATATYPE_MPI_BUILTIN(long, MPI_LONG);

// long long
MXX_DATATYPE_MPI_BUILTIN(unsigned long long, MPI_UNSIGNED_LONG_LONG);
MXX_DATATYPE_MPI_BUILTIN(long long, MPI_LONG_LONG);

// floats
MXX_DATATYPE_MPI_BUILTIN(float, MPI_FLOAT);
MXX_DATATYPE_MPI_BUILTIN(double, MPI_DOUBLE);
MXX_DATATYPE_MPI_BUILTIN(long double, MPI_LONG_DOUBLE);

#undef MXX_DATATYPE_MPI_BUILTIN

/*********************************************************************
 *                 Pair types for MINLOC and MAXLOC                  *
 *********************************************************************/

template <typename T>
struct datatype_pair {
    static MPI_Datatype get_type() {
        return MPI_DATATYPE_NULL;
    }
};

template <typename T>
class is_builtin_pair_type : public std::false_type {};

#define MXX_DATATYPE_BUILTIN_PAIR(ctype, mpi_type)                          \
template <> struct datatype_pair<ctype> {                                   \
    static MPI_Datatype get_type() {                                        \
        return mpi_type;                                                    \
    }                                                                       \
};                                                                          \
template <> class is_builtin_pair_type<ctype> : public std::true_type {};   \

// integers-integer pairs
MXX_DATATYPE_BUILTIN_PAIR(short, MPI_SHORT_INT);
MXX_DATATYPE_BUILTIN_PAIR(int, MPI_2INT);
MXX_DATATYPE_BUILTIN_PAIR(long, MPI_LONG_INT);

// floats
MXX_DATATYPE_BUILTIN_PAIR(float, MPI_FLOAT_INT);
MXX_DATATYPE_BUILTIN_PAIR(double, MPI_DOUBLE_INT);
MXX_DATATYPE_BUILTIN_PAIR(long double, MPI_LONG_DOUBLE_INT);


#undef MXX_DATATYPE_BUILTIN_PAIR

/**
 * @brief   MPI datatype mapping for std::array
 */
template <typename T, std::size_t size>
struct datatype_builder<std::array<T, size> > {
    static MPI_Datatype get_type() {
        MPI_Datatype _type;
        MPI_Datatype base_type = get_datatype<T>().type();
        MPI_Type_contiguous(size, base_type, &_type);
        MPI_Type_commit(&_type);
        return _type;
    }
    static size_t num_basic_elements() {
        return size*datatype_builder<T>::num_basic_elements();
    }
};

/**
 * @brief   MPI datatype mapping for std::pair
 */
template <typename T1, typename T2>
struct datatype_builder<std::pair<T1, T2> > {
    static MPI_Datatype get_type() {
        MPI_Datatype _type;

        int blocklen[2] = {1, 1};
        MPI_Aint displs[2] = {0,0};
        // get actual displacement (in case of padding in the structure)
        std::pair<T1, T2> p;
        MPI_Aint p_adr, t1_adr, t2_adr;
        MPI_Get_address(&p, &p_adr);
        MPI_Get_address(&p.first, &t1_adr);
        MPI_Get_address(&p.second, &t2_adr);
        displs[0] = t1_adr - p_adr;
        displs[1] = t2_adr - p_adr;

        // create type
        // TODO: use cached type!
        MPI_Datatype types[2] = {datatype_builder<T1>::get_type(), datatype_builder<T2>::get_type()};
        // in case elements are represented the opposite way around in
        // the pair (gcc does so), then swap them
        if (displs[0] > displs[1]) {
            std::swap(displs[0], displs[1]);
            std::swap(types[0], types[1]);
        }
        // create MPI_Datatype (resized to actual sizeof())
        MPI_Datatype struct_type;
        MPI_Type_create_struct(2, blocklen, displs, types, &struct_type);
        MPI_Type_create_resized(struct_type, 0, sizeof(p), &_type);
        MPI_Type_commit(&_type);
        MPI_Type_free(&struct_type);
        return _type;
    }

    static size_t num_basic_elements() {
        return datatype_builder<T1>::num_basic_elements() + datatype_builder<T2>::num_basic_elements();
    }
};


// fill in MPI types
template <std::size_t N, std::size_t I, class ...Types>
struct tuple_members {
    static void get(std::map<MPI_Aint, MPI_Datatype>& members) {
        // init tuple to get measurement offsets
        // TODO: use null-ref instead of actual instantiation
        std::tuple<Types...> tuple;

        // get member displacement
        MPI_Aint t_adr, elem_adr;
        MPI_Get_address(&tuple, &t_adr);
        MPI_Get_address(&std::get<N-I>(tuple), &elem_adr);
        // byte offset from beginning of tuple
        MPI_Aint displ = elem_adr - t_adr;
        // fill in type
        // TODO: use cached type!?
        MPI_Datatype mpi_dt = datatype_builder<typename std::tuple_element<N-I,std::tuple<Types...>>::type>::get_type(); //std::get<N-I>(datatypes).type();

        // add to map
        members[displ] = mpi_dt;

        // recursively (during compile time) call same function
        tuple_members<N,I-1, Types...>::get(members);
    }
};

// Base case of meta-recursion
template <std::size_t N, class ...Types>
struct tuple_members<N, 0, Types...> {
    static void get(std::map<MPI_Aint, MPI_Datatype>&) {
    }
};

template <class...Types>
struct tuple_basic_els;

template <class T, class...Types>
struct tuple_basic_els<T,Types...>
{
    static size_t get_num() {
        return datatype_builder<T>::num_basic_elements() + tuple_basic_els<Types...>::get_num();
    }
};

template <class T>
struct tuple_basic_els<T>
{
    static size_t get_num() {
        return datatype_builder<T>::num_basic_elements();
    }
};

/**
 * @brief   MPI datatype mapping for std::tuple
 */
template <class ...Types>
struct datatype_builder<std::tuple<Types...> > {
  // tuple type
  typedef std::tuple<Types...> tuple_t;
  // number of elements of the typle
  static constexpr std::size_t size = std::tuple_size<tuple_t>::value;

  /// returns the MPI_Datatype for the tuple
  static MPI_Datatype get_type() {
        MPI_Datatype _type;
        // fill in the block lengths to 1 each
        int blocklen[size];
        for (std::size_t i = 0; i < size; ++i) {
            blocklen[i] = 1;
        }

        // get the member displacement and type info for the tuple using
        // meta-recursion
        std::map<MPI_Aint, MPI_Datatype> members;
        tuple_members<size,size,Types...>::get(members);


        // fill displacements and types according to in-memory order in tuple
        // NOTE: the in-memory order is not necessarily the same as the order
        // of types as accessed by std::get
        // For gcc the order is actually reversed!
        // Hence, we use a std::map to collect the order information prior
        // to creating the displacement and type arrays
        std::array<MPI_Aint, size> displs;
        std::array<MPI_Datatype, size> mpitypes;
        std::size_t i = 0;
        for (std::map<MPI_Aint, MPI_Datatype>::iterator it = members.begin();
             it != members.end(); ++it) {
            displs[i] = it->first;
            mpitypes[i] = it->second;
            ++i;
        }

        // create type
        MPI_Datatype struct_type;
        MPI_Type_create_struct(size, blocklen, &displs[0], &mpitypes[0], &struct_type);
        MPI_Type_create_resized(struct_type, 0, sizeof(tuple_t), &_type);
        MPI_Type_commit(&_type);
        MPI_Type_free(&struct_type);

        return _type;
    }

    static size_t num_basic_elements() {
        return tuple_basic_els<Types...>::get_num();
    }
};


/*
 * "templates" for different kinds of data structures.
 * Inherit from these to specialize for your own type easily.
 */


/**
 * @brief   A contiguous datatype of the same base type
 */
template <typename T, std::size_t size>
struct datatype_contiguous {
    static_assert(size <= std::numeric_limits<int>::max(),
                  "Compile time contiguous types only support sizes up to INT_MAX");
    static MPI_Datatype get_type() {
        MPI_Datatype _type;
        datatype _base_type = get_datatype<T>();
        MPI_Type_contiguous(size, _base_type.type(), &_type);
        MPI_Type_commit(&_type);
        return _type;
    }
    static size_t num_basic_elements() {
        return size*datatype_builder<T>::num_basic_elements();
    }
};

/*
 * Runtime selection of size
 */
/*
template <typename T>
struct datatype_contiguous<T,0> {
    static MPI_Datatype get_type(size_t size) {
        datatype dt = get_datatype<T>().contiguous(size);
        MPI_Datatype mpidt;
        MPI_Type_dup(dt.type(), &mpidt);
        MPI_Type_commit(&mpidt);
        return mpidt;
    }
};
*/

template <typename T>
datatype get_datatype() {
    datatype dt(datatype_builder<T>::get_type(), is_builtin_type<T>::value);
    return dt;
}

template <typename T>
datatype get_datatype(const T&) {
    datatype dt(datatype_builder<T>::get_type(), is_builtin_type<T>::value);
    return dt;
}

} // namespace mxx



#endif // MXX_DATATYPES_HPP
