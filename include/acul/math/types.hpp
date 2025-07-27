#pragma once

#include <limits>
#include <type_traits>
#ifdef ACUL_GLM_ENABLE
    #define ACUL_MATH_TYPES
    #include <glm/glm.hpp>
    #include "../scalars.hpp"
    #define ACUL_MATH_NAMESPACE glm
#endif

namespace acul
{
    enum class Axis
    {
        x,
        y,
        z
    };

#ifdef ACUL_GLM_ENABLE
    using ivec2 = glm::ivec2;
    using ivec3 = glm::ivec3;
    using ivec4 = glm::ivec4;
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using mat3 = glm::mat3;
    using mat4 = glm::mat4;

    // inline bool operator<(const vec3 &a, const vec3 &b)
    // {
    //     if (a.x != b.x) return a.x < b.x;
    //     if (a.y != b.y) return a.y < b.y;
    //     return a.z < b.z;
    // }

    // inline bool operator==(const vec3 &a, const vec3 &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

    // inline bool operator<(const vec2 &a, const vec2 &b)
    // {
    //     if (a.x != b.x) return a.x < b.x;
    //     if (a.y != b.y) return a.y < b.y;
    //     return false;
    // }
    // inline bool operator==(const vec2 &a, const vec2 &b) { return a.x == b.x && a.y == b.y; }

    template <typename T>
    struct is_glm_vector : std::false_type
    {
    };

    template <glm::length_t L, typename T, glm::qualifier Q>
    struct is_glm_vector<glm::vec<L, T, Q>> : std::true_type
    {
    };

    template <typename T>
    struct is_glm_matrix : std::false_type
    {
    };

    template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
    struct is_glm_matrix<glm::mat<C, R, T, Q>> : std::true_type
    {
    };

    template <typename T>
    struct is_glm_quaternion : std::false_type
    {
    };

    template <typename T, glm::qualifier Q>
    struct is_glm_quaternion<glm::qua<T, Q>> : std::true_type
    {
    };
#endif

    template <typename T>
    constexpr T get_type_min() noexcept
    {
        if constexpr (std::is_arithmetic_v<T>) return std::numeric_limits<T>::lowest();
#ifdef ACUL_GLM_ENABLE
        else if constexpr (is_glm_vector<T>::value || is_glm_matrix<T>::value)
        {
            using Scalar = typename T::value_type;
            return T(std::numeric_limits<Scalar>::lowest());
        }
        else if constexpr (is_glm_quaternion<T>::value)
        {
            using Scalar = typename T::value_type;
            return T(std::numeric_limits<Scalar>::lowest(), 0, 0, 0);
        }
        else
#endif
            static_assert(sizeof(T) == 0, "get_type_min is not implemented for this type");
    }

    template <typename T>
    constexpr T get_type_max() noexcept
    {
        if constexpr (std::is_arithmetic_v<T>) return std::numeric_limits<T>::max();
#ifdef ACUL_GLM_ENABLE
        else if constexpr (is_glm_vector<T>::value || is_glm_matrix<T>::value)
        {
            using Scalar = typename T::value_type;
            return T(std::numeric_limits<Scalar>::max());
        }
        else if constexpr (is_glm_quaternion<T>::value)
        {
            using Scalar = typename T::value_type;
            return T(std::numeric_limits<Scalar>::max(), 0, 0, 0);
        }
#endif
        else
            static_assert(sizeof(T) == 0, "get_type_max is not implemented for this type");
    }

    template <typename T>
    struct min_max
    {
        T min{get_type_max<T>()};
        T max{get_type_min<T>()};
    };
} // namespace acul

#ifdef ACUL_MATH_TYPES
namespace std
{
    template <>
    struct hash<acul::ivec3>
    {

        std::size_t operator()(const acul::ivec3 &k) const
        {
            return ((std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1)) >> 1) ^ (std::hash<int>()(k.z) << 1);
        }
    };

    template <>
    struct hash<acul::ivec2>
    {
        std::size_t operator()(const acul::ivec2 &k) const
        {
            return ((std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1)) >> 1);
        }
    };

    template <>
    struct hash<acul::vec2>
    {
        size_t operator()(const acul::vec2 &vec) const
        {
            size_t h1 = std::hash<f32>()(vec.x);
            size_t h2 = std::hash<f32>()(vec.y);
            return h1 ^ (h2 << 1);
        }
    };

    template <>
    struct hash<acul::vec3>
    {
        size_t operator()(const acul::vec3 &vec) const
        {
            size_t h1 = std::hash<f32>()(vec.x);
            size_t h2 = std::hash<f32>()(vec.y);
            size_t h3 = std::hash<f32>()(vec.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
} // namespace std
#endif