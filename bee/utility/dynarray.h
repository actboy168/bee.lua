#pragma once

#include <memory.h>

#include <cassert>
#include <memory>
#include <type_traits>

// clang-format off
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

        dynarray() noexcept
            : data_()
            , size_(0)
        {}
        dynarray(size_type size) noexcept
            : data_(std::make_unique<value_type[]>(size))
            , size_(size)
        {}
        dynarray(const value_type* data, size_type size) noexcept
            : dynarray(size) {
            memcpy(data_.get(), data, sizeof(value_type) * size);
        }
        template <typename Vec, typename = std::enable_if_t<std::is_same_v<typename Vec::value_type, value_type>>>
        dynarray(Vec const& vec) noexcept
            : dynarray(vec.data(), vec.size())
        {}
        ~dynarray() noexcept
        {}
        dynarray(const dynarray&) = delete;
        dynarray& operator=(const dynarray&) = delete;
        dynarray(dynarray&& right) noexcept
            : dynarray() {
            std::swap(data_, right.data_);
            std::swap(size_, right.size_);
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
            assert(data_ && size <= size_);
            size_ = size;
        }
        pointer release() noexcept {
            size_ = 0;
            return data_.release();
        }
        bool            empty()                 const noexcept { return size_ == 0; }
        pointer         data()                        noexcept { return data_.get(); }
        const_pointer   data()                  const noexcept { return data_.get(); }
        size_type       size()                  const noexcept { return size_; }
        const_iterator  begin()                 const noexcept { assert(data_); return data(); }
        iterator        begin()                       noexcept { assert(data_); return data(); }
        const_iterator  end()                   const noexcept { assert(data_); return data() + size(); }
        iterator        end()                         noexcept { assert(data_); return data() + size(); }
        const_reference operator[](size_type i) const noexcept { assert(data_ && i < size_); return data_[i]; }
        reference       operator[](size_type i)       noexcept { assert(data_ && i < size_); return data_[i]; }

    private:
        std::unique_ptr<value_type[]>  data_;
        size_type                      size_;
    };
}
