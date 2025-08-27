#ifndef APP_ACUL_EVENT_H
#define APP_ACUL_EVENT_H

#include <cassert>
#include <functional>
#include "api.hpp"
#include "hash/hashmap.hpp"
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

        struct event_node
        {
            void *owner;
            void (*call)(void *, event &); // thunk<E>
            void *ctx;                     // listener<E>*
            int prio;
        };

        template <class E>
        static void thunk(void *ctx, event &ev)
        {
            auto &e = static_cast<E &>(ev);
            reinterpret_cast<listener<E> *>(ctx)->invoke(e);
        }

        class event_group
        {
        public:
            using iterator = vector<event_node>::iterator;
            using const_iterator = vector<event_node>::const_iterator;

            iterator begin() { return _nodes.begin(); }
            iterator end() { return _nodes.end(); }
            const_iterator begin() const { return _nodes.begin(); }
            const_iterator end() const { return _nodes.end(); }

            template <class E>
            void add(void *owner, listener<E> *L, int prio)
            {
                remove_by_owner(owner);                          // гарантируем уникальность owner на это событие
                event_node n{owner, &thunk<E>, (void *)L, prio}; // порядок полей: owner, call, ctx, prio
                auto r = equal_range_desc(prio);
                _nodes.insert(_nodes.begin() + r.second,
                              n); // в конец диапазона равных prio (сохраняем порядок регистрации)
            }

            void remove_by_ptr(void *ctx, int prio, bool prio_known = true)
            {
                if (prio_known)
                {
                    auto r = equal_range_desc(prio);
                    for (int i = r.first; i < r.second; ++i)
                        if (_nodes[i].ctx == ctx)
                        {
                            erase_at(i);
                            return;
                        }
                }
                else
                {
                    for (int i = 0; i < (int)_nodes.size(); ++i)
                        if (_nodes[i].ctx == ctx)
                        {
                            erase_at(i);
                            return;
                        }
                }
            }

            bool remove_by_owner(void *owner)
            {
                for (int i = 0; i < (int)_nodes.size(); ++i)
                    if (_nodes[i].owner == owner)
                    {
                        erase_at(i);
                        return true;
                    }
                return false;
            }

            bool find_owner_ptr_prio(void *owner, listener_base *&out_ptr, int &out_prio) const
            {
                for (const auto &n : _nodes)
                {
                    if (n.owner == owner)
                    {
                        out_ptr = reinterpret_cast<listener_base *>(n.ctx);
                        out_prio = n.prio;
                        return true;
                    }
                }
                return false;
            }

            template <class E>
            void dispatch(E &e)
            {
                for (auto &n : _nodes) n.call(n.ctx, e);
            }

            bool empty() const { return _nodes.empty(); }

            void clear_release_all()
            {
                for (auto &n : _nodes) acul::release(reinterpret_cast<listener_base *>(n.ctx));
                _nodes.clear();
            }

        private:
            int lower_bound_desc(int p) const
            {
                int l = 0, r = (int)_nodes.size();
                while (l < r)
                {
                    int m = (l + r) >> 1;
                    (_nodes[m].prio > p) ? l = m + 1 : r = m;
                }
                return l;
            }
            int upper_bound_desc(int p) const
            {
                int l = 0, r = (int)_nodes.size();
                while (l < r)
                {
                    int m = (l + r) >> 1;
                    (_nodes[m].prio >= p) ? l = m + 1 : r = m;
                }
                return l;
            }
            pair<int, int> equal_range_desc(int p) const { return {lower_bound_desc(p), upper_bound_desc(p)}; }

            void erase_at(int i)
            {
                for (int j = i + 1; j < (int)_nodes.size(); ++j) _nodes[j - 1] = _nodes[j];
                _nodes.pop_back();
            }

            vector<event_node> _nodes;
        };

        template <class E, typename = std::enable_if_t<std::is_base_of_v<event, E>>, typename... Args>
        inline void dispatch_event_group(event_group *eg, E &e)
        {
            if (!eg) return;
            eg->template dispatch<E>(e);
        }

        template <class E, class... Args>
        inline void dispatch_event_group(event_group *eg, Args &&...args)
        {
            if (!eg) return;
            E e(std::forward<Args>(args)...);
            eg->template dispatch<E>(e);
        }

        class APPLIB_API dispatcher
        {
        public:
            struct iterator
            {
                u64 id;
                int prio;
                listener_base *ptr;
            };

            template <class E>
            iterator add_listener(void *owner, u64 id, std::function<void(E &)> &&fn, int priority = 5)
            {
                static_assert(std::is_base_of_v<event, E>, "E must inherit event");
                auto *L = alloc<listener<E>>(std::move(fn));
                auto *eg = ensure(id);
                eg->template add<E>(owner, L, priority);
                return iterator{id, priority, L};
            }

            template <typename Listener>
            void bind_event(void *owner, u64 id, Listener &&fn, int priority = 5)
            {
                using arg_t = typename acul::lambda_arg_traits<Listener>::argument_type;
                using E = std::remove_reference_t<arg_t>;
                static_assert(std::is_base_of<event, E>::value, "event type must inherit from event");
                (void)add_listener<E>(owner, id, std::function<void(E &)>(std::forward<Listener>(fn)), priority);
            }

            template <typename Listener>
            inline void bind_event(void *owner, std::initializer_list<u64> ids, Listener &&fn, int priority = 5)
            {
                for (auto id : ids) bind_event(owner, id, std::forward<Listener>(fn), priority);
            }

            bool exist(u64 id) const
            {
                auto it = _slots.find(id);
                return it != _slots.end() && it->second && !it->second->empty();
            }

            event_group *get_event_group(u64 id)
            {
                auto it = _slots.find(id);
                return (it != _slots.end()) ? it->second : nullptr;
            }
            const event_group *get_event_group(u64 id) const
            {
                auto it = _slots.find(id);
                return (it != _slots.end()) ? it->second : nullptr;
            }

            template <class E, std::enable_if_t<std::is_base_of_v<event, E>, int> = 0>
            void dispatch(E &e)
            {
                auto it = _slots.find(e.id);
                if (it != _slots.end() && it->second) it->second->template dispatch<E>(e);
            }

            inline void dispatch(u64 id)
            {
                event e{id};
                dispatch(e);
            }
            template <typename T>
            inline void dispatch(u64 id, T &&data)
            {
                data_event<std::decay_t<T>> e{id, std::forward<T>(data)};
                dispatch(e);
            }
            template <typename E, typename = std::enable_if_t<std::is_base_of_v<event, E>>, typename... Args>
            inline void dispatch(Args &&...args)
            {
                E e(std::forward<Args>(args)...);
                dispatch(e);
            }

            void unbind_listener(void *owner, u64 id)
            {
                auto it = _slots.find(id);
                if (it == _slots.end() || !it->second) return;

                auto *eg = it->second;
                listener_base *to_free = nullptr;
                int prio = 0;
                if (eg->find_owner_ptr_prio(owner, to_free, prio))
                {
                    eg->remove_by_owner(owner);
                    release(to_free);
                }

                if (eg->empty())
                {
                    release(eg);
                    _slots.erase(it);
                }
            }

            void unbind_listeners(void *owner)
            {
                for (auto it = _slots.begin(); it != _slots.end();)
                {
                    auto *eg = it->second;
                    if (!eg)
                    {
                        it = _slots.erase(it);
                        continue;
                    }

                    listener_base *ptr = nullptr;
                    int prio = 0;
                    if (eg->find_owner_ptr_prio(owner, ptr, prio))
                    {
                        eg->remove_by_owner(owner);
                        release(ptr);
                    }

                    if (eg->empty())
                    {
                        release(eg);
                        it = _slots.erase(it);
                    }
                    else
                        ++it;
                }
            }

            void clear()
            {
                for (auto &kv : _slots)
                {
                    if (kv.second)
                    {
                        kv.second->clear_release_all();
                        acul::release(kv.second);
                    }
                }
                _slots.clear();
            }

            ~dispatcher() { clear(); }

        private:
            hashmap<u64, event_group *> _slots;

            event_group *ensure(u64 id)
            {
                auto it = _slots.find(id);
                if (it != _slots.end() && it->second) return it->second;
                auto *eg = alloc<event_group>();
                _slots[id] = eg;
                return eg;
            }
        };

        inline void cache_event_group(u64 id, event_group *&dst, dispatcher *d)
        {
            auto *eg = d->get_event_group(id);
            if (eg && !eg->empty()) dst = eg;
        }
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