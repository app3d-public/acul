#pragma once

#include <oneapi/tbb/concurrent_queue.h>
#include "api.hpp"
#include "astl/list.hpp"

class APPLIB_API MemCache
{
public:
    virtual ~MemCache() = default;

    virtual void free() = 0;
};

template <typename T>
class SharedMemCache : public MemCache
{
public:
    SharedMemCache(const astl::shared_ptr<T> &ptr) : _ptr(ptr) {}

    virtual void free() override {}

private:
    astl::shared_ptr<T> _ptr;
};

class APPLIB_API DisposalQueue
{
public:
    void push(const astl::list<MemCache *> &cache, const std::function<void()> &onWait = nullptr)
    {
        _queue.push({cache, onWait});
    }

    void push(MemCache *cache, const std::function<void()> &onWait = nullptr)
    {
        push(astl::list<MemCache *>{cache}, onWait);
    }

    void flush();

    bool empty() const { return _queue.empty(); }

private:
    struct MemData
    {
        astl::list<MemCache *> cacheList;
        std::function<void()> onWait = nullptr;
    };
    oneapi::tbb::concurrent_queue<MemData> _queue;
};