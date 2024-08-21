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

    struct ListenerInfo;

    // Manages event listeners and dispatches events to them.
    extern APPLIB_API class APPLIB_API EventManager
    {
    public:
        using iterator = MultiMap<int, BaseEventListener *>::iterator;

        HashMap<void *, DArray<ListenerInfo>> pointers;

        // Adds a listener for a specific type of event and returns an iterator to it.
        template <typename E>
        iterator addListener(const std::string &event, std::function<void(E &)> listener, int priority = 5)
        {
            static_assert(std::is_base_of<Event, E>::value, "E must inherit from Event");
            auto eventListener = new EventListener<E>(listener);
            _listeners[event].emplace(priority, eventListener);
            return std::prev(_listeners[event].end());
        }

        // Checks if any listener exists for a specific event.
        bool exist(const std::string &event) const { return _listeners.count(event) > 0; }

        // Removes a listener using its iterator.
        void removeListener(BaseEventListener *listener, const std::string &event);

        // Dispatches an event by name, creating an event instance with the provided arguments.
        // Parameters:
        // - eventName: The name of the event to dispatch.
        // - args: Arguments to pass to the event constructor.
        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>, typename... Args>
        void dispatch(const std::string &eventName, Args &&...args)
        {
            E event(eventName, std::forward<Args>(args)...);
            dispatch(event);
        }

        // Dispatchs an event, invoking all listeners subscribed to this event.
        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
        void dispatch(E &event)
        {
            auto it = _listeners.find(event.name());
            if (it != _listeners.end())
            {
                for (const auto &pair : it->second)
                {
                    auto listener = static_cast<EventListener<E> *>(pair.second);
                    listener->invoke(event);
                }
            }
            else
                _pendingEvents.push(std::move(event));
        }

        // Retrieves a list of all listeners registered for a specific event.
        // Returns a dynamic array of pointers to listeners for the event.
        template <typename E>
        DArray<EventListener<E> *> getListeners(const std::string &event)
        {
            DArray<EventListener<E> *> result;
            auto it = _listeners.find(event);
            if (it != _listeners.end())
            {
                for (const auto &pair : it->second)
                {
                    auto listener = static_cast<EventListener<E> *>(pair.second);
                    result.push_back(listener);
                }
            }
            return result;
        }

        // Processes all events that are pending in the queue.
        void dispatchPendingEvents()
        {
            while (!_pendingEvents.empty())
            {
                dispatch(_pendingEvents.front());
                _pendingEvents.pop();
            }
        }

        // Clears all registered listeners from the manager, ensuring all memory is properly freed.
        void clear();

    private:
        HashMap<std::string, MultiMap<int, BaseEventListener *>> _listeners;
        Queue<Event> _pendingEvents;
    } mng;

    struct ListenerInfo
    {
        BaseEventListener *listener;
        std::string eventName;
    };

    // Binds a listener to an event of a specific type.
    template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
    void bindEvent(void *owner, const std::string &event, std::function<void(E &)> listener, int priority = 5)
    {
        auto id = mng.addListener<E>(event, listener, priority);
        mng.pointers[owner].emplace_back(id->second, event);
    }

    // Binds a listener to a list of events of a specific type.
    template <typename E = Event, typename F, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
    void bindEvent(void *owner, std::initializer_list<std::string> events, F listener, int priority = 5)
    {
        for (const auto &event : events) bindEvent<E>(owner, event, listener, priority);
    }

    /**
    ** Unbinds all listeners managed by this registry from the event manager.
    ** \param owner The owner of the listeners.
    */
    APPLIB_API void unbindListeners(void *owner);
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