/**
 * @file    unzip_iterator.hpp
 * @ingroup iterators
 * @author  Tony Pan <tpan7@gatech.edu>
 * @brief   take a zip iterator and unzip it, extracting one of 2 iterators.
 * @details This class differs from other iterators in that its purpose is to provide a way to access the values in one of the components
 *          of a zip iterator.  Increment relies on the zip iterators, but dereferencing relies on access to the zip iterator's member iterator.
 *          in other words, if 2 unzip iterator refers to the same zip iterator, whether they refer to the same component or not,
 *          increment will affect the same zip iterator.  Thus this does not support multipass, and therefore can only be an input iterator.
 *
 * Copyright (c) 2014 Georgia Institute of Technology.  All Rights Reserved.
 *
 * TODO add License
 */
#ifndef UNZIP_ITERATOR_HPP_
#define UNZIP_ITERATOR_HPP_

#include <cstddef>
#include <type_traits>
#include <iterator>


// TODO: tuple version that supports arbitrary number of base iterators,  via variadic templating.

namespace bliss
{
  namespace iterator
  {

    /**
     * @class    bliss::iterator::UnzipIterator
     * @brief    iterator that returns one of elemenst from the tuple/pair in the tuple from a zip iterator
     * @details
     *
     * @note     This is a forward iterator only.
     *
     * @tparam FirstIter    Type of first iterator to zip
     * @tparam SecondIter   Type of second iterator to zip
     *
     */
    template<typename ZipIter, int select=0, bool advance=true>
    class UnzipIterator :  public std::iterator<std::input_iterator_tag,
                                              typename std::conditional<select==0,
											  	  typename std::iterator_traits<ZipIter>::value_type::first_type,
											  	  typename std::iterator_traits<ZipIter>::value_type::second_type >::type >
    {
      protected:
        // reference to underlying ZipIterator.  This is key that enables lock step increment of zip iterator components.
        ZipIter& iter;


      public:
        /**
         * default constructor. does nothing.
         */
        UnzipIterator() = delete;


        /**
         * @param _first      zip iterator reference.  note that we CANNOT make a copy nor move the input.
         */
        UnzipIterator(ZipIter& _iter) : iter(_iter) {};


        /**
         * default copy constructor
         * @param other  instance of UnzipIterator to copy from
         */
        UnzipIterator(const UnzipIterator<ZipIter, select, advance> & other) : iter(other.iter) {};

        /**
         * default copy assignment operator
         * @param other  instance of UnzipIterator to copy from
         * @return reference to self
         */
        UnzipIterator<ZipIter, select, advance>& operator=(const UnzipIterator<ZipIter, select, advance> & other) {
          iter = other.iter;
          return *this;
        }

        /**
         * default move constructor
         * @param other  instance of UnzipIterator to move from
         */
        UnzipIterator(UnzipIterator<ZipIter, select, advance> && other) : iter(other.iter) {};

        /**
         * default move assignment operator.
         * @param other  instance of UnzipIterator to move from
         * @return reference to self
         */
        UnzipIterator<ZipIter, select, advance>& operator=(UnzipIterator<ZipIter, select, advance> && other) {
          iter = other.iter;
          return *this;
        };



        /**
         * default destructor
         */
        virtual ~UnzipIterator() {};


        /**
         * @brief   pre-increment operator: ++iter.  increments underlying iterators
         * @return  reference to incremented iterator
         */
        UnzipIterator<ZipIter, select, advance>& operator++() {
        	if (advance) ++iter;   // only advance if specified to do so.
        	return *this;
        }

        /**
         * @brief   post-increment operator: iter++.  calls ++iter
         * @param   dummy for c++ to identify this as post increment.
         * @return  incremented copy of iterator
         */
        UnzipIterator<ZipIter, select, advance> operator++(int) {
          UnzipIterator<ZipIter, select, advance> out(*this);
          this->operator++();
          return out;
        }

        /**
         * @brief compare to other iterator for equality.  inspect the underlying iterators
         * @param other   iterator to compare to.
         * @return  bool, true if equal, false otherwise.
         */
        bool operator==(const UnzipIterator<ZipIter, select, advance>& other) const {
          return iter == other.iter;
        }

        /**
         * @brief compare to other iterator for inequality
         * @param other   iterator to compare to.
         * @return  bool, true if not equal, false otherwise.
         */
        bool operator!=(const UnzipIterator<ZipIter, select, advance>& other) const {
          return !(this->operator==(other));
        }

        /**
         * @brief dereference function, *iter.  this basically dereferences the zip iterator and chooses the component.
         * @return  current value
         */
        template <int S = select, typename = typename std::enable_if<S == 0, void>::type>
        typename std::iterator_traits<ZipIter>::value_type::first_type operator*() {
        	return (*iter).first;
        }
        template <int S = select, typename = typename std::enable_if<S == 1, void>::type>
        typename std::iterator_traits<ZipIter>::value_type::second_type operator*() {
        	return (*iter).second;
        }

    };

    template <typename ZipIter, int select = 0>
    using AdvancingUnzipIterator = UnzipIterator<ZipIter, select, true>;

    template <typename ZipIter, int select = 0>
    using NonAdvancingUnzipIterator = UnzipIterator<ZipIter, select, false>;


  } /* namespace iterator */
} /* namespace bliss */

#endif /* UNZIP_ITERATOR_HPP_ */
