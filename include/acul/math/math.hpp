#pragma once

#include "../scalars.hpp"
#include "types.hpp"
#ifdef ACUL_MATH_TYPES
    #include "../pair.hpp"

    #ifdef ACUL_GLM_ENABLE
        #include <glm/ext/scalar_constants.hpp>
        #include <glm/gtc/constants.hpp>
    #endif
#endif

namespace acul
{
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

#ifdef ACUL_MATH_TYPES
    using ACUL_MATH_NAMESPACE::cos;
    using ACUL_MATH_NAMESPACE::cross;
    using ACUL_MATH_NAMESPACE::distance;
    using ACUL_MATH_NAMESPACE::dot;
    using ACUL_MATH_NAMESPACE::half_pi;
    using ACUL_MATH_NAMESPACE::inverse;
    using ACUL_MATH_NAMESPACE::length;
    using ACUL_MATH_NAMESPACE::max;
    using ACUL_MATH_NAMESPACE::min;
    using ACUL_MATH_NAMESPACE::normalize;
    using ACUL_MATH_NAMESPACE::pi;
    using ACUL_MATH_NAMESPACE::radians;
    using ACUL_MATH_NAMESPACE::sin;
    using ACUL_MATH_NAMESPACE::sqrt;

    /// @brief Covert coordinates in screen space to NDC space
    /// @param screen Vector in screen space [x, y]
    /// @param dimensions Framebuffer dimensions [width, height]
    /// @return Vector in NDC space
    inline glm::vec2 screen_to_ndc(const glm::vec2 &screen, point2D<i32> extent)
    {
        f32 x_ndc = (screen.x / extent.x) * 2.0f - 1;
        f32 y_ndc = 1.0f - (screen.y / extent.y) * 2.0f;
        return {x_ndc, y_ndc};
    }

    inline acul::vec3 normal_by_axis(Axis axis)
    {
        switch (axis)
        {
            case Axis::x:
                return acul::vec3{1.0f, 0.0f, 0.0f};
            case Axis::y:
                return acul::vec3{0.0f, 1.0f, 0.0f};
            case Axis::z:
                return acul::vec3{0.0f, 0.0f, 1.0f};
            default:
                return acul::vec3{0.0f};
        }
    }
#endif
} // namespace acul
