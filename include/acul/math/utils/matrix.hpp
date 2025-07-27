#pragma once

#include "../types.hpp"

#ifdef ACUL_GLM_ENABLE
    #include "glm/ext/matrix_projection.hpp"
    #include "glm/ext/matrix_transform.hpp"
#endif

#ifdef ACUL_MATH_TYPES
namespace acul
{
    using ACUL_MATH_NAMESPACE::project;
    using ACUL_MATH_NAMESPACE::rotate;
    using ACUL_MATH_NAMESPACE::translate;
    using ACUL_MATH_NAMESPACE::transpose;
} // namespace acul
#endif
