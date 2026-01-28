#pragma once

#include <functional>
#include "../internal/hash/raw_hl_hashtable.hpp"
#include "../internal/hash/set_traits.hpp"
#include "../memory/alloc.hpp"

namespace acul
{
    template <typename K, typename H = std::hash<K>, typename Eq = std::equal_to<K>>
    using hl_hashset = internal::raw_hl_hashtable<mem_allocator<std::byte>, internal::set_traits<K, H, Eq>>;
} // namespace acul