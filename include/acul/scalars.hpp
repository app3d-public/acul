#ifndef ACUL_STD_BASIC_TYPES_H
#define ACUL_STD_BASIC_TYPES_H

#include <cstdint>
#include "api.hpp"

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
using c8 = char;
using c16 = char16_t;
using c32 = char32_t;

static ACUL_FORCEINLINE unsigned ctz32(u32 x)
{
#if defined(_MSC_VER)
    unsigned long r;
    _BitScanForward(&r, x);
    return (unsigned)r;
#else
    return (unsigned)__builtin_ctz(x);
#endif
}

static ACUL_FORCEINLINE u32 pop_lsb(u32 &m)
{
    u32 r = ctz32(m);
    m &= (m - 1);
    return r;
}

#endif