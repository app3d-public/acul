#include <cstdint>
#include <cstdio>
#include <nmmintrin.h>

extern "C"
{
    __declspec(dllexport) uint32_t crc32_impl(uint32_t crc, const char *buf, size_t len)
    {
        crc = ~crc;
        const char *end = buf + len;
        for (; (reinterpret_cast<uintptr_t>(buf) & 3) && buf < end; ++buf)
            crc = _mm_crc32_u8(crc, *buf);
        for (; buf + 4 <= end; buf += 4)
            crc = _mm_crc32_u32(crc, *(reinterpret_cast<const uint32_t *>(buf)));
        for (; buf < end; ++buf)
            crc = _mm_crc32_u8(crc, *buf);
        return ~crc;
    }
}