#pragma once

#include <oneapi/tbb/concurrent_queue.h>
#include "api.hpp"
#include "functional/unique_function.hpp"
#include "list.hpp"
#include "memory/smart_ptr.hpp"

namespace acul
{
    struct mem_cache
    {
        unique_function<void()> on_free = nullptr;

        template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, mem_cache>>>
        explicit mem_cache(F &&func) : on_free(std::forward<F>(func))
        {
        }

        mem_cache() = default;
        virtual ~mem_cache() = default;
    };

    template <typename T>
    struct shared_mem_cache : mem_cache
    {
        shared_ptr<T> ptr;

        explicit shared_mem_cache(shared_ptr<T> p) : ptr(std::move(p)) {}
    };

    class APPLIB_API disposal_queue
    {
    public:
        struct mem_data
        {
            list<unique_ptr<mem_cache>> cache_list;
            unique_function<void()> on_wait = nullptr;
        };

        void push(mem_data &&data) { _queue.push(std::move(data)); }

        void push(unique_ptr<mem_cache> cache)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            _queue.push(std::move(d));
        }

        template <class F>
        void emplace(F &&f)
        {
            mem_data d;
            d.cache_list.push_back(make_unique<mem_cache>(std::forward<F>(f)));
            _queue.push(std::move(d));
        }

        template <class F>
        void push(unique_ptr<mem_cache> cache, F &&func)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            d.on_wait = std::forward<F>(func);
            _queue.push(std::move(d));
        }

        void flush();

        bool empty() const { return _queue.empty(); }

    private:
        oneapi::tbb::concurrent_queue<mem_data> _queue;
    };
} // namespace acul