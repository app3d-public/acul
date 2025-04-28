#ifndef ACUL_STD_BASIC_TYPES_H
#define ACUL_STD_BASIC_TYPES_H

#include <cstdint>
#ifdef ACUL_HALF_ENABLE
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"
    #include <half.hpp>
    #pragma GCC diagnostic pop
#endif

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
#ifdef ACUL_HALF_ENABLE
using f16 = half_float::half;
#endif
using f32 = float;
using f64 = double;
using c8 = char;
using c16 = char16_t;
using c32 = char32_t;

#endif