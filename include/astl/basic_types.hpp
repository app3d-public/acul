#ifndef CORE_STD_BASIC_TYPES_H
#define CORE_STD_BASIC_TYPES_H

#include <cstdint>
#include <set>
#include <unordered_set>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <half.hpp>
#pragma GCC diagnostic pop
#include <map>
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

namespace astl
{
    template <typename T>
    using deque = std::deque<T, oneapi::tbb::scalable_allocator<T>>;

    template <typename T>
    using queue = std::queue<T, deque<T>>;

    template <typename T>
    using set = std::set<T, std::less<T>, oneapi::tbb::scalable_allocator<T>>;

    template <typename K, typename V, typename C = std::less<K>>
    using map = std::map<K, V, C, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;

    template <typename K, typename V, typename C = std::less<K>>
    using multimap = std::multimap<K, V, C, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;

    template <typename T>
    using hashset = std::unordered_set<T, std::hash<T>, std::equal_to<T>, oneapi::tbb::scalable_allocator<T>>;

    template <typename K, typename V, typename H = std::hash<K>>
    using hashmap =
        std::unordered_map<K, V, H, std::equal_to<K>, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;

    template <typename K, typename V, typename H = std::hash<K>>
    using multi_hashmap =
        std::unordered_multimap<K, V, H, std::equal_to<K>, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;

    template <typename T = i32>
    struct point2D
    {
        T x;
        T y;
    };

    template <typename T>
    inline bool operator==(const point2D<T> &a, const point2D<T> &b)
    {
        return a.x == b.x && a.y == b.y;
    }

    template <typename T>
    inline bool operator!=(const point2D<T> &a, const point2D<T> &b)
    {
        return !(a == b);
    }

    template <typename T>
    inline bool operator<(const point2D<T> &a, const point2D<T> &b)
    {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    }

    template <typename T>
    inline bool operator>(const point2D<T> &a, const point2D<T> &b)
    {
        return b < a;
    }

    template <typename T>
    inline point2D<T> operator+(const point2D<T> &a, const point2D<T> &b)
    {
        return {a.x + b.x, a.y + b.y};
    }

    template <typename T>
    inline point2D<T> &operator+=(point2D<T> &a, const point2D<T> &b)
    {
        a.x += b.x;
        a.y += b.y;
        return a;
    }

    template <typename T>
    inline point2D<T> operator-(const point2D<T> &a, const point2D<T> &b)
    {
        return {a.x - b.x, a.y - b.y};
    }

    template <typename T>
    inline point2D<T> &operator-=(point2D<T> &a, const point2D<T> &b)
    {
        a.x -= b.x;
        a.y -= b.y;
        return a;
    }
    template <typename T>
    inline point2D<T> operator-(const point2D<T> &a)
    {
        return {-a.x, -a.y};
    }
    template <typename T>
    inline point2D<T> operator*(const point2D<T> &a, i32 b)
    {
        return {a.x * b, a.y * b};
    }
    template <typename T>
    inline point2D<T> operator/(const point2D<T> &a, i32 b)
    {
        return {a.x / b, a.y / b};
    }

    template <typename T>
    class proxy
    {
    public:
        proxy(T *ptr = nullptr) : _ptr(ptr) {}

        T *operator->() { return _ptr; }
        T &operator*() { return *_ptr; }

        operator bool() const { return _ptr != nullptr; }

        void set(T *ptr) { _ptr = ptr; }

        T *get() { return _ptr; }

        void reset() { _ptr = nullptr; }

        void operator=(T *ptr) { _ptr = ptr; }
        bool operator==(T *ptr) { return _ptr == ptr; }
        bool operator!=(T *ptr) { return _ptr != ptr; }

        proxy &operator++()
        {
            ++_ptr;
            return *this;
        }

        proxy operator++(int)
        {
            proxy temp = *this;
            ++_ptr;
            return temp;
        }

        proxy &operator--()
        {
            --_ptr;
            return *this;
        }

        proxy operator--(int)
        {
            proxy temp = *this;
            --_ptr;
            return temp;
        }

    private:
        T *_ptr;
    };
} // namespace astl

#endif