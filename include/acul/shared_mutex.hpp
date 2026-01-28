#pragma once

// Based by: https://github.com/Emanem/shared_mutex/tree/master

#include <atomic>
#include <thread>
#include "../acul/api.hpp"
#include "vector.hpp"

#ifndef L1_CACHE_LINESIZE
    #define L1_CACHE_LINESIZE 64
#endif

namespace acul
{
    class APPLIB_API shared_mutex
    {
        struct entry_lock
        {
            const static int W_MASK = 0x80000000, R_MASK = ~W_MASK;
            std::atomic<int> wr_lock;

            entry_lock() : wr_lock(0) {}
        } __attribute__((aligned(L1_CACHE_LINESIZE)));

        vector<entry_lock> _el;

    public:
        shared_mutex() : _el(std::thread::hardware_concurrency()) {}

        void lock_shared();

        void unlock_shared();

        void lock();

        void unlock();
    };

    class exclusive_lock
    {
    public:
        exclusive_lock(shared_mutex &sm) : _sm(sm) { _sm.lock(); }

        ~exclusive_lock() { _sm.unlock(); }

        void lock() { _sm.lock(); }

        void unlock() { _sm.unlock(); }

    private:
        shared_mutex &_sm;
    };

    class shared_lock
    {
    public:
        shared_lock(shared_mutex &sm) : _sm(sm) { _sm.lock_shared(); }

        ~shared_lock() { _sm.unlock_shared(); }

        void lock() { _sm.lock_shared(); }

        void unlock() { _sm.unlock_shared(); }

    private:
        shared_mutex &_sm;
    };
} // namespace acul