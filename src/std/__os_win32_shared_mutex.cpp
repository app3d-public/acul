#include <core/std/shared_mutex.hpp>
#include <windows.h>

namespace astl
{
    std::atomic<size_t> g_idx_hint{0};

    void shared_mutex::lock_shared()
    {
        int retry_count = 0;
        size_t cur_rw_lock;
        while (true)
        {
            cur_rw_lock = _el[get_thread_idx()].wr_lock.load(std::memory_order_acquire);
            if (cur_rw_lock & entry_lock::W_MASK)
            {
                WaitOnAddress(&_el[get_thread_idx()].wr_lock, &cur_rw_lock, sizeof(cur_rw_lock), INFINITE);
                continue;
            }
            if (_el[get_thread_idx()].wr_lock.compare_exchange_weak(cur_rw_lock, cur_rw_lock + 1,
                                                                    std::memory_order_acq_rel))
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
                WakeByAddressAll(&_el[get_thread_idx()].wr_lock);
                break;
            }
        }
    }

    void shared_mutex::lock()
    {
        for (size_t i = 0; i < _el.size(); ++i)
        {
            size_t cur_rw_lock;
            while (true)
            {
                cur_rw_lock = _el[i].wr_lock.load(std::memory_order_acquire);
                if (cur_rw_lock != 0)
                {
                    SwitchToThread();
                    continue;
                }
                if (_el[i].wr_lock.compare_exchange_weak(cur_rw_lock, entry_lock::W_MASK, std::memory_order_acq_rel))
                    break;
            }
        }
    }

    void shared_mutex::unlock()
    {
        for (size_t i = 0; i < _el.size(); ++i)
        {
            size_t cur_rw_lock;
            while (true)
            {
                cur_rw_lock = _el[i].wr_lock.load(std::memory_order_acquire);
                if (_el[i].wr_lock.compare_exchange_weak(cur_rw_lock, 0, std::memory_order_acq_rel))
                {
                    WakeByAddressAll(&_el[i].wr_lock);
                    break;
                }
            }
        }
    }
} // namespace astl