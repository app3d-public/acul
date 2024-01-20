#ifndef CORE_STD_BASIC_TYPES_H
#define CORE_STD_BASIC_TYPES_H

#include <cstdint>
#include <half.hpp>

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

struct Point2D
{
    i32 x;
    i32 y;
};

inline bool operator==(const Point2D &a, const Point2D &b) { return a.x == b.x && a.y == b.y; }

inline bool operator!=(const Point2D &a, const Point2D &b) { return !(a == b); }

inline bool operator<(const Point2D &a, const Point2D &b) { return a.x < b.x || (a.x == b.x && a.y < b.y); }

inline bool operator>(const Point2D &a, const Point2D &b) { return b < a; }

inline Point2D operator+(const Point2D &a, const Point2D &b) { return {a.x + b.x, a.y + b.y}; }

inline Point2D operator-(const Point2D &a, const Point2D &b) { return {a.x - b.x, a.y - b.y}; }

inline Point2D operator-(const Point2D &a) { return {-a.x, -a.y}; }

inline Point2D operator*(const Point2D &a, i32 b) { return {a.x * b, a.y * b}; }

inline Point2D operator/(const Point2D &a, i32 b) { return {a.x / b, a.y / b}; }

#endif