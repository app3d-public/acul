#include <acul/event.hpp>

namespace acul::events
{
    void dispatcher::unbind_listener(void *owner, u64 id)
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

    void dispatcher::unbind_listeners(void *owner)
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
            else ++it;
        }
    }
} // namespace acul::events