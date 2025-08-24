#pragma once

#include <functional>
#include "../memory/alloc.hpp"
#include "internal/raw_hl_hashtable.hpp"
#include "internal/set_traits.hpp"


namespace acul
{
    template <typename K, typename H = std::hash<K>, typename Eq = std::equal_to<K>>
    using hl_hashset = internal::raw_hl_hashtable<mem_allocator<std::byte>, internal::set_traits<K, H, Eq>>;
} // namespace acul