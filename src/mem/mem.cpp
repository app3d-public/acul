#include <core/mem/mem.hpp>

DisposalQueue &DisposalQueue::global()
{
    static DisposalQueue instance;
    return instance;
}

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
}