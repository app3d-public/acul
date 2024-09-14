#ifndef APP_CORE_MEM_ALLOCATOR_H
#define APP_CORE_MEM_ALLOCATOR_H

#include <cstddef>
#include <limits>
#include <oneapi/tbb/scalable_allocator.h>

namespace astl {

template <typename T>
class mem_allocator
{
public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    mem_allocator() noexcept = default;
    mem_allocator(const mem_allocator &) noexcept = default;

    template <typename U>
    mem_allocator(const mem_allocator<U> &) noexcept
    {
    }
    ~mem_allocator() = default;

    pointer allocate(size_type num, const void *hint = 0) noexcept
    {
        if (num > std::numeric_limits<size_type>::max() / sizeof(T)) return nullptr;
        if (auto p = static_cast<pointer>(scalable_malloc(num * sizeof(T)))) return p;
        return nullptr;
    }

    pointer reallocate(pointer p, size_type newSize) noexcept
    {
        return reinterpret_cast<pointer>(p ? scalable_realloc(p, newSize * sizeof(T))
                                           : scalable_malloc(newSize * sizeof(T)));
    }

    inline void deallocate(pointer p, size_type num) noexcept
    {
        if (p) scalable_free(p);
    }

    size_type max_size() const noexcept { return std::numeric_limits<size_type>::max() / sizeof(T); }

    template <typename U, typename... Args>
    inline void construct(U *p, Args &&...args)
    {
        ::new ((void *)p) U(std::forward<Args>(args)...);
    }

    template <typename U>
    inline void destroy(U *p)
    {
        if constexpr (!std::is_trivially_destructible_v<U>) p->~U();
    }
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
}
#endif