#pragma once

#include <cstring>
#include "api.hpp"
#include "pair.hpp"

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define ACUL_WORDS_BIGENDIAN 1
    #endif
#endif

#ifdef _WIN32
    #include <stdlib.h>
    #define ACUL_BSWAP_32(x) _byteswap_ulong(x)
    #define ACUL_BSWAP_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)
    // Mac OS X / Darwin features
    #include <libkern/OSByteOrder.h>
    #define ACUL_BSWAP_32(x) OSSwapInt32(x)
    #define ACUL_BSWAP_64(x) OSSwapInt64(x)

#elif defined(__sun) || defined(sun)
    #include <sys/byteorder.h>
    #define ACUL_BSWAP_32(x) BSWAP_32(x)
    #define ACUL_BSWAP_64(x) BSWAP_64(x)

#elif defined(__FreeBSD__)
    #include <sys/endian.h>
    #define ACUL_BSWAP_32(x) bswap32(x)
    #define ACUL_BSWAP_64(x) bswap64(x)

#elif defined(__OpenBSD__)
    #include <sys/types.h>
    #define ACUL_BSWAP_32(x) swap32(x)
    #define ACUL_BSWAP_64(x) swap64(x)

#elif defined(__NetBSD__)
    #include <machine/bswap.h>
    #include <sys/types.h>

    #if defined(bswap32) && defined(bswap64)
        #define ACUL_BSWAP_32(x) bswap32(x)
        #define ACUL_BSWAP_64(x) bswap64(x)
    #elif defined(__bswap32) && defined(__bswap64)
        #define ACUL_BSWAP_32(x) __bswap32(x)
        #define ACUL_BSWAP_64(x) __bswap64(x)
    #else
        #error "NetBSD: no suitable bswap32/bswap64 found"
    #endif

#else
    #include <byteswap.h>

    #define ACUL_BSWAP_32(x) bswap_32(x)
    #define ACUL_BSWAP_64(x) bswap_64(x)
#endif

#ifdef ACUL_WORDS_BIGENDIAN
    #define ACUL_U32_IN_EXPECTED_ORDER(x) (ACUL_BSWAP_32(x))
    #define ACUL_U64_IN_EXPECTED_ORDER(x) (ACUL_BSWAP_64(x))
#else
    #define ACUL_U32_IN_EXPECTED_ORDER(x) (x)
    #define ACUL_U64_IN_EXPECTED_ORDER(x) (x)
#endif

namespace acul
{
    static ACUL_FORCEINLINE unsigned ctz32(u32 x)
    {
#if defined(_MSC_VER)
        unsigned long r;
        _BitScanForward(&r, x);
        return (unsigned)r;
#else
        return (unsigned)__builtin_ctz(x);
#endif
    }

    static ACUL_FORCEINLINE u32 pop_lsb(u32 &m)
    {
        u32 r = ctz32(m);
        m &= (m - 1);
        return r;
    }

    ACUL_FORCEINLINE u32 load_u32u(const void *p) noexcept
    {
        u32 v;
        memcpy(&v, p, sizeof(v));
        return v;
    }

    ACUL_FORCEINLINE u64 load_u64u(const void *p) noexcept
    {
        u64 v;
        memcpy(&v, p, sizeof(v));
        return v;
    }

    ACUL_FORCEINLINE u32 load_u32u_le(const void *p) noexcept { return ACUL_U32_IN_EXPECTED_ORDER(load_u32u(p)); }

    ACUL_FORCEINLINE u64 load_u64u_le(const void *p) noexcept { return ACUL_U64_IN_EXPECTED_ORDER(load_u64u(p)); }

    ACUL_FORCEINLINE u64 ror64(u64 v, unsigned s) noexcept { return s ? ((v >> s) | (v << (64u - s))) : v; }

    ACUL_FORCEINLINE u64 shift_mix(u64 v) noexcept { return v ^ (v >> 47); }

    typedef pair<u64, u64> u128;

    ACUL_FORCEINLINE u64 low64(const u128 &x) noexcept { return x.first; }
    ACUL_FORCEINLINE u64 high64(const u128 &x) noexcept { return x.second; }

} // namespace acul