#pragma once

#include "../memory/alloc.hpp"

namespace acul
{
    template <typename T, typename Allocator = mem_allocator<T>>
    class basic_string;

    using string = basic_string<char>;
    using u16string = basic_string<char16_t>;
    using wstring = basic_string<wchar_t>;
} // namespace acul