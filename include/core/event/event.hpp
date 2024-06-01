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
    extern APPLIB_API class EventManager
    {
    public:
        using iterator = MultiMap<int, std::shared_ptr<BaseEventListener>>::iterator;

        HashMap<void *, DArray<ListenerInfo>> pointers;

        // Adds a listener for a specific type of event and returns an iterator to it.
        template <typename E>
        iterator addListener(const std::string &event, std::function<void(E &)> listener, int priority = 5)
        {
            static_assert(std::is_base_of<Event, E>::value, "E must inherit from Event");
            auto eventListener = std::make_shared<EventListener<E>>(listener);
            _listeners[event].emplace(priority, eventListener);
            return std::prev(_listeners[event].end());
        }

        // Checks if any listener exists for a specific event.
        bool exist(const std::string &event) const { return _listeners.count(event) > 0; }

        // Removes a listener using its iterator.
        void removeListener(iterator listenerIter, const std::string &event);

        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>, typename... Args>
        void dispatch(const std::string &eventName, Args &&...args)
        {
            E event(eventName, std::forward<Args>(args)...);
            dispatch(event);
        }

        // dispatchs an event, invoking all listeners subscribed to this event.
        template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
        void dispatch(E &event)
        {
            auto it = _listeners.find(event.name());
            if (it != _listeners.end())
            {
                for (const auto &pair : it->second)
                {
                    auto listener = std::static_pointer_cast<EventListener<E>>(pair.second);
                    listener->invoke(event);
                }
            }
            else
                _pendingEvents.push(std::move(event));
        }

        template <typename E>
        DArray<std::shared_ptr<EventListener<E>>> getListeners(const std::string &event)
        {
            DArray<std::shared_ptr<EventListener<E>>> result;
            auto it = _listeners.find(event);
            if (it != _listeners.end())
            {
                for (const auto &pair : it->second)
                {
                    auto listener = std::static_pointer_cast<EventListener<E>>(pair.second);
                    result.push_back(listener);
                }
            }
            return result;
        }

        void dispatchPendingEvents()
        {
            while (!_pendingEvents.empty())
            {
                dispatch(_pendingEvents.front());
                _pendingEvents.pop();
            }
        }

    private:
        HashMap<std::string, MultiMap<int, std::shared_ptr<BaseEventListener>>> _listeners;
        Queue<Event> _pendingEvents;
    } mng;

    struct ListenerInfo
    {
        EventManager::iterator listenerIter;
        std::string eventName;
    };

    inline void addListenerID(void *owner, EventManager::iterator id, const std::string &eventName);

    // Binds a listener to an event of a specific type.
    template <typename E = Event, typename = std::enable_if_t<std::is_base_of_v<Event, E>>>
    void bindEvent(void *owner, const std::string &event, std::function<void(E &)> listener, int priority = 5)
    {
        auto id = mng.addListener<E>(event, listener, priority);
        mng.pointers[owner].emplace_back(id, event);
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