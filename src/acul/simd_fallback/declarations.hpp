#pragma once

#include <acul/api.hpp>
#include <acul/scalars.hpp>
#include <acul/string/string_pool.hpp>

namespace acul
{
    namespace nosimd
    {
        APPLIB_API u32 crc32(u32 crc, const char *buf, size_t len);

        namespace io
        {
            APPLIB_API void fill_line_buffer(const char *data, size_t size, string_pool<char> &dst);
        }
    } // namespace nosimd
} // namespace acul