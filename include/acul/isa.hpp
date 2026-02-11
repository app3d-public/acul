#pragma once

#include "api.hpp"

namespace acul
{
    // Returns true when CPU + OS context support x86-64-v3 requirements.
    APPLIB_API bool is_x86_64_v3_supported() noexcept;
} // namespace acul
