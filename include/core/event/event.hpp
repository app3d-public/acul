#ifndef APP_CORE_EVENT_H
#define APP_CORE_EVENT_H

#include <any>
#include <cassert>
#include <core/api.hpp>
#include <core/std/basic_types.hpp>
#include <core/std/darray.hpp>
#include <functional>
#include <initializer_list>
#include <string>

namespace events
{
    struct Event
    {
    public:
        explicit Event(const std::string &name = "", std::any data = {}) : _name(name), _data(std::move(data)) {}

        virtual ~Event() = default;

        template <typename T>
        T data()
        {
            assert(_data.type() == typeid(T));
            return std::any_cast<T>(_data);
        }

        template <typename T>
        T data() const
        {
            assert(_data.type() == typeid(T));
            return std::any_cast<T>(_data);
        }

        void data(std::any data) { _data = std::move(data); }

        bool operator==(const Event &event) const { return event._name == _name; }
        bool operator!=(const Event &event) const { return !(*this == event); }

        std::string name() const { return _name; }
        void name(const std::string &name) { _name = name; }

    protected:
        std::string _name;
        std::any _data;
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
    extern APPLIB_API class EventManager
    {
    public:
        using iterator = DArray<std::shared_ptr<BaseEventListener>>::iterator;

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
        void removeListener(iterator listenerIter, const std::string &event);

        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>, typename... Args>
        void emit(const std::string &eventName, Args &&...args)
        {
            E event(eventName, std::forward<Args>(args)...);
            emit(event);
        }

        // Emits an event, invoking all listeners subscribed to this event.
        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
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
            else
                _pendingEvents.push(std::move(event));
        }

        template <typename E, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
        DArray<std::shared_ptr<EventListener<E>>> getListeners(const std::string &event)
        {
            DArray<std::shared_ptr<EventListener<E>>> result;
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

        void dispatchPendingEvents()
        {
            while (!_pendingEvents.empty())
            {
                emit(_pendingEvents.front());
                _pendingEvents.pop();
            }
        }

    private:
        HashMap<std::string, DArray<std::shared_ptr<BaseEventListener>>> _listeners;
        Queue<Event> _pendingEvents;
    } mng;

    struct ListenerInfo
    {
        EventManager::iterator listenerIter;
        std::string eventName;
    };

    // Abstract base class for registries that manage event listener bindings.
    class APPLIB_API ListenerRegistry
    {
    public:
        virtual ~ListenerRegistry() { unbindListeners(); }

        // Adds an iterator to a listener to the internal collection.
        void addListenerID(EventManager::iterator id, const std::string &eventName)
        {
            _listeners.push_back(ListenerInfo(id, eventName));
        }

        // Unbinds all listeners managed by this registry from the event manager.
        void unbindListeners();

        // Binds a listener to an event of a specific type.
        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
        void bindEvent(const std::string &event, std::function<void(E &)> listener)
        {
            auto id = mng.addListener<E>(event, listener);
            addListenerID(id, event);
        }

        // Binds a listener to a list of events of a specific type.
        template <typename E = Event, typename F, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
        void bindEvent(std::initializer_list<std::string> events, F listener)
        {
            for (const auto &event : events) bindEvent<E>(event, listener);
        }

    private:
        DArray<ListenerInfo> _listeners;
    };
} // namespace events

namespace std
{
    template <>
    struct hash<events::Event>
    {
        size_t operator()(const events::Event &event) const { return hash<string>{}(event.name()); }
    };
} // namespace std

#endif // APP_VIEWPORT_EVENT_H