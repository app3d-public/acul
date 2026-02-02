#include <acul/string/detail/string_hash.hpp>
#include <utility>

namespace acul::detail
{
    // This probably works well for 16-byte strings as well, but it may be overkill
    // in that case.
    static u64 hash17_32(const char *s, size_t len)
    {
        u64 mul = ACUL_CITYHASH_K2 + len * 2;
        u64 a = load_u64u_le(s) * ACUL_CITYHASH_K1;
        u64 b = load_u64u_le(s + 8);
        u64 c = load_u64u_le(s + len - 8) * mul;
        u64 d = load_u64u_le(s + len - 16) * ACUL_CITYHASH_K2;
        return cityhash16_mul(ror64(a + b, 43) + ror64(c, 30) + d, a + ror64(b + ACUL_CITYHASH_K2, 18) + c, mul);
    }

    // Return a 16-byte hash for 48 bytes.  Quick and dirty.
    // Callers do best to use "random-looking" values for a and b.
    static pair<u64, u64> weak_hash32_with_seeds(u64 w, u64 x, u64 y, u64 z, u64 a, u64 b)
    {
        a += w;
        b = ror64(b + a + z, 21);
        u64 c = a;
        a += x;
        a += y;
        b += ror64(a, 44);
        return {a + z, b + c};
    }

    // Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
    static pair<u64, u64> weak_hash32_with_seeds(const char *s, u64 a, u64 b)
    {
        return weak_hash32_with_seeds(load_u64u_le(s), load_u64u_le(s + 8), load_u64u_le(s + 16), load_u64u_le(s + 24),
                                      a, b);
    }

    // Return an 8-byte hash for 33 to 64 bytes.
    static u64 hash33_64(const char *s, size_t len)
    {
        u64 mul = ACUL_CITYHASH_K2 + len * 2;
        u64 a = load_u64u_le(s) * ACUL_CITYHASH_K2;
        u64 b = load_u64u_le(s + 8);
        u64 c = load_u64u_le(s + len - 24);
        u64 d = load_u64u_le(s + len - 32);
        u64 e = load_u64u_le(s + 16) * ACUL_CITYHASH_K2;
        u64 f = load_u64u_le(s + 24) * 9;
        u64 g = load_u64u_le(s + len - 8);
        u64 h = load_u64u_le(s + len - 16) * mul;
        u64 u = ror64(a + g, 43) + (ror64(b, 30) + c) * 9;
        u64 v = ((a + g) ^ d) + f + 1;
        u64 w = ACUL_BSWAP_64((u + v) * mul) + h;
        u64 x = ror64(e + f, 42) + c;
        u64 y = (ACUL_BSWAP_64((v + w) * mul) + g) * mul;
        u64 z = e + f + c;
        a = ACUL_BSWAP_64((x + z) * mul + y) + b;
        b = shift_mix((z + a) * mul + d + h) * mul;
        return b + x;
    }

    // Based by Google CityHash
    // @ref https://github.com/google/cityhash
    u64 cityhash64_long(const char *s, size_t len)
    {
        if (len <= 32) return hash17_32(s, len);
        else if (len <= 64) return hash33_64(s, len);

        // For strings over 64 bytes we hash the end first, and then as we
        // loop we keep 56 bytes of state: v, w, x, y, and z.
        u64 x = load_u64u_le(s + len - 40);
        u64 y = load_u64u_le(s + len - 16) + load_u64u_le(s + len - 56);
        u64 z = cityhash16(load_u64u_le(s + len - 48) + len, load_u64u_le(s + len - 24));
        pair<u64, u64> v = weak_hash32_with_seeds(s + len - 64, len, z);
        pair<u64, u64> w = weak_hash32_with_seeds(s + len - 32, y + ACUL_CITYHASH_K1, x);
        x = x * ACUL_CITYHASH_K1 + load_u64u_le(s);

        // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
        len = (len - 1) & ~static_cast<size_t>(63);
        do
        {
            x = ror64(x + y + v.first + load_u64u_le(s + 8), 37) * ACUL_CITYHASH_K1;
            y = ror64(y + v.second + load_u64u_le(s + 48), 42) * ACUL_CITYHASH_K1;
            x ^= w.second;
            y += v.first + load_u64u_le(s + 40);
            z = ror64(z + w.first, 33) * ACUL_CITYHASH_K1;
            v = weak_hash32_with_seeds(s, v.second * ACUL_CITYHASH_K1, x + w.first);
            w = weak_hash32_with_seeds(s + 32, z + w.second, y + load_u64u_le(s + 16));
            std::swap(z, x);
            s += 64;
            len -= 64;
        } while (len != 0);
        return cityhash16(cityhash16(v.first, w.first) + shift_mix(y) * ACUL_CITYHASH_K1 + z,
                          cityhash16(v.second, w.second) + x);
    }
} // namespace acul::detail