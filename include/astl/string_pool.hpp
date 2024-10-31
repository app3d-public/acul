#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include "vector.hpp"

namespace astl
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

    template <typename T, template <typename> class allocator_base_t = mem_allocator>
    class string_pool
    {
        static_assert(is_string_char_v<T>, "string_pool requires a string character type");

    public:
        using allocator_t = allocator_base_t<T>;
        static allocator_t allocator;
        using value_type = T *;
        using reference = T *&;
        using const_reference = const T *&;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;

        class Iterator;
        using iterator = Iterator;
        using const_iterator = const Iterator;
        using reverse_iterator = std::reverse_iterator<Iterator>;
        using const_reverse_iterator = std::reverse_iterator<Iterator>;

        explicit string_pool(size_type poolSize) : _poolSize(poolSize), _data(allocator.allocate(poolSize)), _pos(0) {}

        ~string_pool() noexcept { allocator.deallocate(_data, _poolSize); }

        string_pool(const string_pool &other) noexcept
            : _poolSize(other._poolSize), _data(allocator.allocate(_poolSize)), _pos(other._pos)
        {
            memcpy(_data, other._data, _pos * sizeof(T));
            _lines = other._lines;
        }

        string_pool(string_pool &&other) noexcept
            : _poolSize(other._poolSize), _data(other._data), _pos(other._pos), _lines(std::move(other._lines))
        {
            other._data = nullptr;
            other._pos = 0;
            other._poolSize = 0;
        }

        string_pool &operator=(const string_pool &other) noexcept
        {
            if (this != &other)
            {
                _poolSize = other._poolSize;
                _data = allocator.allocate(_poolSize);
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
                allocator.deallocate(_data, _poolSize);
                _poolSize = other._poolSize;
                _data = other._data;
                _pos = other._pos;
                _lines = std::move(other._lines);
                other._data = nullptr;
                other._pos = 0;
                other._poolSize = 0;
            }
            return *this;
        }

        pointer operator[](size_type index) { return _data + _lines[index]; }

        const_pointer operator[](size_type index) const { return _data[_lines[index]]; }

        bool operator==(const string_pool &other) const
        {
            if (_pos != other._pos) return false;
            for (size_type i = 0; i < _pos; ++i)
                if (_data[i] != other._data[i]) return false;
            return true;
        }

        pointer at(size_type index) { return _data + _lines[index]; }

        const_pointer at(size_type index) const { return _data[_lines[index]]; }

        pointer front() { return _data; }

        const_pointer front() const { return _data; }

        pointer back() { return _data + _pos - 1; }

        const_pointer back() const { return _data + _pos - 1; }

        iterator begin() { return iterator(_lines, _data); }

        const_iterator begin() const { return iterator(_lines, _data); }

        const_iterator cbegin() const { return iterator(_lines, _data); }

        iterator end() { return iterator(_lines, _data, _lines.size()); }

        const_iterator end() const { return iterator(_lines, _data, _lines.size()); }

        const_iterator cend() const { return iterator(_lines, _data, _lines.size()); }

        reverse_iterator rbegin() { return reverse_iterator(end()); }

        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }

        const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }

        reverse_iterator rend() { return reverse_iterator(begin()); }

        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

        const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

        bool empty() const noexcept { return _pos == 0; }

        size_type size() const noexcept { return _lines.size(); }

        size_type poolSize() const noexcept { return _poolSize; }

        size_type max_size() const noexcept { return allocator.max_size(); }

        const_pointer data() const { return _data; }
        pointer data() { return _data; }

        void resize(size_type newSize) noexcept
        {
            if (_poolSize > newSize) return;
            _poolSize = newSize;
            _data = allocator.reallocate(_data, _poolSize);
        }

        void clear()
        {
            _lines.clear();
            _pos = 0;
        }

        void push(const_pointer str, size_t length) noexcept
        {
            const size_t newLength = _pos + length + 1;
            if (newLength > _poolSize) resize(newLength);
            memcpy(_data + _pos, str, length);
            _data[_pos + length] = '\0';
            _lines.push_back(_pos);
            _pos = newLength;
        }

        void pop() noexcept
        {
            _pos -= strlen(_data + _lines[_pos]) + 1;
            _lines.pop_back();
        }

    private:
        size_type _poolSize;
        T *_data;
        vector<size_type> _lines;
        size_type _pos;
    };

    template <typename T, template <typename> class allocator_base_t>
    typename string_pool<T, allocator_base_t>::allocator_t string_pool<T, allocator_base_t>::allocator{};

    template <typename T, template <typename> class Allocator>
    class string_pool<T, Allocator>::Iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using const_pointer = const T *;
        using reference = T &;
        using const_reference = const T &;

        Iterator(vector<size_t> &lines, pointer ptr = nullptr, size_t pos = 0) : _ptr(ptr), _lines(lines), _pos(pos) {}

        pointer operator*() { return _ptr + _lines[_pos]; }
        pointer operator->() { return _ptr + _lines[_pos]; }
        pointer operator->() const { return _ptr + _lines[_pos]; }

        Iterator &operator++()
        {
            ++_pos;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        Iterator &operator--()
        {
            --_pos;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        friend bool operator==(const Iterator &a, const Iterator &b) { return a._ptr == b._ptr && a._pos == b._pos; }
        friend bool operator!=(const Iterator &a, const Iterator &b) { return a._ptr != b._ptr || a._pos != b._pos; }

        friend bool operator<(const Iterator &a, const Iterator &b) { return a._ptr < b._ptr || a._pos < b._pos; }
        friend bool operator>(const Iterator &a, const Iterator &b) { return a._ptr > b._ptr || a._pos > b._pos; }
        friend bool operator<=(const Iterator &a, const Iterator &b) { return a._ptr <= b._ptr || a._pos > b._pos; }
        friend bool operator>=(const Iterator &a, const Iterator &b) { return a._ptr >= b._ptr || a._pos > b._pos; }

        reference operator[](difference_type offset) const { return *(*this + offset); }

    private:
        pointer _ptr;
        vector<size_t> _lines;
        size_t _pos;
    };

} // namespace astl