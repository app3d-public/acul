#pragma once

#include "../../bit.hpp"

#define ACUL_CITYHASH_K0 0xc3a5c85c97cb3127ULL
#define ACUL_CITYHASH_K1 0xb492b66fbe98f273ULL
#define ACUL_CITYHASH_K2 0x9ae16a3b2f90404fULL

namespace acul::detail
{
    inline u64 cityhash128_64(const u128 &x)
    {
        // Murmur-inspired hashing.
        const u64 kMul = 0x9ddfea08eb382d69ULL;
        u64 a = (low64(x) ^ high64(x)) * kMul;
        a ^= (a >> 47);
        u64 b = (high64(x) ^ a) * kMul;
        b ^= (b >> 47);
        b *= kMul;
        return b;
    }

    ACUL_FORCEINLINE u64 cityhash16(u64 u, u64 v) { return cityhash128_64(u128{u, v}); }

    inline u64 cityhash16_mul(u64 u, u64 v, u64 mul)
    {
        // Murmur-inspired hashing.
        u64 a = (u ^ v) * mul;
        a ^= (a >> 47);
        u64 b = (v ^ a) * mul;
        b ^= (b >> 47);
        b *= mul;
        return b;
    }

    // Based by Google CityHash
    // @ref https://github.com/google/cityhash
    // Processed 0 .. 16 bytes. For other cases we use cityhash64_long
    inline u64 cityhash64_short(const char *s, size_t len)
    {
        if (len >= 8)
        {
            u64 mul = ACUL_CITYHASH_K2 + len * 2;
            u64 a = load_u64u_le(s) + ACUL_CITYHASH_K2;
            u64 b = load_u64u_le(s + len - 8);
            u64 c = ror64(b, 37) * mul + a;
            u64 d = (ror64(a, 25) + b) * mul;
            return cityhash16_mul(c, d, mul);
        }
        if (len >= 4)
        {
            u64 mul = ACUL_CITYHASH_K2 + len * 2;
            u64 a = load_u32u_le(s);
            return cityhash16_mul(len + (a << 3), load_u32u_le(s + len - 4), mul);
        }
        if (len > 0)
        {
            u8 a = static_cast<u8>(s[0]);
            u8 b = static_cast<u8>(s[len >> 1]);
            u8 c = static_cast<u8>(s[len - 1]);
            u32 y = static_cast<u32>(a) + (static_cast<u32>(b) << 8);
            u32 z = static_cast<u32>(len) + (static_cast<u32>(c) << 2);
            return shift_mix(y * ACUL_CITYHASH_K2 ^ z * ACUL_CITYHASH_K0) * ACUL_CITYHASH_K2;
        }
        return ACUL_CITYHASH_K2;
    }

    APPLIB_API u64 cityhash64_long(const char *s, size_t len);
} // namespace acul::detail