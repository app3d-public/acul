#include <acul/disposal_queue.hpp>
#include <cassert>

namespace acul
{
    void disposal_queue::process(mem_data &data)
    {
        if (data.on_wait) data.on_wait();
        for (auto &buffer : data.cache_list)
        {
            if (buffer->on_free) buffer->on_free();
            buffer.reset();
        }
    }

    void disposal_queue::flush_main_queue()
    {
        assert(_guard == queue_count && "Nested disposal queue flush is not supported");
        struct guard_scope
        {
            size_t &value;
            explicit guard_scope(size_t &v) : value(v) {}
            ~guard_scope() { value = queue_count; }
        } guard(_guard);
        while (!is_main_queue_empty())
        {
            for (size_t i = 0; i < queue_count; ++i)
            {
                auto &queue = _main_queues[i];
                if (queue.empty()) continue;
                _guard = i;
                for (auto &data : queue) process(data);
                queue.clear();
            }
        }
    }

    void disposal_queue::flush_mt_queue()
    {
        if (!_mt_queue) return;
        while (true)
        {
            mem_data data;
            if (!_mt_queue->queue.try_pop(data)) break;
            process(data);
        }
    }

    void disposal_queue::discard()
    {
        for (auto &queue : _main_queues) queue.clear();
        if (!_mt_queue) return;
        mem_data data;
        while (_mt_queue->queue.try_pop(data)) data = {};
    }
} // namespace acul
