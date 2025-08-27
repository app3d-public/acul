#include <acul/io/path.hpp>
#include <acul/simd/simd.hpp>
#include <cassert>
#include "fallback/io.hpp"

#ifndef _WIN32
    #include <dlfcn.h>
    #define LOAD_FUNCTION(name, handle) name = (PFN_##name)dlsym(handle, "acul_" #name)
using module_t = void *;
#else
    #include <windows.h>
    #define LOAD_FUNCTION(name, handle) name = (PFN_##name)GetProcAddress(handle, "acul_" #name)
using module_t = HMODULE;
#endif

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
            // IO
            LOAD_FUNCTION(get_simd_flags, (module_t)handle);
            LOAD_FUNCTION(crc32, (module_t)handle);
            LOAD_FUNCTION(fill_line_buffer, (module_t)handle);
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
            terminate_simd_module();
            internal::g_simd_ctx = internal::get_default_ctx();
            return;
        }
        internal::g_simd_ctx.flags |= internal::g_simd_ctx.get_simd_flags();
    }

    void terminate_simd_module()
    {
        if (internal::g_simd_ctx.handle)
        {
#ifndef _WIN32
            dlclose(internal::g_simd_ctx.handle);
#else
            FreeLibrary((HMODULE)internal::g_simd_ctx.handle);
#endif
            internal::g_simd_ctx.handle = nullptr;
        }
    }
} // namespace acul