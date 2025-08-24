#pragma once

#include "../../pair.hpp"

namespace acul
{
    namespace internal
    {
        template <class K, class V, class H, class Eq>
        struct map_traits
        {
            using size_type = uint32_t;
            using value_type = pair<K, V>;
            using key_type = K;
            using hasher = H;
            using key_equal = Eq;
            using mapped_type = V;

            static ACUL_FORCEINLINE const key_type &get_key(const value_type &v) noexcept { return v.first; }
        };
    } // namespace internal
} // namespace acul