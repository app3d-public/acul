#ifndef APP_CORE_EVENT_H
#define APP_CORE_EVENT_H

#include <functional>
#include <string>
#include <unordered_map>
#include "../std/array.hpp"

class Event
{
public:
    virtual ~Event() = default;

    Event(const std::string &name = "") : _name(name) {}

    bool operator==(const Event &event) const { return event._name == _name; }
    bool operator!=(const Event &event) const { return !(*this == event); }

    std::string name() const { return _name; }
    void name(const std::string &name) { _name = name; }

private:
    std::string _name;
};

namespace std
{
    template <>
    struct hash<Event>
    {
        size_t operator()(const Event &event) const { return hash<string>{}(event.name()); }
    };
} // namespace std

// Base class for event listeners.
class BaseEventListener
{
public:
    virtual ~BaseEventListener() = default;
};

// Template class for listeners specific to a type of event.
template <typename E>
class EventListener : public BaseEventListener
{
    std::function<void(E &)> _listener;

public:
    EventListener(std::function<void(E &)> listener) : _listener(listener) {}

    // Invokes the listener function with the event.
    void invoke(E &event) { _listener(event); }
};

// Manages event listeners and dispatches events to them.
class EventManager
{
public:
    using iterator = Array<std::shared_ptr<BaseEventListener>>::iterator;

private:
    std::unordered_map<std::string, Array<std::shared_ptr<BaseEventListener>>> _listeners;

public:
    // Adds a listener for a specific type of event and returns an iterator to it.
    template <typename E>
    iterator addListener(const std::string &event, std::function<void(E &)> listener)
    {
        static_assert(std::is_base_of<Event, E>::value, "E must inherit from Event");
        auto eventListener = std::make_shared<EventListener<E>>(listener);
        _listeners[event].push_back(eventListener);
        return std::prev(_listeners[event].end());
    }

    // Checks if any listener exists for a specific event.
    bool exist(const std::string &event) const { return _listeners.count(event) > 0; }

    // Removes a listener using its iterator.
    void removeListener(iterator listenerIter, const std::string &event)
    {
        auto it = _listeners.find(event);
        if (it != _listeners.end())
            it->second.erase(listenerIter);
    }

    // Emits an event by its name.
    void emit(const std::string &eventName)
    {
        Event event(eventName);
        emit(event);
    }

    // Emits an event, invoking all listeners subscribed to this event.
    template <typename E, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
    void emit(E &event)
    {
        auto it = _listeners.find(event.name());
        if (it != _listeners.end())
        {
            for (const auto &baseListener : it->second)
            {
                auto listener = std::static_pointer_cast<EventListener<E>>(baseListener);
                listener->invoke(event);
            }
        }
    }

    template <typename E, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
    Array<std::shared_ptr<EventListener<E>>> getListeners(const std::string &event)
    {
        Array<std::shared_ptr<EventListener<E>>> result;
        auto it = _listeners.find(event);
        if (it != _listeners.end())
        {
            for (const auto &baseListener : it->second)
            {
                auto listener = std::static_pointer_cast<EventListener<E>>(baseListener);
                result.push_back(listener);
            }
        }
        return result;
    }
};

struct ListenerInfo
{
    EventManager::iterator listenerIter;
    std::string eventName;
};

// Abstract base class for registries that manage event listener bindings.
class ListenerRegistry
{
public:
    virtual ~ListenerRegistry() = default;

    // Adds an iterator to a listener to the internal collection.
    void addListenerID(EventManager::iterator id, const std::string &eventName)
    {
        _listeners.push_back(ListenerInfo(id, eventName));
    }
    // Pure virtual method to bind listeners to events. Must be implemented by
    // derived classes.
    virtual void bindListeners(EventManager &eventManager) = 0;

    // Unbinds all listeners managed by this registry from the event manager.
    void unbindListeners(EventManager &eventManager)
    {
        for (auto &info : _listeners)
            eventManager.removeListener(info.listenerIter, info.eventName);
        _listeners.clear();
    }

private:
    Array<ListenerInfo> _listeners;
};

// Helper class to bind listeners to events in an EventManager.
class ListenerBinder
{
private:
    EventManager &_eventManager;
    ListenerRegistry &_registry;

public:
    ListenerBinder(EventManager &eventManager, ListenerRegistry &registry)
        : _eventManager(eventManager), _registry(registry)
    {
    }

    // Binds a listener to an event of a specific type.
    template <typename E = Event, typename F, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
    void bind(const std::string &event, F listener)
    {
        auto id = _eventManager.addListener<E>(event, listener);
        _registry.addListenerID(id, event);
    }

    // Binds a listener to an event by its name.
    template <typename E, typename F>
    void bind(const std::string &event, F listener)
    {
        auto id = _eventManager.addListener<E>(event, listener);
        _registry.addListenerID(id, event);
    }

    // Binds a listener to a list of events of a specific type.
    template <typename TEvent>
    void bindList(const Array<std::string> &eventNames, std::function<void(TEvent &)> listener)
    {
        for (const auto &eventName : eventNames)
            bind(eventName, listener);
    }
};

#endif // APP_VIEWPORT_EVENT_H