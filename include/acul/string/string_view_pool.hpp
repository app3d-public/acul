#pragma once

#include "../fwd/string_view_pool.hpp"
#include "../type_traits.hpp"
#include "../vector.hpp"
#include "string_view.hpp"


namespace acul
{
    template <typename T, typename Allocator>
    class string_view_pool
    {
        static_assert(is_char_v<T>, "string_view_pool requires a string character type");
        using view_type = basic_string_view<T>;
        using alloc_view_type = typename Allocator::template rebind<view_type>::other;
        using vector_type = vector<view_type, alloc_view_type>;

    public:
        using value_type = T *;
        using reference = view_type &;
        using const_reference = const view_type &;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;

        using iterator = typename vector_type::iterator;
        using const_iterator = typename vector_type::const_iterator;
        using reverse_iterator = typename vector_type::reverse_iterator;
        using const_reverse_iterator = typename vector_type::const_reverse_iterator;

        explicit string_view_pool() : _lines() {}
        explicit string_view_pool(size_type pool_size) : _lines(pool_size) {}

        string_view_pool(const string_view_pool &other) noexcept : _lines(other._lines) {}

        string_view_pool(string_view_pool &&other) noexcept : _lines(std::move(other._lines)) {}

        string_view_pool &operator=(const string_view_pool &other) noexcept
        {
            if (this != &other) _lines = other._lines;
            return *this;
        }

        string_view_pool &operator=(string_view_pool &&other) noexcept
        {
            if (this != &other) _lines = std::move(other._lines);
            return *this;
        }

        reference operator[](size_type index) { return _lines[index]; }

        const_reference operator[](size_type index) const { return _lines[index]; }

        bool operator==(const string_view_pool &other) const { return _lines == other._lines; }

        reference at(size_type index) { return _lines.at(index); }

        const_reference at(size_type index) const { return _lines.at(index); }

        reference front() { return _lines.front(); }

        const_reference front() const { return _lines.front(); }

        reference back() { return _lines.back(); }

        const_reference back() const { return _lines.back(); }

        iterator begin() { return _lines.begin(); }

        const_iterator begin() const { return _lines.begin(); }

        const_iterator cbegin() const { return _lines.cbegin(); }

        iterator end() { return _lines.end(); }

        const_iterator end() const { return _lines.end(); }

        const_iterator cend() const { return _lines.cend(); }

        reverse_iterator rbegin() { return _lines.rbegin(); }

        const_reverse_iterator rbegin() const { return _lines.rbegin(); }

        const_reverse_iterator crbegin() const { return _lines.crbegin(); }

        reverse_iterator rend() { return _lines.rend(); }

        const_reverse_iterator rend() const { return _lines.rend(); }

        const_reverse_iterator crend() const { return _lines.crend(); }

        bool empty() const noexcept { return _lines.empty(); }

        size_type size() const noexcept { return _lines.size(); }

        size_type max_size() const noexcept { return alloc_view_type::max_size(); }

        const_pointer data() const { return _lines.data(); }
        pointer data() { return _lines.data(); }

        void resize(size_type new_size) noexcept { _lines.resize(new_size); }
        void reserve(size_type new_size) noexcept { _lines.reserve(new_size); }

        void clear() { _lines.clear(); }

        void push(const_pointer str, size_type length) noexcept { _lines.emplace_back(str, length); }

        void pop() noexcept { _lines.pop_back(); }

    private:
        vector_type _lines;
    };
} // namespace acul