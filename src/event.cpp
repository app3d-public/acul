#include <core/event/event.hpp>

namespace events
{
    void EventManager::removeListener(iterator listenerIter, const std::string &event)
    {
        auto it = _listeners.find(event);
        if (it != _listeners.end()) it->second.erase(listenerIter);
    }

    EventManager mng{};

    void unbindListeners(void *owner)
    {
        for (const auto &info : mng.pointers[owner]) mng.removeListener(info.listenerIter, info.eventName);
        mng.pointers.erase(owner);
    }
} // namespace events