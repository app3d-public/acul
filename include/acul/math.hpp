#pragma once

#include <glm/glm.hpp>
#include "api.hpp"
#include "pair.hpp"

namespace acul
{
    namespace math
    {
        /// @brief Covert coordinates in screen space to NDC space
        /// @param screen Vector in screen space [x, y]
        /// @param dimensions Framebuffer dimensions [width, height]
        /// @return Vector in NDC space
        inline glm::vec2 screen_to_ndc(const glm::vec2 &screen, point2D<i32> extent)
        {
            f32 xNdc = (screen.x / extent.x) * 2.0f - 1;
            f32 yNdc = 1.0f - (screen.y / extent.y) * 2.0f;
            return {xNdc, yNdc};
        }

        // Creates a ray in world space from NDC coordinates.
        // @param ndc_pos Coordinates in NDC space [x, y]
        // @param view_inverse Inverse view matrix
        // @param projection Projection matrix
        // @return A pair of the ray's origin and direction
        APPLIB_API std::pair<glm::vec3, glm::vec3> create_ray(const glm::vec2 &ndc_pos, const glm::mat4 &view_inverse,
                                                              const glm::mat4 &projection);

        // Intersects a ray with a plane and computes the intersection point if it exists.
        // @param ray_origin The origin of the ray
        // @param ray_direction The direction of the ray
        // @param plane_normal The normal of the plane
        // @param plane_distance The distance from the origin to the plane along the normal
        // @param intersection Output parameter to store the intersection point coordinates
        // @return true if an intersection exists, otherwise false
        inline bool intersect_ray_with_plane(const glm::vec3 &ray_origin, const glm::vec3 &ray_direction,
                                             const glm::vec3 &plane_normal, f32 plane_distance, glm::vec3 &intersection)
        {
            f32 denom = glm::dot(ray_direction, plane_normal);
            if (fabs(denom) > std::numeric_limits<f32>::epsilon())
            {
                f32 t = (plane_distance - glm::dot(ray_origin, plane_normal)) / denom;
                intersection = ray_origin + t * ray_direction;
                return t >= 0;
            }
            return false;
        }

        /// @brief Convert coordinates in NDC space to world space
        /// @param ndc Vector in NDC space [x, y]
        /// @param projection Projection matrix
        /// @param view_inverse Inverse view matrix
        /// @return Vector in world space
        inline glm::vec3 ndc_to_world(const glm::vec2 &ndc, const glm::mat4 &projection, const glm::mat4 &view_inverse)
        {
            glm::vec3 planeNormal = glm::normalize(glm::vec3{-view_inverse[2]});
            auto [rayOrigin, rayDirection] = create_ray({ndc.x, -ndc.y}, view_inverse, projection);
            glm::vec3 intersection;
            if (intersect_ray_with_plane(rayOrigin, rayDirection, planeNormal, 0.0f, intersection))
                return -intersection;
            return {0.0f, 0.0f, 0.0f};
        }

        /// @brief Axis constants. May be positive or negative
        enum class Axis
        {
            X,
            Y,
            Z
        };

        /**
         * Rounds a floating-point number to the nearest power of 10.
         * @param value The value to be rounded.
         * @return The rounded value.
         */
        constexpr f32 round10(f32 value)
        {
            if (value == 0) return 0;
            f32 sign = (value > 0) ? 1.0f : -1.0f;
            value = abs(value);
            int exponent = static_cast<int>(round(log10(value)));
            f32 base = pow(10, exponent);
            return base * sign;
        }

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

        template <typename T>
        constexpr T get_type_min() noexcept
        {
            if constexpr (std::is_arithmetic_v<T>)
                return std::numeric_limits<T>::lowest();
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
                static_assert(sizeof(T) == 0, "get_type_min is not implemented for this type");
        }

        template <typename T>
        constexpr T get_type_max() noexcept
        {
            if constexpr (std::is_arithmetic_v<T>)
                return std::numeric_limits<T>::max();
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
            else
                static_assert(sizeof(T) == 0, "get_type_max is not implemented for this type");
        }

        template <typename T>
        struct min_max
        {
            T min{get_type_max<T>()};
            T max{get_type_min<T>()};
        };
    } // namespace math
} // namespace acul

namespace glm
{
    inline bool operator<(const vec3 &a, const vec3 &b)
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }

    inline bool operator==(const vec3 &a, const vec3 &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

    inline bool operator<(const vec2 &a, const vec2 &b)
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return false;
    }
    inline bool operator==(const vec2 &a, const vec2 &b) { return a.x == b.x && a.y == b.y; }
} // namespace glm

namespace std
{
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