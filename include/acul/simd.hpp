#pragma once
#include "api.hpp"
#include "enum.hpp"
#include "scalars.hpp"

namespace acul
{
    struct simd_flag_bits
    {
        enum enum_type : u16
        {
            None = 0x0000,
            Avx512 = 0x0001,
            Avx2 = 0x0002,
            Avx = 0x0004,
            Sse42 = 0x0008,
            Pclmul = 0x00010,
            Initialized = 0x00020
        };
        using flag_bitmask = std::true_type;
    };

    using simd_flags = flags<simd_flag_bits>;

    APPLIB_API void init_simd_module();

    APPLIB_API void terminate_simd_module();

    APPLIB_API simd_flags get_simd_flags();
} // namespace acul