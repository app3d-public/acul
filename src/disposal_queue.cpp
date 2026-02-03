#include <acul/disposal_queue.hpp>
#include <cassert>

namespace acul
{
    void disposal_queue::flush()
    {
        while (true)
        {
            mem_data data;
            if (!_queue.try_pop(data)) break;
            if (data.on_wait) data.on_wait();

            for (auto &buffer : data.cache_list)
            {
                if (buffer->on_free) buffer->on_free();
                buffer.reset();
            }
        }
    }
} // namespace acul