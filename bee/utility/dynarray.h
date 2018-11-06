#pragma once

#include <bee/utility/array_view.h>

namespace std {
	class bad_array_length : public bad_alloc {
	public:
		bad_array_length() throw() { }
		virtual ~bad_array_length() throw() { }
		virtual const char* what() const throw() { 
			return "std::bad_array_length"; 
		}
	};

	template <class T>
	class dynarray : public array_view<T> {
	public:
		typedef       array_view<T>                   mybase;
		typedef       T                               value_type;
		typedef       T&                              reference;
		typedef const T&                              const_reference;
		typedef       T*                              pointer;
		typedef const T*                              const_pointer;
		typedef       T*                              iterator;
		typedef const T*                              const_iterator;
		typedef std::reverse_iterator<iterator>       reverse_iterator;
		typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
		typedef size_t                                size_type;
		typedef ptrdiff_t                             difference_type;

		explicit dynarray(size_type c)
			: mybase(alloc(c), c)
		{ 
			size_type i = 0;
			try {
				for (i = 0; i < mybase::count_; ++i) {
					new (mybase::store_+i) T;
				}
			}
			catch (...) {
				for (; i > 0; --i)
					(mybase::store_+(i-1))->~T();
				throw;
			} 
		}

		dynarray(const dynarray& d)
			: mybase(alloc(d.size()), d.size())
		{ 
			try { 
				uninitialized_copy(d.begin(), d.end(), mybase::begin());
			}
			catch (...) {
				delete[] reinterpret_cast<char*>(mybase::store_);
				throw; 
			} 
		}

		dynarray(dynarray&& d)
			: mybase(d.store_, d.count_)
		{
			d.store_ = nullptr;
			d.count_ = 0;
		}

		dynarray(std::initializer_list<T> l)
			: mybase(alloc(l.size()), l.size())
		{
			try {
				uninitialized_copy(l.begin(), l.end(), mybase::begin());
			}
			catch (...) {
				delete[] reinterpret_cast<char*>(mybase::store_);
				throw;
			}
		}

		template <class Vec>
		dynarray(const Vec& v, typename std::enable_if<std::is_same<typename Vec::value_type, T>::value>::type* =0)
			: mybase(alloc(v.size()), v.size())
		{
			try {
				uninitialized_copy(v.begin(), v.end(), mybase::begin());
			}
			catch (...) {
				delete[] reinterpret_cast<char*>(mybase::store_);
				throw;
			}
		}

		~dynarray()
		{
			for (size_type i = 0; i < mybase::count_; ++i) {
				(mybase::store_+i)->~T();
			}
			delete[] reinterpret_cast<char*>(mybase::store_);
		}

		dynarray& operator=(const dynarray& d) {
			if (this != &d) {
				mybase::store_ = alloc(d.count_);
				mybase::count_ = d.count_;
				try {
					uninitialized_copy(d.begin(), d.end(), mybase::begin());
				}
				catch (...) {
					delete[] reinterpret_cast<char*>(mybase::store_);
					throw;
				}
			}
			return *this;
		}
		dynarray& operator=(dynarray&& d) {
			if (this != &d) {
				mybase::store_ = d.store_;
				mybase::count_ = d.count_;
				d.store_ = nullptr;
				d.count_ = 0;
			}
			return *this;
		}

	private:
		pointer alloc(size_type n)
		{ 
			if (n > (std::numeric_limits<size_type>::max)()/sizeof(T))
			{
				throw bad_array_length();
			}
			return reinterpret_cast<pointer>(new char[n*sizeof(T)]); 
		}
	};
}
