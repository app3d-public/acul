#include <acul/disposal_queue.hpp>
#include <cassert>

namespace acul
{
    void disposal_queue::flush()
    {
        while (!_queue.empty())
        {
            mem_data data;
            _queue.try_pop(data);
            if (data.on_wait) data.on_wait();
            for (auto &buffer : data.cache_list)
            {
                assert(buffer);
                buffer->free();
                acul::release(buffer);
            }
        }
    }
} // namespace acul