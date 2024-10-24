#include <cassert>
#include <core/disposal_queue.hpp>

void DisposalQueue::flush()
{
    while (!_queue.empty())
    {
        MemData data;
        _queue.try_pop(data);
        if (data.onWait) data.onWait();
        for (auto &buffer : data.cacheList)
        {
            assert(buffer);
            buffer->free();
            astl::release(buffer);
        }
    }
}