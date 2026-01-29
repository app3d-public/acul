#pragma once

#include "detail/map_traits.hpp"
#include "detail/raw_hl_hashtable.hpp"

namespace acul
{
    template <typename K, typename V, typename H = std::hash<K>, typename Eq = std::equal_to<K>>
    using hl_hashmap = detail::raw_hl_hashtable<mem_allocator<std::byte>, detail::map_traits<K, V, H, Eq>>;
} // namespace acul