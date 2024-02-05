#include <core/event/event.hpp>

namespace events
{
    void EventManager::removeListener(iterator listenerIter, const std::string &event)
    {
        auto it = _listeners.find(event);
        if (it != _listeners.end())
            it->second.erase(listenerIter);
    }

    EventManager mng{};

    void ListenerRegistry::unbindListeners()
    {
        for (const auto &info : _listeners)
            mng.removeListener(info.listenerIter, info.eventName);
        _listeners.clear();
    }
} // namespace events