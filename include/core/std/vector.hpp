#ifndef APP_CORE_STD_VECTOR_H
#define APP_CORE_STD_VECTOR_H

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <oneapi/tbb/scalable_allocator.h>
#include <stdexcept>
#include <type_traits>
#include "../mem/allocator.hpp"

namespace astl
{
    template <typename T, template <typename> class allocator_base_t = mem_allocator>
    class vector
    {
    public:
        using allocator_t = allocator_base_t<T>;
        static allocator_t allocator;
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = typename allocator_t::pointer;
        using const_pointer = const typename allocator_t::pointer;
        using size_type = size_t;

        class Iterator;
        using iterator = Iterator;
        using const_iterator = const Iterator;
        using reverse_iterator = std::reverse_iterator<Iterator>;
        using const_reverse_iterator = const std::reverse_iterator<Iterator>;

        vector() noexcept : _size(0), _capacity(0), _data(nullptr) {}

        explicit vector(size_type size) noexcept : _size(size), _capacity(size), _data(allocator.allocate(size))
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (size_type i = 0; i < _size; ++i) allocator.construct(_data + i);
        }

        vector(size_type size, const_reference value) noexcept
            : _size(size), _capacity(size), _data(allocator.allocate(size))
        {
            for (size_type i = 0; i < _size; ++i)
                if constexpr (std::is_trivially_copyable_v<T>)
                    _data[i] = value;
                else
                    allocator.construct(_data + i, value);
        }

        template <typename InputIt, typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
        vector(InputIt first, InputIt last) : _size(0), _capacity(0), _data(nullptr)
        {
            _size = std::distance(first, last);
            _data = allocator.allocate(_size);
            if (_size > 0)
            {
                _capacity = _size;
                size_type i = 0;
                for (InputIt it = first; it != last; ++it, ++i)
                    if constexpr (std::is_trivially_constructible_v<T>)
                        _data[i] = *it;
                    else
                        allocator.construct(_data + i, *it);
            }
        }

