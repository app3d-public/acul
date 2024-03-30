#include <cassert>
#include <core/crc32/crc32.hpp>
#include <core/log.hpp>
#include <string>
#include <windows.h>

#define CPUID_PCLMUL_BIT  (1 << 1)
#define CPUID_SSE42_BIT   (1 << 20)
#define CPUID_AVX512F_BIT (1 << 16)

namespace core
{
    typedef u32 (*PFN_crc32_impl)(u32, const char *, u32);
    PFN_crc32_impl crc32 = nullptr;

    HMODULE simdlib = nullptr;

    bool loadSIMDLib(const std::filesystem::path &parentFolder)
    {
        logInfo("Searching for Host instruction sets");
        u32 eax, ebx, ecx, edx;
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
        int inst{-1};
        if (!(ecx & CPUID_SSE42_BIT))
        {
            logWarn("AVX-512: No");
            logWarn("SSE4.2: No");
            logError("Required instruction set is not supported.");
            return false;
        }

        if (ecx & CPUID_AVX512F_BIT)
        {
            logInfo("AVX-512: Yes");
            inst = 0;
        }
        else
        {
            logWarn("AVX-512: No");
            logInfo("SSE4.2: Yes");
            inst = 2;
        }

        if (ecx & CPUID_PCLMUL_BIT)
        {
            logInfo("PCLMULQDQ: Yes");
            ++inst;
        }
        else
            logWarn("PCLMULQDQ: No");

        std::filesystem::path simdLibPath;
        switch (inst)
        {
            case 0:
                simdLibPath = parentFolder / "libapp3d-simd-avx512.dll";
                break;
            case 1:
                simdLibPath = parentFolder / "libapp3d-simd-avx512-pclmulqdq.dll";
                break;
            case 3:
                simdLibPath = parentFolder / "libapp3d-simd-sse42-pclmulqdq.dll";
                break;
            default:
            case 2:
                simdLibPath = parentFolder / "libapp3d-simd-sse42.dll";
                break;
        }
        simdlib = LoadLibrary(simdLibPath.string().c_str());
        if (simdlib == NULL)
        {
            logError("Failed to load library: %s", simdLibPath.string());
            return false;
        }
        crc32 = reinterpret_cast<PFN_crc32_impl>(GetProcAddress(simdlib, "crc32_impl"));
        if (crc32 == nullptr)
        {
            FreeLibrary(simdlib);
            return false;
        }

        return true;
    }

    void destroySIMDLib()
    {
        if (simdlib != nullptr)
            FreeLibrary(simdlib);
    }
} // namespace core