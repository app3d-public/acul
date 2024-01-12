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

    // Event this listener is subscribed to.
    Event listenedEvent;
};

// Template class for listeners specific to a type of event.
template <typename TEvent>
class EventListener : public BaseEventListener
{
    std::function<void(TEvent &)> _listener;

public:
    EventListener(std::function<void(TEvent &)> listener) : _listener(listener) {}

    // Invokes the listener function with the event.
    void invoke(TEvent &event) { _listener(event); }
};

// Manages event listeners and dispatches events to them.
class EventManager
{
public:
    using ListenerIterator = Array<std::shared_ptr<BaseEventListener>>::iterator;

private:
    std::unordered_map<Event, Array<std::shared_ptr<BaseEventListener>>> _listeners;

public:
    // Adds a listener for a specific type of event and returns an iterator to it.
    template <typename TEvent>
    ListenerIterator addListener(const TEvent &event, std::function<void(TEvent &)> listener)
    {
        static_assert(std::is_base_of<Event, TEvent>::value, "TEvent must inherit from Event");
        auto eventListener = std::make_shared<EventListener<TEvent>>(listener);
        eventListener->listenedEvent = event;
        _listeners[event].push_back(eventListener);
        return std::prev(_listeners[event].end()); // Возвращаем итератор на только что добавленный слушатель
    }

    // Checks if any listener exists for a specific event.
    bool exist(const Event &event) const { return _listeners.count(event) > 0; }

    // Removes a listener using its iterator.
    void removeListener(ListenerIterator listenerIter)
    {
        auto it = _listeners.find((*listenerIter)->listenedEvent);
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
    template <typename TEvent, typename = std::enable_if_t<std::is_base_of_v<Event, TEvent>>>
    void emit(TEvent &event)
    {
        auto it = _listeners.find(event);
        if (it != _listeners.end())
        {
            for (const auto &baseListener : it->second)
            {
                auto listener = std::static_pointer_cast<EventListener<TEvent>>(baseListener);
                listener->invoke(event);
            }
        }
    }
};

// Abstract base class for registries that manage event listener bindings.
class ListenerRegistry
{
public:
    virtual ~ListenerRegistry() = default;

    // Adds an iterator to a listener to the internal collection.
    void addListenerID(EventManager::ListenerIterator id) { _listeners.push_back(id); }

    // Pure virtual method to bind listeners to events. Must be implemented by
    // derived classes.
    virtual void bindListeners(EventManager &eventManager) = 0;

    // Unbinds all listeners managed by this registry from the event manager.
    void unbindListeners(EventManager &eventManager)
    {
        // logInfo("Unbinding listeners");
        for (auto &id : _listeners)
            eventManager.removeListener(id);
        _listeners.clear();
    }

private:
    Array<EventManager::ListenerIterator> _listeners;
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
    template <typename TEvent, typename TFunc, typename = std::enable_if_t<std::is_base_of_v<Event, TEvent>>>
    void bind(const TEvent &event, TFunc listener)
    {
        auto id = _eventManager.addListener<TEvent>(event, listener);
        _registry.addListenerID(id);
    }

    // Binds a listener to an event by its name.
    template <typename TFunc>
    void bind(const std::string &eventName, TFunc listener)
    {
        Event event(eventName);
        auto id = _eventManager.addListener<Event>(event, listener);
        _registry.addListenerID(id);
    }

    // Binds a listener to a list of events of a specific type.
    template <typename TEvent>
    void bindList(const Array<std::string> &eventNames, std::function<void(TEvent &)> listener)
    {
        for (const auto &eventName : eventNames)
            bind(TEvent(eventName), listener);
    }
};

#endif // APP_VIEWPORT_EVENT_H