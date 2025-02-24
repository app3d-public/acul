#pragma once

#include <glm/fwd.hpp>
#include "../astl/point2d.hpp"
#include "api.hpp"

namespace math
{
    /// @brief Covert coordinates in screen space to NDC space
    /// @param screen Vector in screen space [x, y]
    /// @param dimensions Framebuffer dimensions [width, height]
    /// @return Vector in NDC space
    APPLIB_API glm::vec2 screenToNdc(const glm::vec2 &screen, astl::point2D<i32> extent);

    // Creates a ray in world space from NDC coordinates.
    // @param ndcPos Coordinates in NDC space [x, y]
    // @param viewInverse Inverse view matrix
    // @param projection Projection matrix
    // @return A pair of the ray's origin and direction
    APPLIB_API std::pair<glm::vec3, glm::vec3> createRay(const glm::vec2 &ndcPos, const glm::mat4 &viewInverse,
                                                         const glm::mat4 &projection);

    // Intersects a ray with a plane and computes the intersection point if it exists.
    // @param rayOrigin The origin of the ray
    // @param rayDirection The direction of the ray
    // @param planeNormal The normal of the plane
    // @param planeDistance The distance from the origin to the plane along the normal
    // @param intersection Output parameter to store the intersection point coordinates
    // @return true if an intersection exists, otherwise false
    APPLIB_API bool intersectRayWithPlane(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                                          const glm::vec3 &planeNormal, f32 planeDistance, glm::vec3 &intersection);

    /// @brief Convert coordinates in NDC space to world space
    /// @param ndc Vector in NDC space [x, y]
    /// @param projection Projection matrix
    /// @param viewInverse Inverse view matrix
    /// @return Vector in world space
    APPLIB_API glm::vec3 ndcToWorld(const glm::vec2 &ndc, const glm::mat4 &projection, const glm::mat4 &viewInverse);

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
    APPLIB_API f32 round10(f32 value);

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

namespace glm
{
    APPLIB_API bool operator<(const glm::vec3 &a, const glm::vec3 &b);
    APPLIB_API bool operator==(const glm::vec3 &a, const glm::vec3 &b);

    APPLIB_API bool operator<(const glm::vec2 &a, const glm::vec2 &b);
    APPLIB_API bool operator==(const glm::vec2 &a, const glm::vec2 &b);
} // namespace glm
