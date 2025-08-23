#include <acul/api.hpp>
#include <acul/simd/simd.hpp>

extern "C" APPLIB_API uint16_t acul_get_simd_flags()
{
    acul::simd_flags flags;
#ifdef __AVX512__
    flags |= acul::simd_flag_bits::Avx512;
#endif
#ifdef __AVX2__
    flags |= acul::simd_flag_bits::Avx2;
#endif
#ifdef __AVX__
    flags |= acul::simd_flag_bits::Avx;
#endif
#ifdef __SSE4_2__
    flags |= acul::simd_flag_bits::Sse42;
#endif
#ifdef __PCLMUL__
    flags |= acul::simd_flag_bits::Pclmul;
#endif
    return flags;
}