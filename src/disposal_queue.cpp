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
        assert(!_guard && "Nested disposal queue flush is not supported");
        struct guard_scope
        {
            bool &value;
            explicit guard_scope(bool &v) : value(v) { value = true; }
            ~guard_scope() { value = false; }
        } guard(_guard);
        for (auto &data : _main_queue) process(data);
        _main_queue.clear();
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
        _main_queue.clear();
        if (!_mt_queue) return;
        mem_data data;
        while (_mt_queue->queue.try_pop(data)) data = {};
    }
} // namespace acul
