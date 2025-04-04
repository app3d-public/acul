#pragma once

#include "../memory.hpp"

namespace acul
{

    template <typename T, typename Allocator = mem_allocator<T>>
    class basic_stringstream;
    
    using stringstream = basic_stringstream<char>;
}