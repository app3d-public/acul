#pragma once

#include "../../detail/isa/flags.hpp"

namespace acul::detail
{
    namespace avx2
    {
        uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    namespace avx
    {
        uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    namespace sse42
    {
        uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    namespace scalar
    {
        uint32_t crc32(uint32_t crc0, const char *buf, size_t len);
    }

    using PFN_crc32 = u32 (*)(u32 crc0, const char *buf, size_t len);

    inline PFN_crc32 load_crc32_fn(isa_flags flags)
    {
        if (flags & isa_flag_bits::pclmul)
        {
            if (flags & isa_flag_bits::avx2) return &avx2::crc32;
            else if (flags & isa_flag_bits::avx) return &avx::crc32;
            else if (flags & isa_flag_bits::sse42) return &sse42::crc32;
        }
        return &scalar::crc32;
    }
} // namespace acul::detail