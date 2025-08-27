#pragma once

#include <cassert>
#include "../api.hpp"
#include "../enum.hpp"
#include "../scalars.hpp"
#include "internal/io_callbacks.hpp"

namespace acul
{
    struct simd_flag_bits
    {
        enum enum_type : u16
        {
            none = 0x0000,
            avx512 = 0x0001,
            avx2 = 0x0002,
            avx = 0x0004,
            sse42 = 0x0008,
            pclmul = 0x00010,
            initialized = 0x00020
        };
        using flag_bitmask = std::true_type;
    };

    using simd_flags = flags<simd_flag_bits>;

    APPLIB_API void init_simd_module();

    APPLIB_API void terminate_simd_module();

    namespace internal
    {
        using PFN_get_simd_flags = u16 (*)();

        extern APPLIB_API struct simd_context
        {
            simd_flags flags;
            void *handle;
            PFN_get_simd_flags get_simd_flags;
            ACUL_SIMD_IO_MEMBERS

            void load_functions();
        } g_simd_ctx;
    } // namespace internal

    inline simd_flags get_simd_flags()
    {
        assert(internal::g_simd_ctx.flags & simd_flag_bits::initialized);
        return internal::g_simd_ctx.flags;
    }
} // namespace acul