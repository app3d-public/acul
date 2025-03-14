#pragma once

#include <acul/list.hpp>
#include <oneapi/tbb/concurrent_queue.h>
#include "api.hpp"

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
    SharedMemCache(const acul::shared_ptr<T> &ptr) : _ptr(ptr) {}

    virtual void free() override {}

private:
    acul::shared_ptr<T> _ptr;
};

class APPLIB_API DisposalQueue
{
public:
    void push(const acul::list<MemCache *> &cache, const std::function<void()> &onWait = nullptr)
    {
        _queue.push({cache, onWait});
    }

    void push(MemCache *cache, const std::function<void()> &onWait = nullptr) { _queue.push({{cache}, onWait}); }

    void flush();

    bool empty() const { return _queue.empty(); }

private:
    struct MemData
    {
        acul::list<MemCache *> cacheList;
        std::function<void()> onWait = nullptr;
    };
    oneapi::tbb::concurrent_queue<MemData> _queue;
};