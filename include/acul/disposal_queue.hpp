#pragma once

#include <acul/list.hpp>
#include <oneapi/tbb/concurrent_queue.h>
#include "api.hpp"

namespace acul
{
    struct mem_cache
    {
        std::function<void()> on_free = nullptr;
        mem_cache(const std::function<void()> &on_free) : on_free(on_free) {}
        virtual ~mem_cache() = default;
    };

    template <typename T>
    struct shared_mem_cache : mem_cache
    {
        shared_ptr<T> ptr;

        shared_mem_cache(const shared_ptr<T> &ptr) : mem_cache([]() {}), ptr(ptr) {}
    };

    class APPLIB_API disposal_queue
    {
    public:
        struct mem_data
        {
            acul::list<mem_cache *> cache_list;
            std::function<void()> on_wait = nullptr;
        };

        void push(const mem_data &data) { _queue.push(data); }

        void push(mem_cache *cache, const std::function<void()> &onWait = nullptr)
        {
            _queue.push({{cache}, onWait});
        }

        void flush();

        bool empty() const { return _queue.empty(); }

    private:
        oneapi::tbb::concurrent_queue<mem_data> _queue;
    };
} // namespace acul