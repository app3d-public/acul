#pragma once

#include <oneapi/tbb/concurrent_queue.h>
#include "api.hpp"
#include "std/list.hpp"

class APPLIB_API MemCache
{
public:
    virtual ~MemCache() = default;

    virtual void free() = 0;
};

template <typename T>
class APPLIB_API SharedMemCache : public MemCache
{
public:
    SharedMemCache(const std::shared_ptr<T> &ptr) : _ptr(ptr) {}

    virtual void free() override {}

private:
    std::shared_ptr<T> _ptr;
};

class APPLIB_API DisposalQueue
{
public:
    void push(const List<MemCache *> &cache, const std::function<void()> &onWait = nullptr)
    {
        _queue.push({cache, onWait});
    }

    void push(MemCache *cache, const std::function<void()> &onWait = nullptr) { push(List<MemCache *>{cache}, onWait); }

    void flush();

    bool empty() const { return _queue.empty(); }

private:
    struct MemData
    {
        List<MemCache *> cacheList;
        std::function<void()> onWait = nullptr;
    };
    oneapi::tbb::concurrent_queue<MemData> _queue;
};