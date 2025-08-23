#pragma once

#include <cstdint>

namespace acul
{
    namespace internal
    {
        typedef const uint32_t ctrl_scan_block_size_t;

        typedef struct
        {
            size_t base;
            uint32_t mask;
        } ctrl_scan_state_t;
    } // namespace internal
} // namespace acul