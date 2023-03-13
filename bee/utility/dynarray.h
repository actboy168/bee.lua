#pragma once

#include <new>
#include <limits>
#include <type_traits>
#include <memory.h>

namespace bee {
    template <class T>
    class dynarray {
    public:
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

        static_assert(std::is_trivial_v<value_type>);

        explicit dynarray()
            : data_(nullptr)
            , size_(0)
        {}
        explicit dynarray(size_type c)
            : data_(alloc(c))
            , size_(c)
        {}
        ~dynarray() {
            dealloc(data_);
        }
        dynarray(const dynarray&) = delete;
        dynarray& operator=(const dynarray&) = delete;
        dynarray(dynarray&& right)
            : data_(right.data())
            , size_(right.size()) {
            right.data_ = nullptr;
            right.size_ = 0;
        }
        dynarray& operator=(dynarray&& right) noexcept {
            if (this != std::addressof(right)) {
                std::swap(data_, right.data_);
                std::swap(size_, right.size_);
            }
            return *this;
        }
        void swap(dynarray& right) noexcept {
            if (this != std::addressof(right)) {
                std::swap(data_, right.data_);
                std::swap(size_, right.size_);
            }
        }
        bool            empty()                 const noexcept { return size_ == 0; }
        pointer         data()                        noexcept { return data_; }
        const_pointer   data()                  const noexcept { return data_; }
        size_type       size()                  const noexcept { return size_; }
        const_iterator  begin()                 const noexcept { return data_; }
        iterator        begin()                       noexcept { return data_; }
        const_iterator  end()                   const noexcept { return data_ + size_; }
        iterator        end()                         noexcept { return data_ + size_; }
        const_reference operator[](size_type i) const noexcept { return data_[i]; }
        reference       operator[](size_type i)       noexcept { return data_[i]; }

    private:
        class bad_array_length : public std::bad_alloc {
        public:
            bad_array_length() throw() { }
            virtual ~bad_array_length() throw() { }
            virtual const char* what() const throw() { 
                return "bad_array_length"; 
            }
        };
        static pointer alloc(size_type n) { 
            if (n > (std::numeric_limits<size_type>::max)()/sizeof(T)) {
                throw bad_array_length();
            }
            return reinterpret_cast<pointer>(new std::byte[n*sizeof(T)]); 
        }
        static void dealloc(pointer ptr) { 
            delete[] reinterpret_cast<std::byte*>(ptr);
        }
        void uninitialized_copy(const_iterator f, const_iterator l, iterator v) noexcept {
            memcpy(v, f, sizeof(value_type) * (l - f));
        }
    private:
        pointer   data_;
        size_type size_;
    };
}
