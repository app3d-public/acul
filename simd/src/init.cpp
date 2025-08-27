#include <acul/api.hpp>
#include <acul/simd/simd.hpp>

extern "C" APPLIB_API uint16_t acul_get_simd_flags()
{
    acul::simd_flags flags;
#ifdef __AVX512__
    flags |= acul::simd_flag_bits::avx512;
#endif
#ifdef __AVX2__
    flags |= acul::simd_flag_bits::avx2;
#endif
#ifdef __AVX__
    flags |= acul::simd_flag_bits::avx;
#endif
#ifdef __SSE4_2__
    flags |= acul::simd_flag_bits::sse42;
#endif
#ifdef __PCLMUL__
    flags |= acul::simd_flag_bits::pclmul;
#endif
    return flags;
}