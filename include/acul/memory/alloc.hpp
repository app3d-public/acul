#ifndef APP_ACUL_MEM_ALLOCATOR_H
#define APP_ACUL_MEM_ALLOCATOR_H

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <oneapi/tbb/scalable_allocator.h>
#include <type_traits>
#include <utility>
#include "../type_traits.hpp"

namespace acul
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

        static inline void deallocate(pointer p, size_type num = 0) noexcept { scalable_free(p); }

        static inline size_type max_size() noexcept { return std::numeric_limits<size_type>::max() / sizeof(T); }

        template <typename U, typename... Args>
        static inline void construct(U *p, Args &&...args)
        {
            if constexpr (__is_aggregate(U))
                ::new ((void *)p) U{static_cast<Args &&>(args)...};
            else
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

    template <typename T, typename U>
    inline T *alloc(std::initializer_list<U> init_list)
    {
        auto ptr = mem_allocator<T>::allocate(1);
        mem_allocator<T>::construct(ptr, init_list);
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
        if constexpr (!std::is_trivially_destructible_v<T>) acul::mem_allocator<T>::destroy(ptr);
        acul::mem_allocator<T>::deallocate(ptr, 1);
    }

    template <typename T>
    inline void release(T *ptr, size_t size)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (size_t i = 0; i < size; ++i) acul::mem_allocator<T>::destroy(ptr + i);
        mem_allocator<T>::deallocate(ptr, size);
    }

    /**
     * \brief Get the recommended memory growth size.
     * @param csize Current memory size.
     * @param msize Minimum (preferred) memory size.
     * @return The recommended memory growth size.
     */
    inline size_t get_growth_size(size_t csize, size_t msize)
    {
        return std::max(csize * 2, std::max((size_t)8UL, msize));
    }

    inline uint32_t get_growth_size_aligned(uint32_t x)
    {
        if (x <= 8) return 8;
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        uint32_t r = x + 1;
        return r ? r : 0x80000000u;
    }

    inline size_t align_up(size_t x, size_t a) { return (x + (a - 1)) & ~(a - 1); }

    template <class T>
    inline T *align_up_ptr(T *p, size_t a)
    {
        auto v = reinterpret_cast<size_t>(p);
        v = align_up(v, a);
        return reinterpret_cast<T *>(v);
    }

    template <typename T>
    class proxy
    {
    public:
        proxy(T *ptr = nullptr) : _ptr(ptr) {}

        T *operator->() { return _ptr; }
        T &operator*() { return *_ptr; }

        operator bool() const { return _ptr != nullptr; }

        void set(T *ptr) { _ptr = ptr; }

        T *get() { return _ptr; }

        void reset() { _ptr = nullptr; }

        void operator=(T *ptr) { _ptr = ptr; }
        bool operator==(T *ptr) { return _ptr == ptr; }
        bool operator!=(T *ptr) { return _ptr != ptr; }

        proxy &operator++()
        {
            ++_ptr;
            return *this;
        }

        proxy operator++(int)
        {
            proxy temp = *this;
            ++_ptr;
            return temp;
        }

        proxy &operator--()
        {
            --_ptr;
            return *this;
        }

        proxy operator--(int)
        {
            proxy temp = *this;
            --_ptr;
            return temp;
        }

    private:
        T *_ptr;
    };

    template <typename... Args>
    struct destructible_data
    {
        using PFN_destruct = void (*)(Args...);
        PFN_destruct destruct = nullptr;
    };

    template <typename T, typename... Args>
    struct destructible_value : destructible_data<Args...>
    {
        T value;

        destructible_value(const T &v) : destructible_data<Args...>{}, value(v) {}
    };
} // namespace acul
#endif