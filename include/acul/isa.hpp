#pragma once

#include <acul/symbol_export.h>

namespace acul
{
    // Returns true when CPU + OS context support x86-64-v3 requirements.
    ACUL_EXPORT bool is_x86_64_v3_supported() noexcept;
} // namespace acul
