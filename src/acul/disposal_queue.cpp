#include <acul/disposal_queue.hpp>
#include <cassert>

namespace acul
{
    void disposal_queue::flush()
    {
        for (size_t i = 0; i < _queue.unsafe_size(); ++i)
        {
            mem_data data;
            if (!_queue.try_pop(data)) break;

            if (data.allow && !data.allow())
            {
                _queue.push(std::move(data));
                continue;
            }
            if (data.on_wait) data.on_wait();

            for (auto &buffer : data.cache_list)
            {
                buffer->on_free();
                acul::release(buffer);
            }
        }
    }
} // namespace acul