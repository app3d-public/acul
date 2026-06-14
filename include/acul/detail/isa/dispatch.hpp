#pragma once

#include <acul/symbol_export.h>
#include "../../hash/detail/crc32_isa_fn.hpp"
#include "../../string/detail/string_isa_fn.hpp"
#include "flags.hpp"

namespace acul::detail
{
    extern ACUL_EXPORT struct isa_dispatch
    {
        isa_flags flags;
        PFN_crc32 crc32;
        PFN_fill_line_buffer fill_line_buffer;

        isa_dispatch();
    } g_isa_dispatcher;
} // namespace acul::detail