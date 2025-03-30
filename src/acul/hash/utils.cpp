#include <acul/api.hpp>
#include <acul/scalars.hpp>

namespace acul
{
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define WORDS_BIGENDIAN 1
    #endif
#endif

#ifdef _WIN32

    #include <stdlib.h>
    #define bswap_32(x) _byteswap_ulong(x)
    #define bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)

    // Mac OS X / Darwin features
    #include <libkern/OSByteOrder.h>
    #define bswap_32(x) OSSwapInt32(x)
    #define bswap_64(x) OSSwapInt64(x)

#elif defined(__sun) || defined(sun)

    #include <sys/byteorder.h>
    #define bswap_32(x) BSWAP_32(x)
    #define bswap_64(x) BSWAP_64(x)

#elif defined(__FreeBSD__)

    #include <sys/endian.h>
    #define bswap_32(x) bswap32(x)
    #define bswap_64(x) bswap64(x)

#elif defined(__OpenBSD__)

    #include <sys/types.h>
    #define bswap_32(x) swap32(x)
    #define bswap_64(x) swap64(x)

#elif defined(__NetBSD__)

    #include <machine/bswap.h>
    #include <sys/types.h>

    #if defined(__BSWAP_RENAME) && !defined(__bswap_32)
        #define bswap_32(x) bswap32(x)
        #define bswap_64(x) bswap64(x)
    #endif

#else

    #include <byteswap.h>

#endif

#ifdef WORDS_BIGENDIAN
    #define uint32_in_expected_order(x) (bswap_32(x))
    #define uint64_in_expected_order(x) (bswap_64(x))
#else
    #define uint32_in_expected_order(x) (x)
    #define uint64_in_expected_order(x) (x)
#endif

#if !defined(LIKELY)
    #if defined(__GNUC__) || defined(__clang__)
        #define LIKELY(x) (__builtin_expect(!!(x), 1))
    #else
        #define LIKELY(x) (x)
    #endif
#endif

    typedef std::pair<u64, u64> u128;

    inline u64 Uint128Low64(const u128 &x) { return x.first; }
    inline u64 Uint128High64(const u128 &x) { return x.second; }

    static const u64 k0 = 0xc3a5c85c97cb3127ULL;
    static const u64 k1 = 0xb492b66fbe98f273ULL;
    static const u64 k2 = 0x9ae16a3b2f90404fULL;

    // Magic numbers for 32-bit hashing.  Copied from Murmur3.
    static const u32 c1 = 0xcc9e2d51;
    static const u32 c2 = 0x1b873593;

    static u64 UNALIGNED_LOAD64(const char *p)
    {
        u64 result;
        memcpy(&result, p, sizeof(result));
        return result;
    }

    static u32 UNALIGNED_LOAD32(const char *p)
    {
        u32 result;
        memcpy(&result, p, sizeof(result));
        return result;
    }

    static u32 fetch32(const char *p) { return uint32_in_expected_order(UNALIGNED_LOAD32(p)); }
    static u64 fetch64(const char *p) { return uint64_in_expected_order(UNALIGNED_LOAD64(p)); }

    static inline u64 rotl64(u64 x, int r)
    {
#if defined(_MSC_VER)
        return _rotl64(x, r);
#else
        return (x << r) | (x >> (64 - r));
#endif
    }

    // Bitwise right rotate.  Normally this will compile to a single
    // instruction, especially if the shift is a manifest constant.
    static u64 rotate(u64 val, int shift)
    {
        // Avoid shifting by 64: doing so yields an undefined result.
        return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
    }

    static u64 shift_mix(u64 val) { return val ^ (val >> 47); }

    inline u64 hash128_64(const u128 &x)
    {
        // Murmur-inspired hashing.
        const u64 kMul = 0x9ddfea08eb382d69ULL;
        u64 a = (Uint128Low64(x) ^ Uint128High64(x)) * kMul;
        a ^= (a >> 47);
        u64 b = (Uint128High64(x) ^ a) * kMul;
        b ^= (b >> 47);
        b *= kMul;
        return b;
    }

    static u64 hash16(u64 u, u64 v, u64 mul)
    {
        // Murmur-inspired hashing.
        u64 a = (u ^ v) * mul;
        a ^= (a >> 47);
        u64 b = (v ^ a) * mul;
        b ^= (b >> 47);
        b *= mul;
        return b;
    }

    static u64 hash16(u64 u, u64 v) { return hash128_64(u128(u, v)); }

