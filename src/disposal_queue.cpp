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
        for (auto &data : _main_queue) process(data);
        _main_queue.clear();
    }

    void disposal_queue::flush_mt_queue()
    {
        while (true)
        {
            mem_data data;
            if (!_mt_queue.try_pop(data)) break;
            process(data);
        }
    }
} // namespace acul
