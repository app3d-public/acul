#pragma once
#include "../types.hpp"

#ifdef ACUL_GLM_ENABLE
    #define GLM_ENABLE_EXPERIMENTAL
    #include <glm/gtx/euler_angles.hpp>
    #include <glm/gtx/vector_angle.hpp>
#endif

#ifdef ACUL_MATH_TYPES
namespace acul
{
    using ACUL_MATH_NAMESPACE::extractEulerAngleXYZ;
    using ACUL_MATH_NAMESPACE::extractEulerAngleYXZ;
    using ACUL_MATH_NAMESPACE::extractEulerAngleZYX;
    using ACUL_MATH_NAMESPACE::translate;
    using ACUL_MATH_NAMESPACE::orientedAngle;
} // namespace acul
#endif
