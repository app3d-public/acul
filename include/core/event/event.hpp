#ifndef APP_CORE_EVENT_H
#define APP_CORE_EVENT_H

#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include "../std/array.hpp"

namespace events
{
    class IEvent
    {
    public:
        virtual ~IEvent() = default;

        explicit IEvent(const std::string &name = "") : _name(name) {}

        bool operator==(const IEvent &event) const { return event._name == _name; }
        bool operator!=(const IEvent &event) const { return !(*this == event); }

        std::string name() const { return _name; }
        void name(const std::string &name) { _name = name; }

    private:
        std::string _name;
    };

    template <typename T>
    struct Event : IEvent
    {
    public:
        T data;

        explicit Event(const std::string &name = "", T data = T()) : IEvent(name), data(data) {}
    };

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
        explicit EventListener(std::function<void(E &)> listener) : _listener(listener) {}

        // Invokes the listener function with the event.
        void invoke(E &event) { _listener(event); }
    };

    // Manages event listeners and dispatches events to them.
    extern class EventManager
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
            static_assert(std::is_base_of<IEvent, E>::value, "E must inherit from Event");
            auto eventListener = std::make_shared<EventListener<E>>(listener);
            _listeners[event].push_back(eventListener);
            return std::prev(_listeners[event].end());
        }

        // Checks if any listener exists for a specific event.
        bool exist(const std::string &event) const { return _listeners.count(event) > 0; }

        // Removes a listener using its iterator.
        void removeListener(iterator listenerIter, const std::string &event);

        template <typename E = IEvent, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>, typename... Args>
        void emit(const std::string &eventName, Args &&...args)
        {
            E event(eventName, std::forward<Args>(args)...);
            emit(event);
        }

        // Emits an event, invoking all listeners subscribed to this event.
        template <typename E = IEvent, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>>
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

        template <typename E, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>>
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
    } mng;

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
        virtual void bindListeners() = 0;

        // Unbinds all listeners managed by this registry from the event manager.
        void unbindListeners();

        // Binds a listener to an event of a specific type.
        template <typename E = IEvent, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>>
        void bindEvent(const std::string &event, std::function<void(E &)> listener)
        {
            auto id = mng.addListener<E>(event, listener);
            addListenerID(id, event);
        }

        // Binds a listener to a list of events of a specific type.
        template <typename E = IEvent, typename F, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>>
        void bindEvent(std::initializer_list<std::string> events, F listener)
        {
            for (const auto &event : events)
                bindEvent<E>(event, listener);
        }

    private:
        Array<ListenerInfo> _listeners;
    };
} // namespace events

namespace std
{
    template <>
    struct hash<events::IEvent>
    {
        size_t operator()(const events::IEvent &event) const { return hash<string>{}(event.name()); }
    };
} // namespace std

#endif // APP_VIEWPORT_EVENT_H