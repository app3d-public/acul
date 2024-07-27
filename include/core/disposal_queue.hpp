#pragma once

#include "api.hpp"
#include "std/basic_types.hpp"
#include "std/enum.hpp"
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
    enum class StateBits : u8
    {
        Idle = 0x0,
        MemRelease = 0x1,
        TaskCtx = 0x2
    };

    using State = Flags<StateBits>;

    State state() const { return _state; }

    static APPLIB_API DisposalQueue &global()
    {
        static DisposalQueue queue;
        return queue;
    }

    size_t size() const { return _queue.size(); }

    void push(const List<MemCache *> &cache, const std::function<void()> &onWait = nullptr)
    {
        _state |= StateBits::MemRelease;
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
    State _state = StateBits::Idle;

    DisposalQueue() = default;
};

template <>
struct FlagTraits<DisposalQueue::StateBits>
{
    static constexpr bool isBitmask = true;
    static constexpr DisposalQueue::State allFlags =
        DisposalQueue::StateBits::Idle | DisposalQueue::StateBits::MemRelease | DisposalQueue::StateBits::TaskCtx;
};