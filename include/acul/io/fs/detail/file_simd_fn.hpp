#pragma once

#include "../../../detail/simd/flags.hpp"
#include "../../../fwd/string_view_pool.hpp"

namespace acul
{
    namespace fs::detail
    {
        namespace avx2
        {
            void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
        }

        namespace sse42
        {
            void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
        }

        namespace nosimd
        {
            void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst);
        }
    } // namespace fs::detail

    namespace detail
    {

        using PFN_fill_line_buffer = void (*)(const char *data, size_t size, string_view_pool<char> &dst);

        inline PFN_fill_line_buffer load_fill_line_buffer_fn(acul::detail::simd_flags flags)
        {
            if (flags & acul::detail::simd_flags::enum_type::avx2) return &fs::detail::avx2::fill_line_buffer;
            else if (flags & acul::detail::simd_flags::enum_type::sse42) return &fs::detail::sse42::fill_line_buffer;
            return &fs::detail::nosimd::fill_line_buffer;
        }
    } // namespace detail
} // namespace acul