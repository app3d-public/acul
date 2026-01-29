#pragma once

#include "../../detail/isa/flags.hpp"
#include "../../fwd/string_view_pool.hpp"

namespace acul::detail
{
    namespace avx2
    {
        void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
    }

    namespace sse42
    {
        void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
    }

    namespace scalar
    {
        void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
    }

    using PFN_fill_line_buffer = void (*)(const char *data, size_t size, string_view_pool<char> &dst);

    inline PFN_fill_line_buffer load_fill_line_buffer_fn(isa_flags flags)
    {
        if (flags & isa_flag_bits::avx2) return &avx2::fill_line_buffer;
        else if (flags & isa_flag_bits::sse42) return &sse42::fill_line_buffer;
        return &scalar::fill_line_buffer;
    }
} // namespace acul::detail
