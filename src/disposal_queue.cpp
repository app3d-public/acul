#include <acul/disposal_queue.hpp>
#include <cassert>
#include <oneapi/tbb/concurrent_queue.h>

namespace acul
{
    struct disposal_queue::mt_disposal_queue
    {
        oneapi::tbb::concurrent_queue<mem_data> queue;
    };

    disposal_queue::~disposal_queue()
    {
        flush();
        if (_mt_queue) acul::release(_mt_queue);
    }

    void disposal_queue::enable_mt()
    {
        if (!_mt_queue) _mt_queue = acul::alloc<mt_disposal_queue>();
    }

    void disposal_queue::push_mt(mem_data &&data)
    {
        assert(_mt_queue && "MT disposal queue is not enabled");
        _mt_queue->queue.push(std::move(data));
    }

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

    bool disposal_queue::is_mt_queue_empty() const
    {
        return !_mt_queue || _mt_queue->queue.empty();
    }
} // namespace acul