    static u64 hash0_16(const char *s, size_t len)
    {
        if (len >= 8)
        {
            u64 mul = k2 + len * 2;
            u64 a = fetch64(s) + k2;
            u64 b = fetch64(s + len - 8);
            u64 c = rotate(b, 37) * mul + a;
            u64 d = (rotate(a, 25) + b) * mul;
            return hash16(c, d, mul);
        }
        if (len >= 4)
        {
            u64 mul = k2 + len * 2;
            u64 a = fetch32(s);
            return hash16(len + (a << 3), fetch32(s + len - 4), mul);
        }
        if (len > 0)
        {
            u8 a = static_cast<u8>(s[0]);
            u8 b = static_cast<u8>(s[len >> 1]);
            u8 c = static_cast<u8>(s[len - 1]);
            u32 y = static_cast<u32>(a) + (static_cast<u32>(b) << 8);
            u32 z = static_cast<u32>(len) + (static_cast<u32>(c) << 2);
            return shift_mix(y * k2 ^ z * k0) * k2;
        }
        return k2;
    }

    // This probably works well for 16-byte strings as well, but it may be overkill
    // in that case.
    static u64 hash17_32(const char *s, size_t len)
    {
        u64 mul = k2 + len * 2;
        u64 a = fetch64(s) * k1;
        u64 b = fetch64(s + 8);
        u64 c = fetch64(s + len - 8) * mul;
        u64 d = fetch64(s + len - 16) * k2;
        return hash16(rotate(a + b, 43) + rotate(c, 30) + d, a + rotate(b + k2, 18) + c, mul);
    }

    // Return a 16-byte hash for 48 bytes.  Quick and dirty.
    // Callers do best to use "random-looking" values for a and b.
    static std::pair<u64, u64> weak_hash32_with_seeds(u64 w, u64 x, u64 y, u64 z, u64 a, u64 b)
    {
        a += w;
        b = rotate(b + a + z, 21);
        u64 c = a;
        a += x;
        a += y;
        b += rotate(a, 44);
        return std::make_pair(a + z, b + c);
    }

    // Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
    static std::pair<u64, u64> weak_hash32_with_seeds(const char *s, u64 a, u64 b)
    {
        return weak_hash32_with_seeds(fetch64(s), fetch64(s + 8), fetch64(s + 16), fetch64(s + 24), a, b);
    }

    // Return an 8-byte hash for 33 to 64 bytes.
    static u64 hash33_64(const char *s, size_t len)
    {
        u64 mul = k2 + len * 2;
        u64 a = fetch64(s) * k2;
        u64 b = fetch64(s + 8);
        u64 c = fetch64(s + len - 24);
        u64 d = fetch64(s + len - 32);
        u64 e = fetch64(s + 16) * k2;
        u64 f = fetch64(s + 24) * 9;
        u64 g = fetch64(s + len - 8);
        u64 h = fetch64(s + len - 16) * mul;
        u64 u = rotate(a + g, 43) + (rotate(b, 30) + c) * 9;
        u64 v = ((a + g) ^ d) + f + 1;
        u64 w = bswap_64((u + v) * mul) + h;
        u64 x = rotate(e + f, 42) + c;
        u64 y = (bswap_64((v + w) * mul) + g) * mul;
        u64 z = e + f + c;
        a = bswap_64((x + z) * mul + y) + b;
        b = shift_mix((z + a) * mul + d + h) * mul;
        return b + x;
    }

    // Based by Google CityHash
    // @ref https://github.com/google/cityhash
    APPLIB_API u64 cityhash64(const char *s, size_t len)
    {
        if (len <= 32)
            return len <= 16 ? hash0_16(s, len) : hash17_32(s, len);
        else if (len <= 64)
            return hash33_64(s, len);

        // For strings over 64 bytes we hash the end first, and then as we
        // loop we keep 56 bytes of state: v, w, x, y, and z.
        u64 x = fetch64(s + len - 40);
        u64 y = fetch64(s + len - 16) + fetch64(s + len - 56);
        u64 z = hash16(fetch64(s + len - 48) + len, fetch64(s + len - 24));
        std::pair<u64, u64> v = weak_hash32_with_seeds(s + len - 64, len, z);
        std::pair<u64, u64> w = weak_hash32_with_seeds(s + len - 32, y + k1, x);
        x = x * k1 + fetch64(s);

        // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
        len = (len - 1) & ~static_cast<size_t>(63);
        do {
            x = rotate(x + y + v.first + fetch64(s + 8), 37) * k1;
            y = rotate(y + v.second + fetch64(s + 48), 42) * k1;
            x ^= w.second;
            y += v.first + fetch64(s + 40);
            z = rotate(z + w.first, 33) * k1;
            v = weak_hash32_with_seeds(s, v.second * k1, x + w.first);
            w = weak_hash32_with_seeds(s + 32, z + w.second, y + fetch64(s + 16));
            std::swap(z, x);
            s += 64;
            len -= 64;
        } while (len != 0);
        return hash16(hash16(v.first, w.first) + shift_mix(y) * k1 + z, hash16(v.second, w.second) + x);
    }
} // namespace acul