#ifndef APP_CORE_CRC32_H
#define APP_CORE_CRC32_H

#include <glm/glm.hpp>
#include "api.hpp"
#include "std/basic_types.hpp"

APPLIB_API u32 crc32(u32 crc, const char *buf, size_t len);

namespace std
{
    template <class T>
    inline void hash_combine(std::size_t &seed, const T &v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <typename T, size_t N>
    struct hash<SArray<T, N>>
    {
        std::size_t operator()(const SArray<T, N> &k) const
        {
            size_t seed = 0;
            for (const auto &v : k) hash_combine(seed, v);
            return seed;
        }
    };

    template <>
    struct hash<glm::ivec3>
    {
        std::size_t operator()(const glm::ivec3 &k) const
        {
            return ((std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1)) >> 1) ^ (std::hash<int>()(k.z) << 1);
        }
    };

    template <>
    struct hash<glm::ivec2>
    {
        std::size_t operator()(const glm::ivec2 &k) const
        {
            // Рекомендуется использовать разные простые числа для каждого поля
            return ((std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1)) >> 1);
        }
    };

    template <>
    struct hash<glm::vec2>
    {
        size_t operator()(const glm::vec2 &vec) const
        {
            size_t h1 = std::hash<f32>()(vec.x);
            size_t h2 = std::hash<f32>()(vec.y);
            return h1 ^ (h2 << 1);
        }
    };

    template <>
    struct hash<glm::vec3>
    {
        size_t operator()(const glm::vec3 &vec) const
        {
            size_t h1 = std::hash<f32>()(vec.x);
            size_t h2 = std::hash<f32>()(vec.y);
            size_t h3 = std::hash<f32>()(vec.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
} // namespace std

#endif