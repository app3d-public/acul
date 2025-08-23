#pragma once

#include "alloc.hpp"

namespace acul
{
    namespace detail
    {
        struct mem_control_block
        {
            size_t ref_counts;

            static constexpr size_t strong_count_mask = 0x7FFFFFFF00000000;
            static constexpr size_t weak_count_mask = 0x00000000FFFFFFFF;
            static constexpr size_t external_flag_mask = 0x8000000000000000;

            bool is_external() const { return ref_counts & external_flag_mask; }

            void set_external() { ref_counts |= external_flag_mask; }

            size_t strong_count() const { return (ref_counts & strong_count_mask) >> 32; }

            size_t weak_count() const { return ref_counts & weak_count_mask; }

            void increment_strong() { ref_counts += 1ULL << 32; }

            void increment_weak() { ++ref_counts; }

            size_t decrement_strong()
            {
                ref_counts -= (1ULL << 32);
                return (ref_counts & strong_count_mask) >> 32;
            }

            size_t decrement_weak() { return --ref_counts & weak_count_mask; }

            bool no_strong() const { return strong_count() == 0; }

            bool no_weak() const { return weak_count() == 0; }
        };

        template <class T, class SP>
        inline void accept_owner(T *p, const SP &sp)
        {
            if constexpr (requires { p->_internal_accept_owner(sp); }) p->_internal_accept_owner(sp);
        }
    } // namespace detail

    template <typename T, typename Allocator>
    class weak_ptr;

    template <typename T, typename Allocator = mem_allocator<T>>
    class shared_ptr
    {
    public:
        using value_type = std::conditional_t<std::is_array_v<T>, std::remove_extent_t<T>, T>;
        using pointer = value_type *;
        using size_type = size_t;
        using allocator = typename Allocator::template rebind<value_type>::other;
        using block_allocator = typename Allocator::template rebind<std::byte>::other;

    private:
        detail::mem_control_block *_ctrl;
        value_type *_data;

        void release() noexcept
        {
            if (_ctrl && _ctrl->decrement_strong() == 0)
            {
                allocator::destroy(_data);
                if (_ctrl->is_external()) allocator::deallocate(_data, 1);
                if (_ctrl->decrement_weak() == 0) block_allocator::deallocate((std::byte *)_ctrl, 1);
                _ctrl = nullptr;
                _data = nullptr;
            }
        }

        template <typename U, typename... Args>
        friend shared_ptr<U> make_shared(Args &&...args);

        template <typename U, typename Au>
        friend class shared_ptr;

        template <typename U, typename Au>
        friend class weak_ptr;

    public:
        shared_ptr() : _ctrl(nullptr), _data(nullptr) {}

        shared_ptr(std::nullptr_t) : _ctrl(nullptr), _data(nullptr) {}

        template <typename U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
        explicit shared_ptr(size_t size)
        {
            static_assert(std::is_trivially_constructible_v<value_type>,
                          "Only trivially constructible arrays supported");
            const size_t blockSize = sizeof(detail::mem_control_block) + sizeof(value_type) * size;
            _ctrl = (detail::mem_control_block *)block_allocator::allocate(blockSize);
            _ctrl->ref_counts = (1ULL << 32) | 1ULL;
            _data = reinterpret_cast<value_type *>((std::byte *)_ctrl + sizeof(detail::mem_control_block));
        }

        explicit shared_ptr(pointer ptr) : _ctrl(nullptr), _data(ptr)
        {
            if (!ptr) return;
            _ctrl = (detail::mem_control_block *)block_allocator::allocate(sizeof(detail::mem_control_block));
            _ctrl->ref_counts = (1ULL << 32) | 1ULL;
            _ctrl->set_external();
            detail::accept_owner(ptr, *this);
        }

        shared_ptr(const shared_ptr &other) noexcept : _ctrl(other._ctrl), _data(other._data)
        {
            if (_ctrl) _ctrl->increment_strong();
        }

        template <typename U, typename Au>
        shared_ptr(const shared_ptr<U, Au> &other) noexcept : _ctrl(other._ctrl), _data(other._data)
        {
            if (_ctrl) _ctrl->increment_strong();
        }

        shared_ptr(shared_ptr &&other) noexcept : _ctrl(other._ctrl), _data(other._data)
        {
            other._ctrl = nullptr;
            other._data = nullptr;
        }

        template <typename U, typename Au>
        shared_ptr(const shared_ptr<U, Au> &other, pointer ptr) noexcept : _ctrl(other._ctrl), _data(ptr)
        {
            if (_ctrl) _ctrl->increment_strong();
        }

