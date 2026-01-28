#pragma once

#include "../../hash/detail/crc32.hpp"
#include "../../io/fs/detail/file_simd_fn.hpp"
#include "flags.hpp"

namespace acul::detail
{
    extern APPLIB_API struct simd_context
    {
        simd_flags flags;
        PFN_crc32 crc32 = &nosimd::crc32;
        PFN_fill_line_buffer fill_line_buffer = &nosimd::fill_line_buffer;

        simd_context();
    } g_simd_ctx;
} // namespace acul::detail