        ~vector() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (size_type i = 0; i < _size; ++i) allocator.destroy(_data + i);
            allocator.deallocate(_data, _capacity);
            _data = nullptr;
        }

        vector(const vector &other) noexcept
            : _size(other._size), _capacity(other._size), _data(allocator.allocate(_capacity))
        {
            if (_size > 0)
            {
                if constexpr (std::is_trivially_copyable_v<T>)
                    std::memcpy(_data, other._data, _size * sizeof(T));
                else
                    for (size_type i = 0; i < _size; ++i) allocator.construct(_data + i, other._data[i]);
            }
        }

        vector(vector &&other) noexcept : _size(other._size), _capacity(other._capacity), _data(other._data)
        {
            other._data = nullptr;
            other._size = 0;
            other._capacity = 0;
        }

        vector(std::initializer_list<T> ilist) : _size(ilist.size()), _capacity(ilist.size())
        {
            _data = allocator.allocate(_size);
            std::uninitialized_copy(ilist.begin(), ilist.end(), _data);
        }

        vector &operator=(std::initializer_list<T> ilist) noexcept
        {
            if (_capacity < ilist.size())
            {
                pointer newData = allocator.allocate(ilist.size());
                std::uninitialized_copy(ilist.begin(), ilist.end(), newData);
                clear();
                _data = newData;
                _capacity = ilist.size();
            }
            else
                std::copy(ilist.begin(), ilist.end(), _data);
            _size = ilist.size();
            return *this;
        }

        vector &operator=(const vector &other) noexcept
        {
            if (this != &other)
            {
                const size_type oldCapacity = _capacity;
                _capacity = other._capacity;
                if (oldCapacity < other._capacity) reallocate(false);
                _size = other._size;
                if constexpr (std::is_trivially_copyable_v<T>)
                    std::memcpy(_data, other._data, _size * sizeof(T));
                else
                    for (size_type i = 0; i < _size; ++i) allocator.construct(_data + i, other._data[i]);
            }
            return *this;
        }

        vector &operator=(vector &&other) noexcept
        {
            if (this != &other)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                    for (size_type i = 0; i < _size; ++i) allocator.destroy(_data + i);
                allocator.deallocate(_data, _capacity);
                _data = other._data;
                _size = other._size;
                _capacity = other._capacity;
                other._data = nullptr;
                other._size = 0;
                other._capacity = 0;
            }
            return *this;
        }

        reference operator[](size_type index) { return _data[index]; }

        const_reference operator[](size_type index) const { return _data[index]; }

        bool operator==(const vector &other) const
        {
            if (_size != other._size) return false;
            for (size_type i = 0; i < _size; ++i)
                if (_data[i] != other._data[i]) return false;
            return true;
        }

        reference at(size_type index)
        {
            if (index >= _size) throw std::out_of_range("Index out of range");
            return _data[index];
        }

        const_reference at(size_type index) const
        {
            if (index >= _size) throw std::out_of_range("Index out of range");
            return _data[index];
        }

        reference front() { return *_data; }

        const_reference front() const { return *_data; }

        reference back() { return _data[_size - 1]; }

        const_reference back() const { return _data[_size - 1]; }

        iterator begin() noexcept { return iterator(_data); }

        const_iterator begin() const noexcept { return iterator(_data); }

        const_iterator cbegin() const noexcept { return iterator(_data); }

        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

        iterator end() noexcept { return iterator(_data + _size); }

        const_iterator end() const noexcept { return iterator(_data + _size); }

        const_iterator cend() const noexcept { return iterator(_data + _size); }

        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

        bool empty() const noexcept { return _size == 0; }

        size_type size() const noexcept { return _size; }

        size_type max_size() const noexcept { return allocator.max_size(); }

        size_type capacity() const { return _capacity; }

        const_pointer data() const { return _data; }
        pointer data() { return _data; }

        void reserve(size_type newCapacity)
        {
            if (newCapacity <= _capacity) return;
            _capacity = newCapacity;
            pointer newData = allocator.reallocate(_data, _capacity);
            if (!newData) throw std::bad_alloc();
            _data = newData;
        }

        void resize(size_type newSize)
        {
            if (newSize > _capacity)
            {
                _capacity = std::max(_capacity * 2, newSize);
                pointer newData = allocator.allocate(_capacity);
                if (!newData) throw std::bad_alloc();
                if constexpr (std::is_trivially_copyable_v<T>)
                    std::memcpy(newData, _data, _size * sizeof(T));
                else
                {
                    for (size_type i = 0; i < _size; ++i)
                    {
                        allocator.construct(newData + i, std::move(_data[i]));
                        allocator.destroy(_data + i);
                    }
                }
                allocator.deallocate(_data, _capacity);
                _data = newData;
            }

            if constexpr (!std::is_trivially_constructible_v<T>)
            {
                if (newSize > _size)
                    for (size_type i = _size; i < newSize; ++i) allocator.construct(_data + i);
                else
                    for (size_type i = newSize; i < _size; ++i) allocator.destroy(_data + i);
            }
            _size = newSize;
        }

        void clear() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (size_type i = 0; i < _size; ++i) allocator.destroy(_data + i);
            _size = 0;
        }

        void push_back(const_reference value) noexcept
        {
            if (_size == _capacity) reallocate();
            if constexpr (std::is_trivially_copyable_v<T>)
                _data[_size] = value;
            else
                allocator.construct(_data + _size, value);
            ++_size;
        }

        template <typename U>
        void push_back(U &&value)
        {
            if (_size == _capacity) reallocate();
            if constexpr (std::is_trivially_move_assignable_v<T>)
                _data[_size] = std::forward<U>(value);
            else
                allocator.construct(_data + _size, std::forward<U>(value));
            ++_size;
        }

        template <typename... Args>
        void emplace_back(Args &&...args)
        {
            if (_size == _capacity) reallocate();
            if constexpr (std::is_trivially_constructible_v<T>)
                _data[_size] = T(std::forward<Args>(args)...);
            else
                allocator.construct(_data + _size, T(std::forward<Args>(args)...));
            ++_size;
        }

        void pop_back() noexcept
        {
            if (_size == 0) return;
            if constexpr (!std::is_trivially_destructible_v<T>) allocator.destroy(_data + _size - 1);
            --_size;
        }

        iterator erase(iterator pos);

        iterator erase(iterator first, iterator last);

        iterator insert(iterator pos, const T &value);

        template <typename InputIt>
        void insert(iterator pos, InputIt first, InputIt last);

        template <typename InputIt>
        void assign(InputIt first, InputIt last)
        {
            size_type newSize = std::distance(first, last);
            if (newSize > _capacity)
            {
                pointer newData = allocator.allocate(newSize);
                std::uninitialized_copy(first, last, newData);
                clear();
                allocator.deallocate(_data, _capacity);
                _data = newData;
                _capacity = newSize;
            }
            else
            {
                if constexpr (!std::is_trivially_destructible<T>::value)
                    for (size_t i = newSize; i < _size; ++i) allocator.destroy(_data + i);
                std::copy(first, last, _data);
            }

            _size = newSize;
        }

        void assign(std::initializer_list<T> ilist) { *this = ilist; }

        void assign(size_type count, const_reference value)
        {
            if (count > _capacity)
            {
                pointer newData = allocator.allocate(count);
                std::uninitialized_fill_n(newData, count, value);
                clear();
                allocator.deallocate(_data, _capacity);
                _data = newData;
                _capacity = count;
            }
            else
                std::fill_n(_data, count, value);
            _size = count;
        }

    private:
        size_type _size;
        size_type _capacity;
        pointer _data;

        void reallocate(bool adjustCapacity = true)
        {
            if (adjustCapacity) _capacity = std::max(_capacity * 2, std::max((size_t)8UL, _capacity + 1));
            pointer newData;
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                newData = allocator.reallocate(_data, _capacity);
                if (!newData) throw std::bad_alloc();
            }
            else
            {
                newData = allocator.allocate(_capacity);
                if (!newData) throw std::bad_alloc();
                for (size_type i = 0; i < _size; ++i)
                {
                    allocator.construct(newData + i, std::move(_data[i]));
                    allocator.destroy(_data + i);
                }
                allocator.deallocate(_data, _capacity);
            }
            _data = newData;
        }

        template <typename Iter, typename Dest>
        void move_construct(Iter start, Iter end, Dest dest)
        {
            for (; start != end; ++start, ++dest) new (dest) T(std::move(*start));
        }

        template <typename Iter, typename Dest>
        void copy_construct(Iter start, Iter end, Dest dest)
        {
            for (; start != end; ++start, ++dest) new (dest) T(*start);
        }

        template <typename Iter>
        void move_elements_backward(Iter start, Iter end, Iter destEnd)
        {
            while (end != start) new (--destEnd) T(std::move(*--end));
        }
    };

    template <typename T, template <typename> class allocator_base_t>
    typename vector<T, allocator_base_t>::allocator_t vector<T, allocator_base_t>::allocator{};

    template <typename T, template <typename> class allocator_base_t>
    class vector<T, allocator_base_t>::Iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = vector::pointer;
        using reference = T &;
        using const_reference = const T &;

        Iterator(pointer ptr = nullptr) : _ptr(ptr) {}

        reference operator*() { return *_ptr; }
        reference operator*() const { return *_ptr; }
        pointer operator->() { return _ptr; }
        pointer operator->() const { return _ptr; }

        Iterator &operator++()
        {
            ++_ptr;
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
            --_ptr;
            return *this;
        }
        Iterator operator--(int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        friend bool operator==(const Iterator &a, const Iterator &b) { return a._ptr == b._ptr; }
        friend bool operator!=(const Iterator &a, const Iterator &b) { return a._ptr != b._ptr; }

        Iterator &operator+=(difference_type movement)
        {
            _ptr += movement;
            return *this;
        }
        Iterator &operator-=(difference_type movement)
        {
            _ptr -= movement;
            return *this;
        }
        Iterator operator+(difference_type movement) const { return Iterator(_ptr + movement); }
        Iterator operator-(difference_type movement) const { return Iterator(_ptr - movement); }

        difference_type operator-(const Iterator &other) const { return _ptr - other._ptr; }

        friend bool operator<(const Iterator &a, const Iterator &b) { return a._ptr < b._ptr; }
        friend bool operator>(const Iterator &a, const Iterator &b) { return a._ptr > b._ptr; }
        friend bool operator<=(const Iterator &a, const Iterator &b) { return a._ptr <= b._ptr; }
        friend bool operator>=(const Iterator &a, const Iterator &b) { return a._ptr >= b._ptr; }

        reference operator[](difference_type offset) const { return *(*this + offset); }

    private:
        pointer _ptr;
    };

    template <typename T, template <typename> class Allocator>
    vector<T, Allocator>::iterator vector<T, Allocator>::erase(iterator pos)
    {
        if (pos >= begin() && pos < end())
        {
            std::ptrdiff_t index = pos - begin();
            std::move(_data + index + 1, _data + _size, _data + index);
            --_size;
            if constexpr (!std::is_trivially_destructible_v<T>) allocator.destroy(_data + _size);
            return iterator(_data + index);
        }
        return end();
    }

    template <typename T, template <typename> class Allocator>
    vector<T, Allocator>::iterator vector<T, Allocator>::erase(iterator first, iterator last)
    {
        if (first >= begin() && last <= end() && first < last)
        {
            std::ptrdiff_t index = first - begin();
            std::ptrdiff_t count = last - first;
            std::move(_data + index + count, _data + _size, _data + index);
            _size -= count;
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (std::ptrdiff_t i = 0; i < count; ++i) allocator.destroy(_data + _size + i);
            return iterator(_data + index);
        }
        return end();
    }

    template <typename T, template <typename> class Allocator>
    vector<T, Allocator>::iterator vector<T, Allocator>::insert(Iterator pos, const T &value)
    {
        if (pos < begin() || pos > end()) return end();
        std::ptrdiff_t index = pos - begin();
        if (_size == _capacity) reserve(_capacity == 0 ? 1 : _capacity * 2);
        std::move_backward(_data + index, _data + _size, _data + _size + 1);
        if constexpr (std::is_trivially_copyable_v<T>)
            _data[index] = value;
        else
            allocator.construct(_data + index, value);
        ++_size;
        return iterator(_data + index);
    }

    template <typename T, template <typename> class Allocator>
    template <typename InputIt>
    void vector<T, Allocator>::insert(iterator pos, InputIt first, InputIt last)
    {
        size_type posIndex = pos - _data;
        size_type insertCount = std::distance(first, last);
        if (_size + insertCount > _capacity)
        {
            size_type newCapacity = std::max(_capacity * 2, _size + insertCount);
            pointer newData = allocator.allocate(newCapacity);
            if (!newData) throw std::bad_alloc();

            move_construct(_data, _data + posIndex, newData);
            copy_construct(first, last, newData + posIndex);
            move_construct(_data + posIndex, _data + _size, newData + posIndex + insertCount);

            if constexpr (!std::is_trivially_destructible_v<T>)
                for (size_type i = 0; i < _size; ++i) allocator.destroy(_data + i);
            allocator.deallocate(_data, _capacity);
            _data = newData;
            _size += insertCount;
            _capacity = newCapacity;
        }
        else
        {
            move_elements_backward(_data + posIndex, _data + _size, _data + posIndex + insertCount);
            copy_construct(first, last, _data + posIndex);
            _size += insertCount;
        }
    }
} // namespace astl

#endif