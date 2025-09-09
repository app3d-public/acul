#pragma once

#include <array>
#include <cstddef>
#include "api.hpp"

namespace acul
{
    template <std::size_t N, typename Traits>
    struct lut_table
    {
        using value_type = typename Traits::value_type;
        using enum_type = typename Traits::enum_type;
        using array_type = std::array<enum_type, N>;

        static ACUL_CONSTEVAL array_type load_lut_table()
        {
            array_type a{};
            for (auto &x : a) x = Traits::unknown;
            Traits::fill_lut_table(a);
            return a;
        }

        inline static ACUL_CONSTINIT array_type data = load_lut_table();

        static constexpr enum_type find(value_type v) noexcept
        {
            const auto idx = static_cast<std::size_t>(v);
            return (idx < N) ? data[idx] : Traits::unknown;
        }
    };

} // namespace acul
