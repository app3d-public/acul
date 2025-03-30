#pragma once

#include <atomic>
#include "base.hpp"

namespace acul
{
    class refstring
    {
    private:
        struct Rep
        {
            std::atomic<int> count;
            size_t len;
            size_t cap;
            char data[];
        };

        const char *_data;

        bool uses_refcount() const noexcept
        {
            return _data != nullptr && reinterpret_cast<const Rep *>(_data - sizeof(Rep))->count.load() >= 0;
        }

        static Rep *rep_from_data(const char *data) noexcept
        {
            return reinterpret_cast<Rep *>(const_cast<char *>(data) - sizeof(Rep));
        }

        static char *data_from_rep(Rep *rep) noexcept { return rep->data; }

        void assign(const char *msg)
        {
            release();
            size_t len = null_terminated_length(msg);
            Rep *rep = static_cast<Rep *>(::operator new(sizeof(Rep) + len + 1));
            rep->count.store(1);
            rep->len = len;
            rep->cap = len;
            memcpy(rep->data, msg, len + 1);
            _data = rep->data;
        }

    public:
        refstring() noexcept : _data(nullptr) {}

        explicit refstring(const char *msg) { assign(msg); }

        refstring(const refstring &other) noexcept : _data(other._data)
        {
            if (uses_refcount()) rep_from_data(_data)->count.fetch_add(1, std::memory_order_relaxed);
        }

        refstring &operator=(const refstring &other) noexcept
        {
            if (this != &other)
            {
                release();
                _data = other._data;
                if (uses_refcount()) rep_from_data(_data)->count.fetch_add(1, std::memory_order_relaxed);
            }
            return *this;
        }

        refstring &operator=(const char *msg)
        {
            assign(msg);
            return *this;
        }

        ~refstring() noexcept { release(); }

        const char *c_str() const noexcept { return _data; }

    private:
        void release() noexcept
        {
            if (uses_refcount())
            {
                Rep *rep = rep_from_data(_data);
                if (rep->count.fetch_sub(1, std::memory_order_acq_rel) == 1) { ::operator delete(rep); }
            }
        }
    };

} // namespace acul
