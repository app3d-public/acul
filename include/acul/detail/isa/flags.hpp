#pragma once

#include "../../enum.hpp"
#include "../../scalars.hpp"

namespace acul::detail
{
    struct isa_flag_bits
    {
        enum enum_type : u16
        {
            none = 0x0000,
            avx512 = 0x0001,
            avx2 = 0x0002,
            avx = 0x0004,
            sse42 = 0x0008,
            pclmul = 0x00010,
        };
        using flag_bitmask = std::true_type;
    };

    using isa_flags = flags<isa_flag_bits>;
} // namespace acul::detail
