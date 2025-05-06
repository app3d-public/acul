#ifndef APP_ACUL_EVENT_H
#define APP_ACUL_EVENT_H

#include <cassert>
#include <functional>
#include "api.hpp"
#include "hash/hashmap.hpp"
#include "map.hpp"
#include "scalars.hpp"
#include "type_traits.hpp"
#include "vector.hpp"

namespace acul
{
    namespace events
    {
        struct event
        {
            // unsigned 62 number for id
            // Other bits reserved for pre & post events
            u64 id;

            explicit event(u64 id = 0) : id(id) {}

            virtual ~event() = default;

            bool operator==(const event &event) const { return event.id == id; }
            bool operator!=(const event &event) const { return !(*this == event); }
        };

        inline u64 make_pre_event_id(u64 id) { return id | 0x4000000000000000ULL; }

        inline u64 make_post_event_id(u64 id) { return id | 0x8000000000000000ULL; }

        template <typename T>
        struct data_event final : event
        {
        public:
            T data;

            template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, data_event>>>
            explicit data_event(u64 id, U &&data) : event(id), data(std::forward<U>(data))
            {
            }

            explicit data_event(u64 id) : event(id), data() {}
        };

        using ptr_event = data_event<event *>;

        // Base class for event listeners.
        class listener_base
        {
        public:
            virtual ~listener_base() = default;
        };

        // Template class for listeners specific to a type of event.
        template <typename E>
        class listener final : public listener_base
        {
            std::function<void(E &)> _listener;

        public:
            explicit listener(std::function<void(E &)> listener) : _listener(listener) {}

            // Invokes the listener function with the event.
            void invoke(E &event) { _listener(event); }
        };

        struct listener_info
        {
            listener_base *listener;
            u64 id;
        };
        // Manages event listeners and dispatches events to them.
        class APPLIB_API dispatcher
        {
        public:
            using iterator = acul::multimap<int, listener_base *>::iterator;
            acul::hashmap<void *, vector<listener_info>> pointers;

            // Adds a listener for a specific type of event and returns an iterator to it.
            template <typename E>
            iterator addListener(u64 event, std::function<void(E &)> listener, int priority = 5)
            {
                static_assert(std::is_base_of<acul::events::event, E>::value, "E must inherit from event");
                auto eventListener = acul::alloc<acul::events::listener<E>>(listener);
                return _listeners[event].emplace(priority, eventListener);
            }

            // Checks if any listener exists for a specific event.
            bool exist(u64 id) const { return _listeners.count(id) > 0; }

            // Removes a listener using its iterator.
            void remove_listener(listener_base *listener, u64 event)
            {
                auto it = _listeners.find(event);
                if (it != _listeners.end())
                {
                    auto &listeners = it->second;
                    for (auto iter = listeners.begin(); iter != listeners.end();)
                    {
                        if (iter->second == listener)
                        {
                            acul::release(iter->second);
                            iter = listeners.erase(iter);
                        }
                        else
                            ++iter;
                    }

                    if (listeners.empty()) _listeners.erase(it);
                }
            }

            // Dispatchs an event, invoking all listeners subscribed to this event.
            template <typename E, typename = std::enable_if_t<std::is_base_of_v<event, E>>>
            void dispatch(E &event)
            {
                auto it = _listeners.find(event.id);
                if (it != _listeners.end())
                {
                    for (auto iter = it->second.begin(); iter != it->second.end();)
                    {
                        auto listener = static_cast<acul::events::listener<E> *>(iter->second);
                        listener->invoke(event);
                        ++iter;
                    }
                }
            }

            // Dispatches an event by name, creating an event instance with the provided arguments.
            inline void dispatch(u64 id)
            {
                event event{id};
                dispatch(event);
            }

            template <typename T>
            inline void dispatch(u64 id, T &&data)
            {
                using data_type = std::decay_t<T>;
                auto event = data_event<data_type>(id, std::forward<T>(data));
                dispatch(event);
            }

            template <typename E, typename = std::enable_if_t<std::is_base_of_v<event, E>>, typename... Args>
            inline void dispatch(Args &&...args)
            {
                E event(std::forward<Args>(args)...);
                dispatch(event);
            }

            // Retrieves a list of all listeners registered for a specific event.
            // Returns a dynamic array of pointers to listeners for the event.
            template <typename E>
            vector<listener<E> *> get_listeners(u64 event)
            {
                vector<listener<E> *> result;
                auto it = _listeners.find(event);
                if (it != _listeners.end())
                {
                    for (const auto &pair : it->second)
                    {
                        auto listener = static_cast<acul::events::listener<E> *>(pair.second);
                        result.push_back(listener);
                    }
                }
                return result;
            }

            /**
             ** Unbinds all listeners managed by this registry from the event manager.
             ** \param owner The owner of the listeners.
             */
            void unbind_listeners(void *owner)
            {
                auto it = pointers.find(owner);
                if (it != pointers.end())
                {
                    for (auto &info : it->second) remove_listener(info.listener, info.id);
                    pointers.erase(owner);
                }
            }

            // Binds a listener to an event of a specific type.
            template <typename Listener>
            void bind_event(void *owner, u64 event, Listener &&listener, int priority = 5)
            {
                using arg_type = typename acul::lambda_arg_traits<Listener>::argument_type;
                using event_type = typename std::remove_reference<arg_type>::type;
                static_assert(std::is_base_of<acul::events::event, event_type>::value,
                              "EventType must inherit from event");
                auto id = addListener<event_type>(event, std::forward<Listener>(listener), priority);
                pointers[owner].emplace_back(id->second, event);
            }

            // Binds a listener to a list of events of a specific type.
            template <typename Listener>
            inline void bind_event(void *owner, std::initializer_list<u64> events, Listener &&listener,
                                   int priority = 5)
            {
                for (const auto &event : events) bind_event(owner, event, std::forward<Listener>(listener), priority);
            }

            // Clears all registered listeners from the manager, ensuring all memory is properly freed.
            void clear()
            {
                for (auto &eventPair : _listeners)
                {
                    auto &listeners = eventPair.second;
                    for (auto &listenerPair : listeners) acul::release(listenerPair.second);
                    listeners.clear();
                }
                _listeners.clear();
                pointers.clear();
            }

        private:
            acul::hashmap<u64, acul::multimap<int, listener_base *>> _listeners;
        };
    } // namespace events
} // namespace acul

namespace std
{
    template <>
    struct hash<acul::events::event>
    {
        size_t operator()(const acul::events::event &event) const { return event.id; }
    };
} // namespace std

#endif // APP_VIEWPORT_EVENT_H