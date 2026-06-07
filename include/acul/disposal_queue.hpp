#pragma once

#include "api.hpp"
#include "functional/unique_function.hpp"
#include "list.hpp"
#include "memory/smart_ptr.hpp"
#include "vector.hpp"

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

        ~disposal_queue();

        void enable_mt();

        inline void push(mem_data &&data)
        {
            if (_guard) process(data);
            else _main_queue.push_back(std::move(data));
        }

        void push_mt(mem_data &&data);

        void push(unique_ptr<mem_cache> cache)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            push(std::move(d));
        }

        void push_mt(unique_ptr<mem_cache> cache)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            push_mt(std::move(d));
        }

        template <class F>
        void emplace(F &&f)
        {
            mem_data d;
            d.cache_list.push_back(make_unique<mem_cache>(std::forward<F>(f)));
            push(std::move(d));
        }

        template <class F>
        void emplace_mt(F &&f)
        {
            mem_data d;
            d.cache_list.push_back(make_unique<mem_cache>(std::forward<F>(f)));
            push_mt(std::move(d));
        }

        template <class F>
        void push(unique_ptr<mem_cache> cache, F &&func)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            d.on_wait = std::forward<F>(func);
            push(std::move(d));
        }

        template <class F>
        void push_mt(unique_ptr<mem_cache> cache, F &&func)
        {
            mem_data d;
            d.cache_list.push_back(std::move(cache));
            d.on_wait = std::forward<F>(func);
            push_mt(std::move(d));
        }

        void flush_main_queue();

        void flush_mt_queue();

        void discard();

        inline void flush()
        {
            if (!_main_queue.empty()) flush_main_queue();
            if (!is_mt_queue_empty()) flush_mt_queue();
        }

        bool is_main_queue_empty() const { return _main_queue.empty(); }
        bool is_mt_queue_empty() const;
        
    private:
        struct mt_disposal_queue;

        void process(mem_data &data);

        vector<mem_data> _main_queue;
        bool _guard = false;
        mt_disposal_queue *_mt_queue = nullptr;
    };
} // namespace acul
