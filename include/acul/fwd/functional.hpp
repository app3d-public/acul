#pragma once

#include <cstddef>

namespace acul
{
    template <class>
    class mem_allocator;

    template <class Sig, size_t S = sizeof(void *) * 2, template <class> class Alloc = mem_allocator>
    class unique_function;

    template <class Sig, size_t S = sizeof(void *) * 2, template <class> class Alloc = mem_allocator>
    class function;
} // namespace acul
