#ifndef APP_ACUL_CRC32_H
#define APP_ACUL_CRC32_H

#include <random>
#include "../api.hpp"
#include "../scalars.hpp"

namespace acul
{
    struct id_gen
    {
        id_gen() : gen(std::random_device{}()), dist(0, UINT64_MAX) {}

        u64 operator()() { return dist(gen); }

    private:
        std::mt19937_64 gen;
        std::uniform_int_distribution<uint64_t> dist;
    };

    // Based by Google CityHash
    // @ref https://github.com/google/cityhash
    APPLIB_API u64 cityhash64(const char *s, size_t len);

    APPLIB_API u32 crc32(u32 crc, const char *buf, size_t len);
} // namespace acul

namespace std
{
    template <class T>
    inline void hash_combine(std::size_t &seed, const T &v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
} // namespace std

#endif