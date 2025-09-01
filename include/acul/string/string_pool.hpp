#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include "../fwd/string_pool.hpp"
#include "../vector.hpp"
#include "base.hpp"

namespace acul
{
    template <typename T>
    struct is_string_char
        : std::disjunction<std::is_same<std::remove_cv_t<T>, char>, std::is_same<std::remove_cv_t<T>, char8_t>,
                           std::is_same<std::remove_cv_t<T>, char16_t>, std::is_same<std::remove_cv_t<T>, char32_t>,
                           std::is_same<std::remove_cv_t<T>, wchar_t>>
    {
    };

    template <typename T>
    inline constexpr bool is_string_char_v = is_string_char<T>::value;

    template <typename T, typename Allocator>
    class string_pool
    {
        static_assert(is_string_char_v<T>, "string_pool requires a string character type");

    public:
        using value_type = T *;
        using reference = T *&;
        using const_reference = const T *&;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;

        class iterator;
        using const_iterator = iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        explicit string_pool(size_type pool_size)
            : _pool_size(pool_size), _data(Allocator::allocate(pool_size)), _pos(0)
        {
        }

        ~string_pool() noexcept { Allocator::deallocate(_data, _pool_size); }

        string_pool(const string_pool &other) noexcept
            : _pool_size(other._pool_size), _data(Allocator::allocate(_pool_size)), _pos(other._pos)
        {
            memcpy(_data, other._data, _pos * sizeof(T));
            _lines = other._lines;
        }

        string_pool(string_pool &&other) noexcept
            : _pool_size(other._pool_size), _data(other._data), _pos(other._pos), _lines(std::move(other._lines))
        {
            other._data = nullptr;
            other._pos = 0;
            other._pool_size = 0;
        }

        string_pool &operator=(const string_pool &other) noexcept
        {
            if (this != &other)
            {
                _pool_size = other._pool_size;
                _data = Allocator::allocate(_pool_size);
                _pos = other._pos;
                memcpy(_data, other._data, _pos * sizeof(T));
                _lines = other._lines;
            }
            return *this;
        }

        string_pool &operator=(string_pool &&other) noexcept
        {
            if (this != &other)
            {
                Allocator::deallocate(_data, _pool_size);
                _pool_size = other._pool_size;
                _data = other._data;
                _pos = other._pos;
                _lines = std::move(other._lines);
                other._data = nullptr;
                other._pos = 0;
                other._pool_size = 0;
            }
            return *this;
        }

        pointer operator[](size_type index) { return _data + _lines[index]; }

        const_pointer operator[](size_type index) const { return _data + _lines[index]; }

        bool operator==(const string_pool &other) const
        {
            if (_pos != other._pos) return false;
            for (size_type i = 0; i < _pos; ++i)
                if (_data[i] != other._data[i]) return false;
            return true;
        }

        pointer at(size_type index) { return _data + _lines[index]; }

        const_pointer at(size_type index) const { return _data + _lines[index]; }

        pointer front() { return _data; }

        const_pointer front() const { return _data; }

        pointer back() { return _data + _pos - 1; }

        const_pointer back() const { return _data + _pos - 1; }

        iterator begin() { return iterator(_lines.data(), _data); }

        const_iterator begin() const { return const_iterator(_lines.data(), _data); }

        const_iterator cbegin() const { return iterator(_lines.data(), _data); }

        iterator end() { return iterator(_lines.data(), _data, _lines.size()); }

        const_iterator end() const { return const_iterator(_lines.data(), _data, _lines.size()); }

        const_iterator cend() const { return iterator(_lines.data(), _data, _lines.size()); }

        reverse_iterator rbegin() { return reverse_iterator(end()); }

        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }

        const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }

        reverse_iterator rend() { return reverse_iterator(begin()); }

        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

        const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

        bool empty() const noexcept { return _pos == 0; }

        size_type size() const noexcept { return _lines.size(); }

        size_type pool_size() const noexcept { return _pool_size; }

        size_type max_size() const noexcept { return Allocator::max_size(); }

        const_pointer data() const { return _data; }
        pointer data() { return _data; }

        void resize(size_type newSize) noexcept
        {
            if (_pool_size > newSize) return;
            _pool_size = newSize;
            _data = Allocator::reallocate(_data, _pool_size);
        }

        void clear()
        {
            _lines.clear();
            _pos = 0;
        }

        void push(const_pointer str, size_type length) noexcept
        {
            if (_pos + length + 1 > _pool_size) resize(_pos + length + 1);
            memcpy(_data + _pos, str, length);
            _data[_pos + length] = '\0';
            _lines.push_back(_pos);
            _pos += (length + 1);
        }

        void pop() noexcept
        {
            _pos -= null_terminated_length(_data + _lines[_pos]) + 1;
            _lines.pop_back();
        }

    private:
        size_type _pool_size;
        pointer _data;
        vector<size_type> _lines;
        size_type _pos;
    };

    template <typename T, typename Allocator>
    class string_pool<T, Allocator>::iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = const T *;
        using difference_type = std::ptrdiff_t;
        using pointer = const T *;
        using reference = const T *;
        using size_type = typename string_pool::size_type;

        iterator(const size_type *lines, pointer base, size_type pos = 0) noexcept
            : _lines(lines), _base(base), _pos(pos)
        {
        }

        reference operator*() const noexcept { return _base + _lines[_pos]; }
        pointer operator->() const noexcept { return _base + _lines[_pos]; }

        iterator &operator++() noexcept
        {
            ++_pos;
            return *this;
        }
        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        iterator &operator--() noexcept
        {
            --_pos;
            return *this;
        }
        iterator operator--(int) noexcept
        {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        iterator &operator+=(difference_type n) noexcept
        {
            _pos += static_cast<size_type>(n);
            return *this;
        }
        iterator &operator-=(difference_type n) noexcept
        {
            _pos -= static_cast<size_type>(n);
            return *this;
        }
        friend iterator operator+(iterator it, difference_type n) noexcept
        {
            it += n;
            return it;
        }
        friend iterator operator+(difference_type n, iterator it) noexcept
        {
            it += n;
            return it;
        }
        friend iterator operator-(iterator it, difference_type n) noexcept
        {
            it -= n;
            return it;
        }
        friend difference_type operator-(const iterator &a, const iterator &b) noexcept
        {
            return static_cast<difference_type>(a._pos) - static_cast<difference_type>(b._pos);
        }

        reference operator[](difference_type off) const noexcept { return _base + _lines[_pos + off]; }

        friend bool operator==(const iterator &a, const iterator &b) noexcept
        {
            return a._base == b._base && a._lines == b._lines && a._pos == b._pos;
        }
        friend bool operator!=(const iterator &a, const iterator &b) noexcept { return !(a == b); }
        friend bool operator<(const iterator &a, const iterator &b) noexcept { return a._pos < b._pos; }
        friend bool operator>(const iterator &a, const iterator &b) noexcept { return a._pos > b._pos; }
        friend bool operator<=(const iterator &a, const iterator &b) noexcept { return a._pos <= b._pos; }
        friend bool operator>=(const iterator &a, const iterator &b) noexcept { return a._pos >= b._pos; }

    private:
        const size_type *_lines;
        pointer _base;
        size_type _pos;
    };

} // namespace acul