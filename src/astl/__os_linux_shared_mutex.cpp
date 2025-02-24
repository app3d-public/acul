#include <astl/shared_mutex.hpp>
#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace astl
{
    int futex_wait(std::atomic<int> *addr, int expected)
    {
        while (true)
        {
            int res = syscall(SYS_futex, reinterpret_cast<int *>(addr), FUTEX_WAIT, static_cast<int>(expected), nullptr,
                            nullptr, 0);
            if (res == 0) return 0;
            if (res == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
                if (errno == EINTR) continue;
                return -1;
            }
        }
    }

    int futex_wake(std::atomic<int> *addr, int count)
    {
        return syscall(SYS_futex, reinterpret_cast<int *>(addr), FUTEX_WAKE, count, nullptr, nullptr, 0);
    }

        std::atomic<size_t> g_idx_hint{0};

        void shared_mutex::lock_shared()
        {
            entry_lock &lock = _el[get_thread_idx()];
            while (true)
            {
                int current_rw_lock = lock.wr_lock.load(std::memory_order_acquire);

                if (current_rw_lock & entry_lock::W_MASK)
                {
                    futex_wait(&lock.wr_lock, current_rw_lock);
                    continue;
                }

                if (lock.wr_lock.compare_exchange_weak(current_rw_lock, current_rw_lock + 1, std::memory_order_acq_rel,
                                                    std::memory_order_acquire))
                    break;
            }
        }

    void shared_mutex::unlock_shared()
    {
        int cur_rw_lock;
        while (true)
        {
            cur_rw_lock = _el[get_thread_idx()].wr_lock.load(std::memory_order_acquire);
            if (_el[get_thread_idx()].wr_lock.compare_exchange_weak(cur_rw_lock, cur_rw_lock - 1,
                                                                    std::memory_order_acq_rel))
            {
                futex_wake(&_el[get_thread_idx()].wr_lock, INT32_MAX);
                break;
            }
        }
    }

    void shared_mutex::lock()
    {
        for (size_t i = 0; i < _el.size(); ++i)
        {
            entry_lock &lock = _el[i];
            int current_rw_lock;

            while (true)
            {
                current_rw_lock = lock.wr_lock.load(std::memory_order_acquire);

                if (current_rw_lock != 0)
                {
                    futex_wait(&lock.wr_lock, current_rw_lock);
                    continue;
                }

                if (lock.wr_lock.compare_exchange_weak(current_rw_lock, entry_lock::W_MASK, std::memory_order_acq_rel))
                    break;
            }
        }
    }

    void shared_mutex::unlock()
    {
        for (size_t i = 0; i < _el.size(); ++i)
        {
            entry_lock &lock = _el[i];
            while (true)
            {
                int current = lock.wr_lock.load(std::memory_order_acquire);

                if (lock.wr_lock.compare_exchange_weak(current, 0, std::memory_order_acq_rel,
                                                       std::memory_order_acquire))
                {
                    futex_wake(&lock.wr_lock, INT32_MAX);
                    break;
                }
                else
                    futex_wait(&lock.wr_lock, current);
            }
        }
    }
} // namespace astl