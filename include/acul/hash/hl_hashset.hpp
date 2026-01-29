#pragma once

#include "detail/raw_hl_hashtable.hpp"
#include "detail/set_traits.hpp"

namespace acul
{
    template <typename K, typename H = std::hash<K>, typename Eq = std::equal_to<K>>
    using hl_hashset = detail::raw_hl_hashtable<mem_allocator<std::byte>, detail::set_traits<K, H, Eq>>;
} // namespace acul