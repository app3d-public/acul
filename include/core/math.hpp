#pragma once

#include <glm/fwd.hpp>
#include "std/basic_types.hpp"
#include "api.hpp"

namespace math
{
    APPLIB_API float dot(const glm::vec3 &a, const glm::vec3 &b);

    APPLIB_API float length2(const glm::vec3 &v);

    APPLIB_API float length(const glm::vec3 &v);

    APPLIB_API glm::vec3 normalize(const glm::vec3 &v);

    /// @brief Covert coordinates in screen space to NDC space
    /// @param screen Vector in screen space [x, y]
    /// @param dimensions Framebuffer dimensions [width, height]
    /// @return Vector in NDC space
    APPLIB_API glm::vec2 screenToNdc(const glm::vec2 &screen, Point2D extent);

    /// @brief Convert coordinates in NDC space to world space
    /// @param ndc Vector in NDC space [x, y]
    /// @param projection Projection matrix
    /// @param viewMatrix View matrix
    /// @return Vector in world space
    APPLIB_API glm::vec3 ndcToWorld(const glm::vec3 &ndc, const glm::mat4 &projection, const glm::mat4 &view);

    /// @brief Axis constants. May be positive or negative
    enum class Axis
    {
        X,
        Y,
        Z
    };
} // namespace math

namespace glm
{
    bool operator<(const glm::vec3 &a, const glm::vec3 &b);
    bool operator==(const glm::vec3 &a, const glm::vec3 &b);

    bool operator<(const glm::vec2 &a, const glm::vec2 &b);
    bool operator==(const glm::vec2 &a, const glm::vec2 &b);
} // namespace glm
