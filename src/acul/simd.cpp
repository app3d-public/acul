#include <acul/io/path.hpp>
#include <acul/simd.hpp>
#include <cassert>
#include "simd_fallback/declarations.hpp"

#ifndef _WIN32
    #include <dlfcn.h>
    #define LOAD_FUNCTION(name, handle) name = (PFN_##name)dlsym(handle, #name)
using module_t = void *;
#else
    #include <windows.h>
    #define LOAD_FUNCTION(name, handle) name = (PFN_##name)GetProcAddress(handle, #name)
using module_t = HMODULE;
#endif

namespace acul
{
    using PFN_acul_get_simd_flags = u16 (*)();
    using PFN_acul_crc32 = u32 (*)(u32 crc0, const char *buf, size_t len);
    using PFN_acul_io_fill_line_buffer = void (*)(const char *data, size_t size, string_pool<char> &dst);

    struct simd_context
    {
        simd_flags flags;
        module_t handle = nullptr;
        PFN_acul_get_simd_flags acul_get_simd_flags = nullptr;
        PFN_acul_crc32 acul_crc32 = nullptr;
        PFN_acul_io_fill_line_buffer acul_io_fill_line_buffer = nullptr;

        void load_functions();
    } g_simd_ctx{simd_flag_bits::None};

    void init_simd_module()
    {
        g_simd_ctx.flags = simd_flag_bits::Initialized;
#ifdef _WIN32
        g_simd_ctx.handle = LoadLibraryA("libacul-simd.dll");
#else
        g_simd_ctx.handle = dlopen("libacul-simd.so", RTLD_LAZY);
#endif
        if (!g_simd_ctx.handle) return;
        g_simd_ctx.load_functions();
        if (g_simd_ctx.acul_get_simd_flags) g_simd_ctx.flags |= g_simd_ctx.acul_get_simd_flags();
    }

    void simd_context::load_functions()
    {
        LOAD_FUNCTION(acul_get_simd_flags, handle);
        LOAD_FUNCTION(acul_crc32, handle);
        LOAD_FUNCTION(acul_io_fill_line_buffer, handle);
    }

    void terminate_simd_module()
    {
        if (g_simd_ctx.handle)
        {
#ifndef _WIN32
            dlclose(g_simd_ctx.handle);
#endif
            g_simd_ctx.handle = nullptr;
        }
    }

    simd_flags get_simd_flags()
    {
        assert(g_simd_ctx.flags & simd_flag_bits::Initialized);
        return g_simd_ctx.flags;
    }

    APPLIB_API u32 crc32(u32 crc, const char *buf, size_t len)
    {
        if (g_simd_ctx.acul_crc32) return g_simd_ctx.acul_crc32(crc, buf, len);
        return nosimd::crc32(crc, buf, len);
    }

    namespace io
    {
        namespace file
        {
            APPLIB_API void fill_line_buffer(const char *data, size_t size, string_pool<char> &dst)
            {
                if (g_simd_ctx.acul_io_fill_line_buffer)
                {
                    g_simd_ctx.acul_io_fill_line_buffer(data, size, dst);
                    return;
                }
                nosimd::io::fill_line_buffer(data, size, dst);
            }
        } // namespace file
    } // namespace io
} // namespace acul