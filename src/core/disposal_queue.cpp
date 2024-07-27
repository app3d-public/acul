#include <core/disposal_queue.hpp>

void DisposalQueue::releaseResources()
{
    while (!_queue.empty())
    {
        auto &data = _queue.front();
        if (data.onWait) data.onWait();
        for (auto &buffer : data.cacheList)
        {
            buffer->free();
            delete buffer;
        }
        _queue.pop();
    }
    _state &= ~StateBits::MemRelease;
}