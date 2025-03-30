#pragma once

#include "../exception.hpp"
#include "../fwd/string_view.hpp"
#include "base.hpp"

namespace acul
{
    template <typename T>
    class basic_string_view
    {
    public:
        using size_type = size_t;
        using value_type = T;
        using pointer = const T *;
        using const_pointer = const T *;
        using reference = const T &;
        using iterator = pointer;
        using const_iterator = pointer;

        constexpr basic_string_view() noexcept : _data(nullptr), _size(0) {}

        constexpr basic_string_view(const_pointer str) noexcept
            : _data(str), _size(str ? null_terminated_length(str) : 0)
        {
        }

        constexpr basic_string_view(const pointer str, size_type len) noexcept : _data(str), _size(len) {}

        constexpr basic_string_view(const basic_string_view &other) noexcept = default;
        constexpr basic_string_view &operator=(const basic_string_view &other) noexcept = default;

        constexpr pointer data() const noexcept { return _data; }
        constexpr size_type size() const noexcept { return _size; }
        constexpr bool empty() const noexcept { return _size == 0; }

        constexpr reference operator[](size_type index) const noexcept { return _data[index]; }

        constexpr reference at(size_type index) const
        {
            if (index >= _size) throw acul::out_of_range(_size, index);
            return _data[index];
        }

        bool operator==(const basic_string_view &other) const noexcept
        {
            size_type len = size();
            return len == other.size() && compare_string(_data, _size, other._data, _size) == 0;
        }

        bool operator!=(const basic_string_view &other) const noexcept { return !(*this == other); }

        constexpr iterator begin() const noexcept { return _data; }
        constexpr iterator end() const noexcept { return _data + _size; }
        constexpr const_iterator cbegin() const noexcept { return _data; }
        constexpr const_iterator cend() const noexcept { return _data + _size; }

        constexpr basic_string_view substr(size_type pos, size_type len = SIZE_MAX) const noexcept
        {
            if (pos >= _size) return {};
            return basic_string_view(_data + pos, len < _size - pos ? len : _size - pos);
        }

        constexpr size_type find(value_type ch, size_type pos = 0) const noexcept
        {
            if (pos >= _size) return SIZE_MAX;
            const_pointer p = static_cast<const_pointer>(memchr(_data + pos, ch, _size - pos));
            return p ? static_cast<size_type>(p - _data) : SIZE_MAX;
        }

        constexpr size_type find(basic_string_view str, size_type pos = 0) const noexcept
        {
            if (pos >= _size || str.size() > _size - pos) return SIZE_MAX;
            const_pointer p = strstr(_data + pos, str.data());
            return p ? static_cast<size_type>(p - _data) : SIZE_MAX;
        }

    private:
        pointer _data;
        size_type _size;
    };

    template <typename T>
    constexpr auto operator<=>(const basic_string_view<T> &a, const basic_string_view<T> &b)
    {
        return compare_string(a.begin(), a.size(), b.begin(), b.size());
    }
} // namespace acul
