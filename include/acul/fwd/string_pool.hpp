#pragma once

#include "../memory/alloc.hpp"

namespace acul
{
    template <typename T, typename Allocator = mem_allocator<T>>
    class string_pool;
}