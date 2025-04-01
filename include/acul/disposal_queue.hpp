#pragma once

#include <acul/list.hpp>
#include <oneapi/tbb/concurrent_queue.h>
#include "api.hpp"

namespace acul
{
    class APPLIB_API mem_cache
    {
    public:
        virtual ~mem_cache() = default;

        virtual void free() = 0;
    };

    template <typename T>
    class shared_mem_cache : public mem_cache
    {
    public:
        shared_mem_cache(const acul::shared_ptr<T> &ptr) : _ptr(ptr) {}

        virtual void free() override {}

    private:
        acul::shared_ptr<T> _ptr;
    };

    class APPLIB_API disposal_queue
    {
    public:
        void push(const acul::list<mem_cache *> &cache, const std::function<void()> &onWait = nullptr)
        {
            _queue.push({cache, onWait});
        }

        void push(mem_cache *cache, const std::function<void()> &onWait = nullptr) { _queue.push({{cache}, onWait}); }

        void flush();

        bool empty() const { return _queue.empty(); }

    private:
        struct mem_data
        {
            acul::list<mem_cache *> cache_list;
            std::function<void()> on_wait = nullptr;
        };
        oneapi::tbb::concurrent_queue<mem_data> _queue;
    };
} // namespace acul