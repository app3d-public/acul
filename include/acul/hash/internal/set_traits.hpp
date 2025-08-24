#pragma once

#include <cstdint>
#include <type_traits>
#include "../../api.hpp"

namespace acul
{
    namespace internal
    {
        template <class K, class H, class Eq>
        struct set_traits
        {
            using size_type = uint32_t;
            using value_type = K;
            using key_type = K;
            using hasher = H;
            using key_equal = Eq;
            using mapped_type = std::false_type;

            static ACUL_FORCEINLINE const key_type &get_key(const value_type &v) noexcept { return v; }
        };
    } // namespace internal
} // namespace acul