#pragma once

#include <memory>
#include <new>
#include <limits>
#include <type_traits>
#include <memory.h>
#include <assert.h>

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

        dynarray()
            : data_(nullptr)
            , size_(0)
        {}
        dynarray(size_type size)
            : data_(alloc(size))
            , size_(size)
        {}
        dynarray(const value_type* data, size_type size)
            : data_(alloc(size))
            , size_(size) {
            memcpy(data_, data, sizeof(value_type) * size);
        }
        template <typename Vec, typename = std::enable_if_t<std::is_same_v<typename Vec::value_type, value_type>>>
        dynarray(Vec const& vec)
            : dynarray(vec.data(), vec.size())
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
        void resize(size_t size) noexcept {
            assert(data_ != nullptr && size <= size_);
            size_ = size;
        }
        pointer release() noexcept {
            pointer r = data_;
            data_ = nullptr;
            size_ = 0;
            return r;
        }
        bool            empty()                 const noexcept { return size_ == 0; }
        pointer         data()                        noexcept { assert(data_ != nullptr); return data_; }
        const_pointer   data()                  const noexcept { assert(data_ != nullptr); return data_; }
        size_type       size()                  const noexcept { assert(data_ != nullptr); return size_; }
        const_iterator  begin()                 const noexcept { assert(data_ != nullptr); return data_; }
        iterator        begin()                       noexcept { assert(data_ != nullptr); return data_; }
        const_iterator  end()                   const noexcept { assert(data_ != nullptr); return data_ + size_; }
        iterator        end()                         noexcept { assert(data_ != nullptr); return data_ + size_; }
        const_reference operator[](size_type i) const noexcept { assert(data_ != nullptr && i < size_); return data_[i]; }
        reference       operator[](size_type i)       noexcept { assert(data_ != nullptr && i < size_); return data_[i]; }

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
    private:
        pointer   data_;
        size_type size_;
    };
}
