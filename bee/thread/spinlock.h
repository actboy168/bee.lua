#pragma once

#include <bee/nonstd/atomic_flag.h>

namespace bee {
    class spinlock {
    public:
        void lock() noexcept;
        void unlock() noexcept;
        bool try_lock() noexcept;

    private:
        atomic_flag l;
    };

    void cpu_relax() noexcept;
}
