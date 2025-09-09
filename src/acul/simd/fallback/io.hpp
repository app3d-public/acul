#pragma once

#include <acul/api.hpp>
#include <acul/fwd/string_view_pool.hpp>
#include <acul/scalars.hpp>

namespace acul
{
    namespace nosimd
    {
        APPLIB_API u32 crc32(u32 crc, const char *buf, size_t len);

        namespace io
        {
            APPLIB_API void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
        }
    } // namespace nosimd
} // namespace acul