        template <typename U, typename Au>
        explicit shared_ptr(const weak_ptr<U, Au> &weak) noexcept : _ctrl(nullptr), _data(nullptr)
        {
            if (weak._ctrl && weak._ctrl->strong_count() > 0)
            {
                _ctrl = weak._ctrl;
                if (_ctrl) _ctrl->increment_strong();
                _data = weak._data;
            }
        }

        shared_ptr &operator=(const shared_ptr &other) noexcept
        {
            if (this != &other)
            {
                release();
                _ctrl = other._ctrl;
                if (_ctrl) _ctrl->increment_strong();
                _data = other._data;
            }
            return *this;
        }

        shared_ptr &operator=(std::nullptr_t) noexcept
        {
            release();
            _ctrl = nullptr;
            _data = nullptr;
            return *this;
        }

        shared_ptr &operator=(shared_ptr &&other) noexcept
        {
            if (this != &other)
            {
                release();
                _ctrl = other._ctrl;
                other._ctrl = nullptr;
                _data = other._data;
                other._data = nullptr;
            }
            return *this;
        }

        ~shared_ptr() { release(); }

        void reset() noexcept
        {
            release();
            _ctrl = nullptr;
            _data = nullptr;
        }

        template <typename U = T>
        std::enable_if_t<!std::is_void_v<T>, U> &operator*()
        {
            return *_data;
        }

        template <typename U = T>
        std::enable_if_t<!std::is_void_v<T>, U> &operator*() const
        {
            return *_data;
        }

        pointer operator->() noexcept { return _data; }
        pointer operator->() const noexcept { return _data; }
        pointer get() noexcept { return _data; }
        pointer get() const noexcept { return _data; }
        explicit operator bool() const { return _ctrl != nullptr; }
        bool operator==(std::nullptr_t) const noexcept { return _data == nullptr; }
        bool operator!=(std::nullptr_t) const noexcept { return _data != nullptr; }

        size_type use_count() const { return _ctrl ? _ctrl->strong_count() : 0; }
    };

    template <typename T, typename... Args>
    shared_ptr<T> make_shared(Args &&...args)
    {
        shared_ptr<T> result;
        const size_t blockSize = sizeof(detail::mem_control_block) + sizeof(T);
        result._ctrl = (detail::mem_control_block *)mem_allocator<std::byte>::allocate(blockSize);
        result._ctrl->ref_counts = (1ULL << 32) | 1ULL;
        result._data = reinterpret_cast<T *>((std::byte *)result._ctrl + sizeof(detail::mem_control_block));
        if constexpr (!std::is_trivially_constructible_v<T> || has_args<Args...>())
            mem_allocator<T>::construct(result._data, std::forward<Args>(args)...);
        detail::accept_owner(result._data, result);
        return result;
    }

    template <class To, class From>
    inline shared_ptr<To> dynamic_pointer_cast(const shared_ptr<From> &from) noexcept
    {
        typedef typename shared_ptr<To>::pointer pointer;
        typedef typename shared_ptr<To>::allocator allocator;
        pointer ptr = dynamic_cast<pointer>(from.get());
        return ptr ? shared_ptr<To, allocator>(from, ptr) : shared_ptr<To, allocator>();
    }

    template <typename To, typename From>
    inline shared_ptr<To> static_pointer_cast(const shared_ptr<From> &from)
    {
        typedef typename shared_ptr<To>::pointer pointer;
        typedef typename shared_ptr<To>::allocator allocator;
        return shared_ptr<To, allocator>(from, static_cast<pointer>(from.get()));
    }

    template <typename To, typename From>
    inline shared_ptr<To> reinterpret_pointer_cast(const shared_ptr<From> &from) noexcept
    {
        using pointer = typename shared_ptr<To>::pointer;
        return shared_ptr<To>(from, reinterpret_cast<pointer>(from.get()));
    }

    template <typename T, typename Allocator = mem_allocator<std::byte>>
    class weak_ptr
    {
        detail::mem_control_block *_ctrl;
        T *_data;
        using block_allocator = typename Allocator::template rebind<std::byte>::other;

        template <typename U, typename Au>
        friend class shared_ptr;

    public:
        weak_ptr() : _ctrl(nullptr), _data(nullptr) {}

        weak_ptr(const shared_ptr<T> &ptr) : _ctrl(ptr._ctrl), _data(ptr._data)
        {
            if (_ctrl) _ctrl->increment_weak();
        }

