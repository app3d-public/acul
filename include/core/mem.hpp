#ifndef CORE_MEM_H
#define CORE_MEM_H

#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <queue>

inline size_t alignUp(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

class MemCache
{
public:
    virtual ~MemCache() = default;

    virtual void free() = 0;
};

template <typename T>
class SharedMemCache : public MemCache
{
public:
    SharedMemCache(const std::shared_ptr<T> &ptr) : _ptr(ptr) {}

    virtual void free() override {}

private:
    std::shared_ptr<T> _ptr;
};

class DisposalQueue
{
public:
    static DisposalQueue &getSingleton();

    void push(const std::list<MemCache *> &cache, const std::function<void()> &onWait = nullptr)
    {
        _queue.push({cache, onWait});
    }

    void push(MemCache *cache, const std::function<void()> &onWait = nullptr)
    {
        push(std::list<MemCache *>{cache}, onWait);
    }

    void releaseResources();

    bool empty() const { return _queue.empty(); }

private:
    struct MemData
    {
        std::list<MemCache *> cacheList;
        std::function<void()> onWait = nullptr;
    };
    std::queue<MemData> _queue;
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