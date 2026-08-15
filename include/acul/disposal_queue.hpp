#pragma once

#include <cstddef>
#include <oneapi/tbb/concurrent_queue.h>
#include "functional/unique_function.hpp"
#include "list.hpp"
#include "memory/smart_ptr.hpp"
#include "vector.hpp"

namespace acul
{
    namespace detail
    {
        struct mt_disposal_queue;
    } // namespace detail

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

    class disposal_queue
    {
    public:
        struct mem_data
        {
            list<unique_ptr<mem_cache>> cache_list;
            unique_function<void()> on_wait = nullptr;
        };

        ~disposal_queue()
        {
            flush();
            if (_mt_queue) acul::release(_mt_queue);
        }

        inline void enable_mt()
        {
            if (!_mt_queue) _mt_queue = acul::alloc<detail::mt_disposal_queue>();
        }
        // Forced work is queued for the next flush wave instead of executing
        // reentrantly when push is called from a queue callback.
        inline void push(mem_data &&data, bool force = false)
        {
            const bool guarded = _guard < queue_count;
            if (guarded && !force) process(data);
            else _main_queues[guarded ? (_guard ^ 1u) : 0u].push_back(std::move(data));
        }
        inline void push_mt(mem_data &&data);

        void push(unique_ptr<mem_cache> cache, bool force = false)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            push(std::move(d), force);
        }

        void push_mt(unique_ptr<mem_cache> cache)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            push_mt(std::move(d));
        }

        template <class F>
        void emplace(F &&f, bool force = false)
        {
            mem_data d;
            d.cache_list.push_back(make_unique<mem_cache>(std::forward<F>(f)));
            push(std::move(d), force);
        }

        template <class F>
        void emplace_mt(F &&f)
        {
            mem_data d;
            d.cache_list.push_back(make_unique<mem_cache>(std::forward<F>(f)));
            push_mt(std::move(d));
        }

        template <class F>
        void push(unique_ptr<mem_cache> cache, F &&func, bool force = false)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            d.on_wait = std::forward<F>(func);
            push(std::move(d), force);
        }

        template <class F>
        void push_mt(unique_ptr<mem_cache> cache, F &&func)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            d.on_wait = std::forward<F>(func);
            push_mt(std::move(d));
        }

        ACUL_EXPORT void flush_main_queue();
        ACUL_EXPORT void flush_mt_queue();
        ACUL_EXPORT void discard();

        ACUL_FORCEINLINE void flush()
        {
            if (!is_main_queue_empty()) flush_main_queue();
            if (!is_mt_queue_empty()) flush_mt_queue();
        }

        bool is_main_queue_empty() const { return _main_queues[0].empty() && _main_queues[1].empty(); }
        inline bool is_mt_queue_empty() const;
        bool is_empty() const { return is_main_queue_empty() && is_mt_queue_empty(); }

    private:
        ACUL_EXPORT void process(mem_data &data);

        static constexpr size_t queue_count = 2u;
        vector<mem_data> _main_queues[queue_count];
        // queue_count means idle; 0/1 identifies the queue currently being processed.
        size_t _guard = queue_count;
        detail::mt_disposal_queue *_mt_queue = nullptr;
    };

    namespace detail
    {
        struct mt_disposal_queue
        {
            oneapi::tbb::concurrent_queue<disposal_queue::mem_data> queue;
        };
    } // namespace detail

    inline bool disposal_queue::is_mt_queue_empty() const { return !_mt_queue || _mt_queue->queue.empty(); }
    inline void disposal_queue::push_mt(mem_data &&data)
    {
        assert(_mt_queue && "MT disposal queue is not enabled");
        _mt_queue->queue.push(std::move(data));
    }
} // namespace acul
