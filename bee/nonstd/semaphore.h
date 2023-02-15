#pragma once

#if defined(__cpp_lib_semaphore)
#include <semaphore>
#else

#include <mutex>
#include <condition_variable>

namespace bee {
    class binary_semaphore {
    public:
        binary_semaphore(std::ptrdiff_t desired) {
            if (desired != 0) {
                acquire();
            }
        }
        void release() {
            std::unique_lock<std::mutex> lk(mutex);
            if (ok) {
                return;
            }
            ok = true;
            lk.unlock();
            condition.notify_one();
        }
        void acquire() {
            std::unique_lock<std::mutex> lk(mutex);
            condition.wait(lk, [this] { return ok; });
            ok = false;
        }
		template<class Rep, class Period>
		bool try_acquire_for(const std::chrono::duration<Rep, Period>& timeout) {
			std::unique_lock<std::mutex> lk(mutex);
			if (condition.wait_for(lk, timeout, [this] { return ok; })) {
				ok = false;
				return true;
			}
			return false;
		}
		template <class Clock, class Duration>
		bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& timeout) {
			std::unique_lock<std::mutex> lk(mutex);
			if (condition.wait_until(lk, timeout, [this] { return ok; })) {
				ok = false;
				return true;
			}
			return false;
		}
    private:
        std::mutex mutex;
        std::condition_variable condition;
        bool ok = false;
    };
}
namespace std {
    using bee::binary_semaphore;
}
#endif
