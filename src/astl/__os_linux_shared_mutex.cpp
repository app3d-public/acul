#include <astl/shared_mutex.hpp>
#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

int futex_wait(std::atomic<u64> *addr, u64 expected)
{
    return syscall(SYS_futex, reinterpret_cast<i32 *>(addr), FUTEX_WAIT, expected, NULL, NULL, 0);
}

int futex_wake(std::atomic<u64> *addr, int count)
{
    return syscall(SYS_futex, reinterpret_cast<i32 *>(addr), FUTEX_WAKE, count, NULL, NULL, 0);
}

namespace astl
{
    std::atomic<size_t> g_idx_hint{0};

    void shared_mutex::lock_shared()
    {
        entry_lock &lock = _el[get_thread_idx()];
        while (true)
        {
            u64 current_rw_lock = lock.wr_lock.load(std::memory_order_acquire);

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
        size_t cur_rw_lock;
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
            u64 current_rw_lock;

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
            size_t expected = entry_lock::W_MASK;
            while (
                !lock.wr_lock.compare_exchange_weak(expected, 0, std::memory_order_acq_rel, std::memory_order_acquire))
                futex_wait(&lock.wr_lock, expected);
            futex_wake(&lock.wr_lock, INT32_MAX);
        }
    }
} // namespace astl