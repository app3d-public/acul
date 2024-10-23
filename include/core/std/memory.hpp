#ifndef APP_CORE_MEM_ALLOCATOR_H
#define APP_CORE_MEM_ALLOCATOR_H

#include <cstddef>
#include <limits>
#include <oneapi/tbb/scalable_allocator.h>
#include "type_traits.hpp"

namespace astl
{
    template <typename T>
    class mem_allocator
    {
    public:
        using value_type = T;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        static inline pointer allocate(size_type num, const void *hint = 0) noexcept
        {
            if (num > std::numeric_limits<size_type>::max() / sizeof(T)) return nullptr;
            if (auto p = static_cast<pointer>(scalable_malloc(num * sizeof(T)))) return p;
            return nullptr;
        }

        static inline pointer reallocate(pointer p, size_type newSize) noexcept
        {
            return reinterpret_cast<pointer>(p ? scalable_realloc(p, newSize * sizeof(T))
                                               : scalable_malloc(newSize * sizeof(T)));
        }

        static inline void deallocate(pointer p, size_type num) noexcept { scalable_free(p); }

        static inline size_type max_size() noexcept { return std::numeric_limits<size_type>::max() / sizeof(T); }

        template <typename U, typename... Args>
        static inline void construct(U *p, Args &&...args)
        {
            ::new ((void *)p) U(std::forward<Args>(args)...);
        }

        template <typename U>
        static inline std::enable_if_t<!std::is_void_v<U>> destroy(U *p)
        {
            if (!p) return;
            if constexpr (!std::is_trivially_destructible_v<U>) p->~U();
        }

        template <typename U>
        static inline std::enable_if_t<std::is_void_v<U>> destroy(U *p)
        {
        }

        template <typename U>
        struct rebind
        {
            using other = mem_allocator<U>;
        };
    };

    template <typename T, typename U>
    bool operator==(const mem_allocator<T> &, const mem_allocator<U> &)
    {
        return true;
    }

    template <typename T, typename U>
    bool operator!=(const mem_allocator<T> &, const mem_allocator<U> &)
    {
        return false;
    }

    template <typename T, typename... Args>
    inline T *alloc(Args &&...args)
    {
        auto ptr = mem_allocator<T>::allocate(1);
        if constexpr (!std::is_trivially_constructible_v<T> || has_args<Args...>())
            mem_allocator<T>::construct(ptr, std::forward<Args>(args)...);
        return ptr;
    }

    template <typename T>
    inline T *alloc_n(size_t size)
    {
        auto ptr = mem_allocator<T>::allocate(size);
        if constexpr (!std::is_trivially_constructible_v<T>)
            for (size_t i = 0; i < size; ++i) mem_allocator<T>::construct(ptr + i);
        return ptr;
    }

    template <typename T>
    inline void release(T *ptr)
    {
        astl::mem_allocator<T>::destroy(ptr);
        astl::mem_allocator<T>::deallocate(ptr, 1);
    }

    template <typename T>
    inline void release(T *ptr, size_t size)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (size_t i = 0; i < size; ++i) astl::mem_allocator<T>::destroy(ptr + i);
        mem_allocator<T>::deallocate(ptr, size);
    }

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

        size_t decrement_strong() { return (ref_counts -= 1ULL << 32) >> 32; }

        size_t decrement_weak() { return --ref_counts & weak_count_mask; }

        bool no_strong() const { return strong_count() == 0; }

        bool no_weak() const { return weak_count() == 0; }
    };

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
        mem_control_block *_ctrl;
        value_type *_data;

        void release() noexcept
        {
            if (_ctrl && _ctrl->strong_count() > 0)
            {
                _ctrl->decrement_strong();
                if (_ctrl->strong_count() == 0)
                {
                    allocator::destroy(_data);
                    if (_ctrl->is_external()) allocator::deallocate(_data, 1);
                    if (_ctrl->weak_count() == 0)
                    {
                        block_allocator::deallocate((std::byte *)_ctrl, 1);
                        _ctrl = nullptr;
                    }
                }
            }
        }

        template <typename U, typename... Args>
        friend shared_ptr<U> make_shared(Args &&...args);

        template <typename U, typename Au>
        friend class shared_ptr;

        template <typename U, typename Au>
        friend class weak_ptr;

    public:
        shared_ptr() : _ctrl(nullptr) {}

        shared_ptr(std::nullptr_t) : _ctrl(nullptr) {}

        template <typename U = T, std::enable_if_t<std::is_array_v<U>, int> = 0>
        explicit shared_ptr(size_t size)
        {
            static_assert(std::is_trivially_constructible_v<value_type>,
                          "Only trivially constructible arrays supported");
            const size_t blockSize = sizeof(mem_control_block) + sizeof(value_type) * size;
            _ctrl = (mem_control_block *)block_allocator::allocate(blockSize);
            _ctrl->ref_counts = 1ULL << 32;
            _data = reinterpret_cast<value_type *>((std::byte *)_ctrl + sizeof(mem_control_block));
        }

        explicit shared_ptr(pointer ptr) : _ctrl(nullptr), _data(ptr)
        {
            if (ptr)
            {
                _ctrl = (mem_control_block *)block_allocator::allocate(sizeof(mem_control_block));
                _ctrl->ref_counts = 1ULL << 32;
                _ctrl->set_external();
            }
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
                _ctrl->increment_strong();
                _data = weak._data;
            }
        }

        shared_ptr &operator=(const shared_ptr &other) noexcept
        {
            if (this != &other)
            {
                release();
                _ctrl = other._ctrl;
                _ctrl->increment_strong();
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

        void operator++() { _ctrl->increment_strong(); }
        void operator--() { release(); }

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

        size_type use_count() const { return _ctrl ? _ctrl->strong_count() : 0; }
    };

    template <typename T, typename... Args>
    shared_ptr<T> make_shared(Args &&...args)
    {
        shared_ptr<T> result;
        const size_t blockSize = sizeof(mem_control_block) + sizeof(T);
        result._ctrl = (mem_control_block *)mem_allocator<std::byte>::allocate(blockSize);
        result._ctrl->ref_counts = 1ULL << 32;
        result._data = reinterpret_cast<T *>((std::byte *)result._ctrl + sizeof(mem_control_block));
        if constexpr (!std::is_trivially_constructible_v<T> || has_args<Args...>())
            mem_allocator<T>::construct(result._data, std::forward<Args>(args)...);
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
        mem_control_block *_ctrl;
        T *_data;

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
            if (_ctrl) _ctrl->decrement_weak();
        }

        shared_ptr<T> lock() const
        {
            if (_ctrl && _ctrl->strong_count() > 0) return shared_ptr<T>(*this);
            return nullptr;
        }

        bool expired() const { return !_ctrl || _ctrl->strong_count() == 0; }
    };

} // namespace astl
#endif