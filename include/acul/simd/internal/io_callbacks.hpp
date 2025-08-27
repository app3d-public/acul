#pragma once
#include "../../fwd/string_pool.hpp"
#include "../../scalars.hpp"

namespace acul
{
    namespace internal
    {
        using PFN_crc32 = u32 (*)(u32 crc0, const char *buf, size_t len);
        using PFN_fill_line_buffer = void (*)(const char *data, size_t size, string_pool<char> &dst);
    } // namespace internal
} // namespace acul

#define ACUL_SIMD_IO_MEMBERS \
    PFN_crc32 crc32;         \
    PFN_fill_line_buffer fill_line_buffer;
