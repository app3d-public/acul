#include <acul/detail/isa/dispatch.hpp>

#if defined(__GNUC__) || defined(__clang__)
    #include <cpuid.h>
#endif

namespace acul::detail
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

        bool os_avx_ready = false;
        bool os_avx512_ready = false;

        if (has_osxsave && has_avx)
        {
            u64 xcr0 = 0;
            u32 eax_x, edx_x;
            __asm__ volatile("xgetbv" : "=a"(eax_x), "=d"(edx_x) : "c"(0));
            xcr0 = ((u64)edx_x << 32) | eax_x;

            os_avx_ready = (xcr0 & 0x06) == 0x06;
            os_avx512_ready = (xcr0 & 0xE0) == 0xE0;
        }

        if (os_avx_ready)
        {
            flags |= isa_flag_bits::avx;
            __cpuid_count(7, 0, info[0], info[1], info[2], info[3]);

            if (info[1] & (1 << 5)) flags |= isa_flag_bits::avx2;
            if (os_avx512_ready && (info[1] & (1 << 16))) flags |= isa_flag_bits::avx512;
        }

        return flags;
    }

    isa_dispatch::isa_dispatch()
    {
        flags = init_flags();
        crc32 = load_crc32_fn(flags);
        fill_line_buffer = load_fill_line_buffer_fn(flags);
    }
} // namespace acul::detail
