#pragma once

#include "../../detail/simd/flags.hpp"

namespace acul::detail
{
    namespace avx2
    {
        APPLIB_API uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    namespace avx
    {
        APPLIB_API uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    namespace sse42
    {
        APPLIB_API uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    namespace nosimd
    {
        APPLIB_API uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    using PFN_crc32 = u32 (*)(u32 crc0, const char *buf, size_t len);

    inline PFN_crc32 load_crc32_fn(simd_flags flags)
    {
        if (flags & simd_flags::enum_type::avx2) return &avx2::crc32;
        else if (flags & simd_flags::enum_type::avx) return &avx::crc32;
        else if (flags & simd_flags::enum_type::sse42) return &sse42::crc32;
        return &nosimd::crc32;
    }
} // namespace acul::detail