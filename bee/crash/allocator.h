#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

namespace bee::crash {
    class allocator {
    public:
        allocator()
            : page_size_(getpagesize())
            , last_(NULL)
            , current_page_(NULL)
            , page_offset_(0) {
        }

        ~allocator() {
            page_header* next;
            for (page_header* cur = last_; cur; cur = next) {
                next = cur->next;
                munmap(cur, cur->num_pages * page_size_);
            }
        }

        void* allocate(size_t bytes) {
            if (!bytes)
                return NULL;
            if (current_page_ && page_size_ - page_offset_ >= bytes) {
                uint8_t* ret = current_page_ + page_offset_;
                page_offset_ += bytes;
                if (page_offset_ == page_size_) {
                    page_offset_  = 0;
                    current_page_ = NULL;
                }
                return ret;
            }
            const size_t num_pages = (bytes + sizeof(page_header) + page_size_ - 1) / page_size_;

            void* a = mmap(NULL, page_size_ * num_pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
            if (a == MAP_FAILED)
                return NULL;
            struct page_header* header = reinterpret_cast<page_header*>(a);
            header->next               = last_;
            header->num_pages          = num_pages;
            last_                      = header;
            uint8_t* ret               = reinterpret_cast<uint8_t*>(a);
            page_offset_               = (page_size_ - (page_size_ * num_pages - (bytes + sizeof(page_header)))) % page_size_;
            current_page_              = page_offset_ ? ret + page_size_ * (num_pages - 1) : NULL;
            return ret + sizeof(page_header);
        }

    private:
        struct page_header {
            page_header* next;
            size_t num_pages;
        };
        const size_t page_size_;
        page_header* last_;
        uint8_t* current_page_;
        size_t page_offset_;
    };
}
