#include <acul/io/path.hpp>
#include <cassert>
#include "crc32.hpp"
#include "fallback/io.hpp"

namespace acul
{
    namespace internal
    {
        simd_context get_default_ctx()
        {
            return {.flags = {},
                    .handle = nullptr,
                    .get_simd_flags = nullptr,
                    .crc32 = nosimd::crc32,
                    .fill_line_buffer = nosimd::io::fill_line_buffer};
        }

        simd_context g_simd_ctx = get_default_ctx();

        bool check_io() { return g_simd_ctx.crc32 && g_simd_ctx.fill_line_buffer; }

        void simd_context::load_functions()
        {
            crc32 = internal::load_crc32_fn(flags);
            fill_line_buffer = internal::load_fill_line_buffer_fn(flags);
        }
    } // namespace internal

    void init_simd_module()
    {
        internal::g_simd_ctx.flags = simd_flag_bits::initialized;
#ifdef _WIN32
        internal::g_simd_ctx.handle = LoadLibraryA("libacul-simd.dll");
#else
        internal::g_simd_ctx.handle = dlopen("libacul-simd.so", RTLD_LAZY);
#endif
        if (!internal::g_simd_ctx.handle) return;
        internal::g_simd_ctx.load_functions();
        if (!internal::g_simd_ctx.get_simd_flags || !internal::check_io())
        {
            internal::g_simd_ctx = internal::get_default_ctx();
            return;
        }
        internal::g_simd_ctx.flags |= internal::g_simd_ctx.get_simd_flags();
    }
} // namespace acul