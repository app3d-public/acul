#include <acul/detail/isa/dispatch.hpp>

#if defined(__x86_64__) || defined(_M_X64)
    #if defined(__GNUC__) || defined(__clang__)
        #include <cpuid.h>
    #endif
#endif

namespace acul
{
#if defined(__x86_64__) || defined(_M_X64)
    static inline u64 get_xcr0() noexcept
    {
        u32 eax = 0;
        u32 edx = 0;
        __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
        return (static_cast<u64>(edx) << 32) | eax;
    }

    static inline bool is_avx_ready(u64 xcr0) noexcept { return (xcr0 & 0x06u) == 0x06u; }

    static inline bool is_avx512_ready(u64 xcr0) noexcept { return (xcr0 & 0xE0u) == 0xE0u; }

    static inline bool get_leaf7(u32 &eax, u32 &ebx, u32 &ecx, u32 &edx) noexcept
    {
    #if defined(__GNUC__) || defined(__clang__)
        if (__get_cpuid_max(0, nullptr) < 7) return false;
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        return true;
    #else
        return false;
    #endif
    }
#endif

    namespace detail
    {
        isa_dispatch g_isa_dispatcher{};

        isa_flags init_flags()
        {
            isa_flags flags = isa_flag_bits::none;
            u32 info[4] = {0};
            __get_cpuid(1, &info[0], &info[1], &info[2], &info[3]);

            const bool has_sse42 = (info[2] & (1 << 20)) != 0;
            const bool has_pclmul = (info[2] & (1 << 1)) != 0;
            const bool has_osxsave = (info[2] & (1 << 27)) != 0;
            const bool has_avx = (info[2] & (1 << 28)) != 0;

            if (has_sse42) flags |= isa_flag_bits::sse42;
            if (has_pclmul) flags |= isa_flag_bits::pclmul;

            bool is_avx_ready = false;
            bool is_avx512_ready = false;

            if (has_osxsave && has_avx)
            {
                u64 xcr0 = get_xcr0();
                is_avx_ready = acul::is_avx_ready(xcr0);
                is_avx512_ready = acul::is_avx512_ready(xcr0);
            }

            if (is_avx_ready)
            {
                flags |= isa_flag_bits::avx;
                if (!get_leaf7(info[0], info[1], info[2], info[3])) return flags;

                if (info[1] & (1 << 5)) flags |= isa_flag_bits::avx2;
                if (is_avx512_ready && (info[1] & (1 << 16))) flags |= isa_flag_bits::avx512;
            }

            return flags;
        }

        isa_dispatch::isa_dispatch()
        {
            flags = init_flags();
            crc32 = load_crc32_fn(flags);
            fill_line_buffer = load_fill_line_buffer_fn(flags);
        }
    } // namespace detail

#if defined(__x86_64__) || defined(_M_X64)
    APPLIB_API bool is_x86_64_v3_supported() noexcept
    {
    #if defined(__GNUC__) || defined(__clang__)
        unsigned int eax = 0;
        unsigned int ebx = 0;
        unsigned int ecx = 0;
        unsigned int edx = 0;

        if (__get_cpuid_max(0, nullptr) < 7) return false;
        if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return false;

        const bool has_ssse3 = (ecx & (1u << 9)) != 0;
        const bool has_fma = (ecx & (1u << 12)) != 0;
        const bool has_cx16 = (ecx & (1u << 13)) != 0;
        const bool has_sse42 = (ecx & (1u << 20)) != 0;
        const bool has_movbe = (ecx & (1u << 22)) != 0;
        const bool has_popcnt = (ecx & (1u << 23)) != 0;
        const bool has_osxsave = (ecx & (1u << 27)) != 0;
        const bool has_avx = (ecx & (1u << 28)) != 0;
        const bool has_f16c = (ecx & (1u << 29)) != 0;

        if (!(has_ssse3 && has_fma && has_cx16 && has_sse42 && has_movbe && has_popcnt && has_osxsave && has_avx &&
              has_f16c))
            return false;

        if (!is_avx_ready(get_xcr0())) return false;

        if (!get_leaf7(eax, ebx, ecx, edx)) return false;
        const bool has_bmi1 = (ebx & (1u << 3)) != 0;
        const bool has_avx2 = (ebx & (1u << 5)) != 0;
        const bool has_bmi2 = (ebx & (1u << 8)) != 0;
        if (!(has_bmi1 && has_avx2 && has_bmi2)) return false;

        if (__get_cpuid_max(0x80000000, nullptr) < 0x80000001u) return false;
        if (!__get_cpuid(0x80000001u, &eax, &ebx, &ecx, &edx)) return false;
        const bool has_lzcnt = (ecx & (1u << 5)) != 0;
        return has_lzcnt;
    #else
        return false;
    #endif
    }
#else
    APPLIB_API bool is_x86_64_v3_supported() noexcept { return false; }
#endif
} // namespace acul
