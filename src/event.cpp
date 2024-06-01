#include <cassert>
#include <core/event/event.hpp>

namespace events
{
    void EventManager::removeListener(BaseEventListener *listener, const std::string &event)
    {
        auto it = _listeners.find(event);
        if (it != _listeners.end())
        {
            auto &listeners = it->second;
            for (auto iter = listeners.begin(); iter != listeners.end();)
            {
                if (iter->second == listener)
                {
                    delete iter->second;
                    iter = listeners.erase(iter);
                }
                else
                    ++iter;
            }
        }
    }

    EventManager mng{};

    void unbindListeners(void *owner)
    {
        for (auto &info : mng.pointers[owner]) mng.removeListener(info.listener, info.eventName);
        mng.pointers.erase(owner);
    }

     void EventManager::clear()
    {
        for (auto &eventPair : _listeners)
        {
            auto &listeners = eventPair.second;
            for (auto &listenerPair : listeners) delete listenerPair.second;
            listeners.clear();
        }
        _listeners.clear(); // Очищаем все события
    }

} // namespace events