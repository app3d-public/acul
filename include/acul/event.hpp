#ifndef APP_CORE_EVENT_H
#define APP_CORE_EVENT_H

#include <cassert>
#include <functional>
#include "../astl/hash.hpp"
#include "../astl/map.hpp"
#include "../astl/type_traits.hpp"
#include "../astl/vector.hpp"
#include "api.hpp"

namespace events
{
    struct IEvent
    {
        // unsigned 62 number for id
        // Other bits reserved for pre & post events
        u64 id;

        explicit IEvent(u64 id = 0) : id(id) {}

        virtual ~IEvent() = default;

        bool operator==(const IEvent &event) const { return event.id == id; }
        bool operator!=(const IEvent &event) const { return !(*this == event); }
    };

    inline u64 makePreEventID(u64 id) { return id | 0x4000000000000000ULL; }

    inline u64 makePostEventID(u64 id) { return id | 0x8000000000000000ULL; }

    template <typename T>
    struct Event final : IEvent
    {
    public:
        T data;

        template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, Event>>>
        explicit Event(u64 id, U &&data) : IEvent(id), data(std::forward<U>(data))
        {
        }

        explicit Event(u64 id) : IEvent(id), data() {}
    };

    using PointerEvent = Event<IEvent *>;

    // Base class for event listeners.
    class BaseEventListener
    {
    public:
        virtual ~BaseEventListener() = default;
    };

    // Template class for listeners specific to a type of event.
    template <typename E>
    class EventListener final : public BaseEventListener
    {
        std::function<void(E &)> _listener;

    public:
        explicit EventListener(std::function<void(E &)> listener) : _listener(listener) {}

        // Invokes the listener function with the event.
        void invoke(E &event) { _listener(event); }
    };

    struct ListenerInfo
    {
        BaseEventListener *listener;
        u64 id;
    };
    // Manages event listeners and dispatches events to them.
    class APPLIB_API Manager
    {
    public:
        using iterator = astl::multimap<int, BaseEventListener *>::iterator;
        astl::hashmap<void *, astl::vector<ListenerInfo>> pointers;

        // Adds a listener for a specific type of event and returns an iterator to it.
        template <typename E>
        iterator addListener(u64 event, std::function<void(E &)> listener, int priority = 5)
        {
            static_assert(std::is_base_of<IEvent, E>::value, "E must inherit from Event");
            auto eventListener = astl::alloc<EventListener<E>>(listener);
            return _listeners[event].emplace(priority, eventListener);
        }

        // Checks if any listener exists for a specific event.
        bool exist(u64 id) const { return _listeners.count(id) > 0; }

        // Removes a listener using its iterator.
        void removeListener(BaseEventListener *listener, u64 event)
        {
            auto it = _listeners.find(event);
            if (it != _listeners.end())
            {
                auto &listeners = it->second;
                for (auto iter = listeners.begin(); iter != listeners.end();)
                {
                    if (iter->second == listener)
                    {
                        astl::release(iter->second);
                        iter = listeners.erase(iter);
                    }
                    else
                        ++iter;
                }
            }
        }

        // Dispatchs an event, invoking all listeners subscribed to this event.
        template <typename E, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>>
        void dispatch(E &event)
        {
            auto it = _listeners.find(event.id);
            if (it != _listeners.end())
            {
                for (auto iter = it->second.begin(); iter != it->second.end();)
                {
                    auto listener = static_cast<EventListener<E> *>(iter->second);
                    listener->invoke(event);
                    ++iter;
                }
            }
        }

        // Dispatches an event by name, creating an event instance with the provided arguments.
        inline void dispatch(u64 id)
        {
            auto event = IEvent(id);
            dispatch(event);
        }

        template <typename T>
        inline void dispatch(u64 id, const T &data)
        {
            auto event = Event<T>(id, data);
            dispatch(event);
        }

        template <typename E, typename = std::enable_if_t<std::is_base_of_v<IEvent, E>>, typename... Args>
        inline void dispatch(Args &&...args)
        {
            E event(std::forward<Args>(args)...);
            dispatch(event);
        }

        // Retrieves a list of all listeners registered for a specific event.
        // Returns a dynamic array of pointers to listeners for the event.
        template <typename E>
        astl::vector<EventListener<E> *> getListeners(u64 event)
        {
            astl::vector<EventListener<E> *> result;
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

        /**
         ** Unbinds all listeners managed by this registry from the event manager.
         ** \param owner The owner of the listeners.
         */
        void unbindListeners(void *owner)
        {
            auto it = pointers.find(owner);
            if (it != pointers.end())
            {
                for (auto &info : it->second) removeListener(info.listener, info.id);
                pointers.erase(owner);
            }
        }

        // Binds a listener to an event of a specific type.
        template <typename Listener>
        void bindEvent(void *owner, u64 event, Listener &&listener, int priority = 5)
        {
            using arg_type = typename astl::lambda_arg_traits<Listener>::argument_type;
            using event_type = typename std::remove_reference<arg_type>::type;
            static_assert(std::is_base_of<IEvent, event_type>::value, "EventType must inherit from Event");
            auto id = addListener<event_type>(event, std::forward<Listener>(listener), priority);
            pointers[owner].emplace_back(id->second, event);
        }

        // Binds a listener to a list of events of a specific type.
        template <typename Listener>
        inline void bindEvent(void *owner, std::initializer_list<u64> events, Listener &&listener, int priority = 5)
        {
            for (const auto &event : events) bindEvent(owner, event, std::forward<Listener>(listener), priority);
        }

        // Clears all registered listeners from the manager, ensuring all memory is properly freed.
        void clear()
        {
            for (auto &eventPair : _listeners)
            {
                auto &listeners = eventPair.second;
                for (auto &listenerPair : listeners) astl::release(listenerPair.second);
                listeners.clear();
            }
            _listeners.clear();
            pointers.clear();
        }

    private:
        astl::hashmap<u64, astl::multimap<int, BaseEventListener *>> _listeners;
    };
} // namespace events

namespace std
{
    template <>
    struct hash<events::IEvent>
    {
        size_t operator()(const events::IEvent &event) const { return event.id; }
    };
} // namespace std

#endif // APP_VIEWPORT_EVENT_H