#ifndef CORE_STD_BASIC_TYPES_H
#define CORE_STD_BASIC_TYPES_H

#include <cstdint>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <half.hpp>
#pragma GCC diagnostic pop
#include <oneapi/tbb/scalable_allocator.h>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f16 = half_float::half;
using f32 = float;
using f64 = double;
using c8 = char;
using c16 = char16_t;
using c32 = char32_t;

#endif