#ifndef APP_CORE_STD_ARRAY_H
#define APP_CORE_STD_ARRAY_H

#include <__memory/allocator.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

template <typename T>
class Array
{
public:
    using value_type = T;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;

    class Iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        Iterator(pointer ptr) : m_ptr(ptr) {}

        // Операции разыменования и доступа к членам
        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }

        // Инкремент и декремент
        Iterator &operator++()
        {
            m_ptr++;
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
            m_ptr--;
            return *this;
        }
        Iterator operator--(int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        // Сравнение
        friend bool operator==(const Iterator &a, const Iterator &b) { return a.m_ptr == b.m_ptr; }
        friend bool operator!=(const Iterator &a, const Iterator &b) { return a.m_ptr != b.m_ptr; }

        // Операции для случайного доступа
        Iterator &operator+=(difference_type movement)
        {
            m_ptr += movement;
            return *this;
        }
        Iterator &operator-=(difference_type movement)
        {
            m_ptr -= movement;
            return *this;
        }
        Iterator operator+(difference_type movement) const { return Iterator(m_ptr + movement); }
        Iterator operator-(difference_type movement) const { return Iterator(m_ptr - movement); }

        difference_type operator-(const Iterator &other) const { return m_ptr - other.m_ptr; }

        // Операции сравнения для случайного доступа
        friend bool operator<(const Iterator &a, const Iterator &b) { return a.m_ptr < b.m_ptr; }
        friend bool operator>(const Iterator &a, const Iterator &b) { return a.m_ptr > b.m_ptr; }
        friend bool operator<=(const Iterator &a, const Iterator &b) { return a.m_ptr <= b.m_ptr; }
        friend bool operator>=(const Iterator &a, const Iterator &b) { return a.m_ptr >= b.m_ptr; }

        // Операция индексирования
        reference operator[](difference_type offset) const { return *(*this + offset); }

    private:
        pointer m_ptr;
    };

    using iterator = Iterator;
    using const_iterator = const Iterator;

    Array() : _size(0), _capacity(0), _data(nullptr) {}

    Array(size_t size) : _size(size), _capacity(size)
    {
        _data = (T *)malloc(_size * sizeof(T));
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (size_t i = 0; i < _size; ++i)
                new (_data + i) T();
    }

    Array(size_t size, const T &value) : _size(size), _capacity(size)
    {
        _data = (T *)malloc(_capacity * sizeof(T));
        for (size_t i = 0; i < _size; ++i)
            if constexpr (std::is_trivially_copyable_v<T>)
                _data[i] = value;
            else
                new (_data + i) T(value);
    }

    template <typename InputIt, typename = std::enable_if_t<!std::is_integral<InputIt>::value>>
    Array(InputIt first, InputIt last) : _size(0), _capacity(0), _data(nullptr)
    {
        _size = std::distance(first, last);
        if (_size > 0)
        {
            _data = (T *)malloc(_size * sizeof(T));
            _capacity = _size;
            size_t i = 0;
            for (InputIt it = first; it != last; ++it, ++i)
                new (_data + i) T(*it);
        }
    }

    ~Array()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (size_t i = 0; i < _size; ++i)
                _data[i].~T();
        free(_data);
        _data = nullptr;
    }

    Array(const Array &other) : _size(other._size), _capacity(other._size)
    {
        _data = (T *)malloc(_capacity * sizeof(T));
        if constexpr (std::is_trivially_copyable_v<T>)
            std::memcpy(_data, other._data, _size * sizeof(T));
        else
            for (size_t i = 0; i < _size; ++i)
                new (&_data[i]) T(other._data[i]);
    }

    Array(Array &&other) noexcept : _data(other._data), _size(other._size), _capacity(other._capacity)
    {
        other._data = nullptr;
        other._size = 0;
        other._capacity = 0;
    }

    Array(std::initializer_list<T> ilist) : _size(ilist.size()), _capacity(ilist.size())
    {
        _data = (T *)malloc(_size * sizeof(T));
        std::uninitialized_copy(ilist.begin(), ilist.end(), _data);
    }

    Array &operator=(std::initializer_list<T> ilist)
    {
        if (_capacity < ilist.size())
        {
            T *newData = (T *)malloc(ilist.size() * sizeof(T));
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

    Array &operator=(const Array &other)
    {
        if (this != &other)
        {
            if constexpr (!std::is_trivially_copyable_v<T>)
                for (size_t i = 0; i < _size; ++i)
                    _data[i].~T();
            free(_data);
            _size = other._size;
            _capacity = other._capacity;
            _data = (T *)malloc(other._size * sizeof(T));
            if constexpr (std::is_trivially_copyable_v<T>)
                std::memcpy(_data, other._data, _size * sizeof(T));
            else
                for (size_t i = 0; i < _size; ++i)
                    new (&_data[i]) T(other._data[i]); // Используем placement new для копирования
        }
        return *this;
    }

    Array &operator=(Array &&other) noexcept
    {
        if (this != &other)
        {
            free(_data);
            _data = other._data;
            _size = other._size;
            _capacity = other._capacity;
            other._data = nullptr;
            other._size = 0;
            other._capacity = 0;
        }
        return *this;
    }

    T &operator[](size_t index) { return _data[index]; }

    const T &operator[](size_t index) const { return _data[index]; }

    bool operator==(const Array &other) const
    {
        if (_size != other._size)
            return false;
        for (size_t i = 0; i < _size; ++i)
            if (_data[i] != other._data[i])
                return false;
        return true;
    }

    T &at(size_t index)
    {
        if (index >= _size)
            throw std::out_of_range("Index out of range");
        assert(_data + index != nullptr);
        return _data[index];
    }

    const T &at(size_t index) const
    {
        if (index >= _size)
            throw std::out_of_range("Index out of range");
        return _data[index];
    }

    T &front() { return *_data; }

    const T &front() const { return *_data; }

    T &back() { return _data[_size - 1]; }

    const T &back() const { return _data[_size - 1]; }

    iterator begin() { return Iterator(_data); }

    const_iterator begin() const { return Iterator(_data); }

    iterator end() { return Iterator(_data + _size); }

    const_iterator end() const { return Iterator(_data + _size); }

    bool empty() const { return _size == 0; }

    size_t size() const { return _size; }

    size_t capacity() const { return _capacity; }

    const T *data() const { return _data; }
    T *data() { return _data; }

    void reserve(size_t newCapacity)
    {
        if (newCapacity <= _capacity)
            return;
        size_t oldCapacity = _capacity;
        _capacity = newCapacity;
        T *newData;
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            newData = (T *)realloc(_data, _capacity * sizeof(T));
            if (!newData)
                throw std::bad_alloc();
        }
        else
        {
            newData = (T *)malloc(_capacity * sizeof(T));
            if (!newData)
                throw std::bad_alloc();
            for (size_t i = 0; i < _size; ++i)
            {
                new (&newData[i]) T(std::move(_data[i]));
                _data[i].~T();
            }
            free(_data);
        }
        _data = newData;
    }

    void resize(size_t newSize)
    {
        if (newSize > _capacity)
        {
            _capacity = std::max(_capacity * 2, newSize);
            T *newData = (T *)malloc(_capacity * sizeof(T));
            if (!newData)
                throw std::bad_alloc();
            if constexpr (std::is_trivially_copyable_v<T>)
                std::memcpy(newData, _data, _size * sizeof(T));
            else
            {
                for (size_t i = 0; i < _size; ++i)
                {
                    new (&newData[i]) T(std::move(_data[i]));
                    _data[i].~T();
                }
            }
            free(_data);
            _data = newData;
        }

        if constexpr (!std::is_trivially_constructible_v<T>)
        {
            if (newSize > _size)
                for (size_t i = _size; i < newSize; ++i)
                    new (&_data[i]) T();
            else
                for (size_t i = newSize; i < _size; ++i)
                    _data[i].~T();
        }
        _size = newSize;
    }

    void clear()
    {
        if constexpr (!std::is_trivially_destructible<T>::value)
            for (size_t i = 0; i < _size; ++i)
                _data[i].~T();
        _size = 0;
    }

    void push_back(const T &value)
    {
        if (_size == _capacity)
            reallocate();
        new (&_data[_size]) T(value);
        ++_size;
    }

    template <typename U>
    void push_back(U &&value)
    {
        if (_size == _capacity)
            reallocate();
        new (&_data[_size]) T(std::forward<U>(value));
        ++_size;
    }

    template <typename... Args>
    void emplace_back(Args &&...args)
    {
        if (_size == _capacity)
            reallocate();
        new (&_data[_size]) T(std::forward<Args>(args)...);
        ++_size;
    }

    void pop_back()
    {
        if (_size == 0)
            return;
        _data[_size - 1].~T();
        --_size;
    }

    iterator erase(iterator pos)
    {
        if (pos >= begin() && pos < end())
        {
            std::ptrdiff_t index = pos - begin();
            _data[index].~T();
            std::move(_data + index + 1, _data + _size, _data + index);

            --_size;
            return iterator(_data + index);
        }
        return end();
    }

    iterator insert(iterator pos, const T &value)
    {
        if (pos < begin() || pos > end())
            return end();
        std::ptrdiff_t index = pos - begin();
        if (_size == _capacity)
            reserve(_capacity == 0 ? 1 : _capacity * 2);
        std::move_backward(_data + index, _data + _size, _data + _size + 1);
        new (_data + index) T(value);
        ++_size;
        return iterator(_data + index);
    }

    size_t indexof(iterator pos) const { return pos - _data; }

    template <typename InputIt>
    void insert(iterator pos, InputIt first, InputIt last)
    {
        size_t posIndex = indexof(pos);
        size_t insertCount = std::distance(first, last);
        if (_size + insertCount > _capacity)
        {
            size_t newCapacity = std::max(_capacity * 2, _size + insertCount);
            T *newData = (T *)malloc(newCapacity * sizeof(T));
            if (!newData)
                throw std::bad_alloc();

            move_construct(_data, _data + posIndex, newData);
            copy_construct(first, last, newData + posIndex);
            move_construct(_data + posIndex, _data + _size, newData + posIndex + insertCount);

            if constexpr (!std::is_trivially_destructible<T>::value)
                for (size_t i = 0; i < _size; ++i)
                    _data[i].~T();

            free(_data);
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

    template <typename InputIt>
    void assign(InputIt first, InputIt last)
    {
        size_t newSize = std::distance(first, last);
        if (newSize > _capacity)
        {
            T *newData = (T *)malloc(newSize * sizeof(T));
            std::uninitialized_copy(first, last, newData);
            clear();
            free(_data);
            _data = newData;
            _capacity = newSize;
        }
        else
        {
            if constexpr (!std::is_trivially_destructible<T>::value)
                for (size_t i = newSize; i < _size; ++i)
                    _data[i].~T();
            std::copy(first, last, _data);
        }

        _size = newSize;
    }

    void assign(std::initializer_list<T> ilist) { *this = ilist; }

    void assign(size_t count, const T &value)
    {
        if (count > _capacity)
        {
            T *newData = (T *)malloc(count * sizeof(T));
            std::uninitialized_fill_n(newData, count, value);
            clear();
            free(_data);
            _data = newData;
            _capacity = count;
        }
        else
            std::fill_n(_data, count, value);
        _size = count;
    }

private:
    size_t _size;
    size_t _capacity;
    T *_data;

    void reallocate()
    {
        _capacity = std::max(_capacity * 2, std::max((size_t)8UL, _capacity + 1));
        T *newData;
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            newData = (T *)realloc(_data, _capacity * sizeof(T));
            if (!newData)
                throw std::bad_alloc();
        }
        else
        {
            newData = (T *)malloc(_capacity * sizeof(T));
            if (!newData)
                throw std::bad_alloc();
            for (size_t i = 0; i < _size; ++i)
            {
                new (&newData[i]) T(std::move(_data[i]));
                _data[i].~T();
            }
            free(_data);
        }
        _data = newData;
    }

    template <typename Iter, typename Dest>
    void move_construct(Iter start, Iter end, Dest dest)
    {
        for (; start != end; ++start, ++dest)
            new (dest) T(std::move(*start));
    }

    template <typename Iter, typename Dest>
    void copy_construct(Iter start, Iter end, Dest dest)
    {
        for (; start != end; ++start, ++dest)
            new (dest) T(*start);
    }

    template <typename Iter>
    void move_elements_backward(Iter start, Iter end, Iter destEnd)
    {
        while (end != start)
            new (--destEnd) T(std::move(*--end));
    }
};

#endif // APP_CORE_STD_ARRAY