        weak_ptr(const weak_ptr &other) : _ctrl(other._ctrl), _data(other._data)
        {
            if (_ctrl) _ctrl->increment_weak();
        }

        weak_ptr &operator=(const weak_ptr &other)
        {
            if (this != &other)
            {
                if (_ctrl) _ctrl->decrement_weak();
                _ctrl = other._ctrl;
                _data = other._data;
                if (_ctrl) _ctrl->increment_weak();
            }
            return *this;
        }

        ~weak_ptr()
        {
            if (_ctrl)
            {
                _ctrl->decrement_weak();
                if (_ctrl->no_strong() && _ctrl->no_weak()) block_allocator::deallocate((std::byte *)_ctrl, 1);
            }
        }

        shared_ptr<T> lock() const
        {
            if (_ctrl && _ctrl->strong_count() > 0) return shared_ptr<T>(*this);
            return nullptr;
        }

        bool expired() const { return !_ctrl || _ctrl->no_strong(); }
    };

    template <typename T, typename Allocator = mem_allocator<T>>
    class enable_shared_from_this
    {
    protected:
        enable_shared_from_this() noexcept = default;
        enable_shared_from_this(const enable_shared_from_this &) noexcept = default;
        enable_shared_from_this &operator=(const enable_shared_from_this &) noexcept = default;
        ~enable_shared_from_this() = default;

    public:
        shared_ptr<T, Allocator> shared_from_this() { return shared_ptr<T>(_weak_this); }

        shared_ptr<T, Allocator> shared_from_this() const { return shared_ptr<const T>(_weak_this); }

        weak_ptr<T, Allocator> weak_from_this() noexcept { return _weak_this; }

        weak_ptr<T, Allocator> weak_from_this() const noexcept { return _weak_this; }

    private:
        mutable weak_ptr<T, Allocator> _weak_this;

        template <typename U>
        void _internal_accept_owner(const shared_ptr<U> &shared_ptr) const noexcept
        {
            if (_weak_this.expired()) _weak_this = static_pointer_cast<T>(shared_ptr);
        }

        template <typename U, typename Au>
        friend class shared_ptr;

        template <class P, class Sp>
        friend void detail::accept_owner(P *, const Sp &);
    };

    template <typename T>
    struct default_delete
    {
        void operator()(T *p) const { release(p); }
    };

    template <typename T, typename D = default_delete<T>>
    class unique_ptr
    {
        template <typename U, typename Au, typename... Args>
        friend unique_ptr<U, Au> make_unique(Args &&...args);

        template <typename U, typename Au>
        friend class unique_ptr;

    public:
        using value_type = std::conditional_t<std::is_array_v<T>, std::remove_extent_t<T>, T>;
        using pointer = value_type *;
        using size_type = size_t;

        unique_ptr(pointer ptr = nullptr) : _data(ptr) {}

        unique_ptr(std::nullptr_t) : _data(nullptr) {}

        unique_ptr(const unique_ptr &) = delete;
        unique_ptr &operator=(const unique_ptr &) = delete;

        unique_ptr(unique_ptr &&other) noexcept : _data(other._data) { other._data = nullptr; }

        template <typename U, typename Au>
        unique_ptr(unique_ptr<U, Au> &&other) noexcept : _data(other._data)
        {
            other._data = nullptr;
        }

        unique_ptr &operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        unique_ptr &operator=(unique_ptr &&other) noexcept
        {
            if (this != &other)
            {
                _deleter(_data);
                _data = other._data;
                other._data = nullptr;
            }
            return *this;
        }

        ~unique_ptr() { reset(); }

        void reset(pointer ptr = nullptr) noexcept
        {
            if (_data != ptr)
            {
                _deleter(_data);
                _data = ptr;
            }
        }

        template <typename U = T>
        std::enable_if_t<!std::is_void_v<T>, U> &operator*()
        {
            return *_data;
        }

        template <typename U = T>
        std::enable_if_t<!std::is_void_v<T>, U> &operator*() const
        {
            return *_data;
        }

        pointer operator->() noexcept { return _data; }
        pointer operator->() const noexcept { return _data; }
        pointer get() noexcept { return _data; }
        pointer get() const noexcept { return _data; }
        explicit operator bool() const { return _data != nullptr; }

    private:
        pointer _data;
        D _deleter;
    };

    template <typename T, typename D = default_delete<T>, typename... Args>
    unique_ptr<T, D> make_unique(Args &&...args)
    {
        unique_ptr<T, D> result(alloc<T>(std::forward<Args>(args)...));
        return result;
    }
} // namespace acul