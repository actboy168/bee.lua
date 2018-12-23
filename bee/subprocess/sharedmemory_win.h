#pragma once

#include <cstddef>

namespace bee::win::subprocess {
    struct create_only_t {};
    struct open_only_t {};
    struct open_or_create_t {};
    inline const create_only_t create_only = create_only_t();
    inline const open_only_t open_only = open_only_t();
    inline const open_or_create_t open_or_create = open_or_create_t();

    class filemapping {
    public:
        filemapping(const filemapping& other) = delete;
        filemapping& operator = (const filemapping& other) = delete;
        filemapping(open_only_t, const wchar_t* name);
        filemapping(create_only_t, const wchar_t* name, size_t size);
        ~filemapping();
        bool   ok()     const;
        size_t size()   const;
        void*  handle() const;

    private:
        bool create(const wchar_t* name, size_t size);
        bool open(const wchar_t* name);
    private:
        void*  m_handle;
        size_t m_size;
    };

    class sharedmemory {
    public:
        sharedmemory(const sharedmemory& other) = delete;
        sharedmemory& operator = (const sharedmemory& other) = delete;

        sharedmemory(open_only_t, const wchar_t* name);
        sharedmemory(create_only_t, const wchar_t* name, size_t size);
        sharedmemory(open_or_create_t, const wchar_t* name, size_t size);
        ~sharedmemory();
        bool       ok() const;
        std::byte* data();
        size_t     size() const;

    private:
        bool mapview(size_t size);

    private:
        filemapping m_mapping;
        std::byte*  m_data;
    };
}
