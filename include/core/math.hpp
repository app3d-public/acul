#pragma once

#include <glm/fwd.hpp>
#include "api.hpp"
#include "std/basic_types.hpp"

namespace math
{
    /// @brief Covert coordinates in screen space to NDC space
    /// @param screen Vector in screen space [x, y]
    /// @param dimensions Framebuffer dimensions [width, height]
    /// @return Vector in NDC space
    APPLIB_API glm::vec2 screenToNdc(const glm::vec2 &screen, Point2D extent);

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
} // namespace math

namespace glm
{
    APPLIB_API bool operator<(const glm::vec3 &a, const glm::vec3 &b);
    APPLIB_API bool operator==(const glm::vec3 &a, const glm::vec3 &b);

    APPLIB_API bool operator<(const glm::vec2 &a, const glm::vec2 &b);
    APPLIB_API bool operator==(const glm::vec2 &a, const glm::vec2 &b);
} // namespace glm
