#ifndef CORE_MEM_H
#define CORE_MEM_H

#include <core/api.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include "../std/basic_types.hpp"
#include "../std/list.hpp"

inline size_t alignUp(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

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
    static APPLIB_API DisposalQueue &global();

    void push(const List<MemCache *> &cache, const std::function<void()> &onWait = nullptr)
    {
        _queue.push({cache, onWait});
    }

    void push(MemCache *cache, const std::function<void()> &onWait = nullptr) { push(List<MemCache *>{cache}, onWait); }

    void releaseResources();

    bool empty() const { return _queue.empty(); }

private:
    struct MemData
    {
        List<MemCache *> cacheList;
        std::function<void()> onWait = nullptr;
    };
    Queue<MemData> _queue;
};

template <typename T>
class Proxy
{
public:
    Proxy(const std::shared_ptr<T> &ptr = nullptr) : _ptr(ptr) {}

    T *operator->() { return _ptr.get(); }

    T &operator*() { return *_ptr; }

    operator bool() const { return _ptr != nullptr; }

    void set(const std::shared_ptr<T> &ptr) { _ptr = ptr; }

    std::shared_ptr<T> &get() { return _ptr; }

    void reset() { _ptr.reset(); }

private:
    std::shared_ptr<T> _ptr;
};

#endif