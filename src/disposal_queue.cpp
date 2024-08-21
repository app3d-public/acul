#include <core/disposal_queue.hpp>

void DisposalQueue::releaseResources()
{
    while (!_queue.empty())
    {
        MemData data;
        _queue.try_pop(data);
        if (data.onWait) data.onWait();
        for (auto &buffer : data.cacheList)
        {
            buffer->free();
            delete buffer;
        }
    }
}