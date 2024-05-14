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
    APPLIB_API bool operator<(const glm::vec3 &a, const glm::vec3 &b);
    APPLIB_API bool operator==(const glm::vec3 &a, const glm::vec3 &b);

    APPLIB_API bool operator<(const glm::vec2 &a, const glm::vec2 &b);
    APPLIB_API bool operator==(const glm::vec2 &a, const glm::vec2 &b);
} // namespace glm
