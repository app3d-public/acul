#ifndef APP_ACUL_STD_VECTOR_H
#define APP_ACUL_STD_VECTOR_H

#include "exception/exception.hpp"
#include "memory/alloc.hpp"
#include "type_traits.hpp"

namespace acul
{
    template <typename T, typename Allocator = mem_allocator<T>>
    class vector
    {
    public:
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = typename Allocator::pointer;
        using const_pointer = typename Allocator::const_pointer;
        using size_type = typename Allocator::size_type;

        class iterator;
        using const_iterator = const iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = const std::reverse_iterator<iterator>;

        vector() noexcept : _size(0), _capacity(0), _data(nullptr) {}

        explicit vector(size_type size) noexcept : _size(size), _capacity(size), _data(Allocator::allocate(size))
        {
            if constexpr (!std::is_trivially_constructible_v<value_type>)
                for (size_type i = 0; i < _size; ++i) Allocator::construct(_data + i);
        }

        vector(size_type size, const_reference value) noexcept
            : _size(size), _capacity(size), _data(Allocator::allocate(size))
        {
            for (size_type i = 0; i < _size; ++i)
                if constexpr (std::is_trivially_copyable_v<T>)
                    _data[i] = value;
                else
                    Allocator::construct(_data + i, value);
        }

        template <typename InputIt, std::enable_if_t<is_input_iterator<InputIt>::value, int> = 0>
        vector(InputIt first, InputIt last) : _size(0), _capacity(0), _data(nullptr)
        {
            for (; first != last; ++first) emplace_back(*first);
        }

        template <typename ForwardIt, std::enable_if_t<is_forward_iterator_based<ForwardIt>::value, int> = 0>
        vector(ForwardIt first, ForwardIt last)
            : _size(std::distance(first, last)), _capacity(_size), _data(Allocator::allocate(_size))
        {
            if (_size > 0)
            {
                size_type i = 0;
                for (ForwardIt it = first; it != last; ++it, ++i)
                    if constexpr (std::is_trivially_constructible_v<value_type>)
                        _data[i] = *it;
                    else
                        Allocator::construct(_data + i, *it);
            }
        }

        ~vector() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<value_type>)
                for (size_type i = 0; i < _size; ++i) Allocator::destroy(_data + i);
            Allocator::deallocate(_data, _capacity);
            _data = nullptr;
        }

        vector(const vector &other) noexcept
            : _size(other._size), _capacity(other._size), _data(Allocator::allocate(_capacity))
        {
            if (_size > 0) copy_construct(other._data, other._data + _size, _data);
        }

        vector(vector &&other) noexcept : _size(other._size), _capacity(other._capacity), _data(other._data)
        {
            other._data = nullptr;
            other._size = 0;
            other._capacity = 0;
        }

        vector(std::initializer_list<value_type> ilist) : _size(ilist.size()), _capacity(ilist.size())
        {
            _data = Allocator::allocate(_size);
            std::uninitialized_copy(ilist.begin(), ilist.end(), _data);
        }

        vector &operator=(std::initializer_list<value_type> ilist) noexcept
        {
            if (_capacity < ilist.size())
            {
                pointer new_data = Allocator::allocate(ilist.size());
                std::uninitialized_copy(ilist.begin(), ilist.end(), new_data);
                clear();
                _data = new_data;
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
                if constexpr (!std::is_trivially_destructible_v<value_type>)
                    for (size_type i = 0; i < _size; ++i) Allocator::destroy(_data + i);
                const size_type oldCapacity = _capacity;
                _capacity = other._capacity;
                if (oldCapacity < other._capacity) reallocate(false);
                _size = other._size;
                copy_construct(other._data, other._data + _size, _data);
            }
            return *this;
        }

        vector &operator=(vector &&other) noexcept
        {
            if (this != &other)
            {
                if constexpr (!std::is_trivially_destructible_v<value_type>)
                    for (size_type i = 0; i < _size; ++i) Allocator::destroy(_data + i);
                Allocator::deallocate(_data, _capacity);
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
            if (index >= _size) throw out_of_range(_size, index);
            return _data[index];
        }

        const_reference at(size_type index) const
        {
            if (index >= _size) throw out_of_range(_size, index);
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

        size_type max_size() const noexcept { return Allocator::max_size(); }

        size_type capacity() const { return _capacity; }

        const_pointer data() const { return _data; }
        pointer data() { return _data; }

        void reserve(size_type new_capacity)
        {
            if (new_capacity <= _capacity) return;
            _capacity = new_capacity;
            reallocate(false);
        }

        void resize(size_type new_size)
        {
            if (new_size > _capacity)
            {
                _capacity = std::max(_capacity * 2, new_size);
                pointer new_data = Allocator::allocate(_capacity);
                if (!new_data) throw bad_alloc(_capacity);
                if constexpr (std::is_trivially_copyable_v<value_type>)
                    memcpy(new_data, _data, _size * sizeof(value_type));
                else
                {
                    for (size_type i = 0; i < _size; ++i)
                    {
                        Allocator::construct(new_data + i, std::move(_data[i]));
                        Allocator::destroy(_data + i);
                    }
                }
                Allocator::deallocate(_data, _capacity);
                _data = new_data;
            }

            if constexpr (!std::is_trivially_constructible_v<value_type>)
            {
                if (new_size > _size)
                    for (size_type i = _size; i < new_size; ++i) Allocator::construct(_data + i);
                else
                    for (size_type i = new_size; i < _size; ++i) Allocator::destroy(_data + i);
            }
            _size = new_size;
        }

        void clear() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<value_type>)
                for (size_type i = 0; i < _size; ++i) Allocator::destroy(_data + i);
            _size = 0;
        }

        /**
         * Releases ownership of the internal data pointer without deallocating it.
         *
         * @return A pointer to the data that was managed by the vector.
         */
        pointer release() noexcept
        {
            pointer result = _data;
            _data = nullptr;
            _size = 0;
            _capacity = 0;
            return result;
        }

        void push_back(const_reference value) noexcept
        {
            if (_size == _capacity) reallocate();
            if constexpr (std::is_trivially_copyable_v<value_type>)
                _data[_size] = value;
            else
                Allocator::construct(_data + _size, value);
            ++_size;
        }

        template <typename U>
        void push_back(U &&value)
        {
            if (_size == _capacity) reallocate();
            if constexpr (std::is_trivially_move_assignable_v<value_type>)
                _data[_size] = std::forward<U>(value);
            else
                Allocator::construct(_data + _size, std::forward<U>(value));
            ++_size;
        }

        template <typename... Args>
        void emplace_back(Args &&...args)
        {
            if (_size == _capacity) reallocate();
            if constexpr (!std::is_trivially_constructible_v<value_type> || has_args<Args...>())
                Allocator::construct(_data + _size, std::forward<Args>(args)...);
            ++_size;
        }

        void pop_back() noexcept
        {
            if (_size == 0) return;
            Allocator::destroy(_data + _size - 1);
            --_size;
        }

        iterator erase(iterator pos);

        iterator erase(iterator first, iterator last);

        iterator insert(iterator pos, const_reference value);
        iterator insert(iterator pos, T &&value);

        template <typename InputIt>
        void insert(iterator pos, InputIt first, InputIt last);

        iterator insert(iterator pos, size_type count, const_reference value);

        template <typename InputIt, std::enable_if_t<is_input_iterator_based<InputIt>::value, int> = 0>
        void assign(InputIt first, InputIt last)
        {
            size_type new_size = std::distance(first, last);
            if (new_size > _capacity)
            {
                pointer new_data = Allocator::allocate(new_size);
                std::uninitialized_copy(first, last, new_data);
                clear();
                Allocator::deallocate(_data, _capacity);
                _data = new_data;
                _capacity = new_size;
            }
            else
            {
                if constexpr (!std::is_trivially_destructible<T>::value)
                    for (size_t i = new_size; i < _size; ++i) Allocator::destroy(_data + i);
                std::copy(first, last, _data);
            }

            _size = new_size;
        }

        void assign(std::initializer_list<T> ilist) { *this = ilist; }

        void assign(size_type count, const_reference value)
        {
            if (count > _capacity)
            {
                pointer new_data = Allocator::allocate(count);
                std::uninitialized_fill_n(new_data, count, value);
                clear();
                Allocator::deallocate(_data, _capacity);
                _data = new_data;
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
            if (adjustCapacity) _capacity = get_growth_size(_capacity, _capacity + 1);
            pointer new_data;
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                new_data = Allocator::reallocate(_data, _capacity);
                if (!new_data) throw bad_alloc(_capacity);
            }
            else
            {
                new_data = Allocator::allocate(_capacity);
                if (!new_data) throw bad_alloc(_capacity);
                for (size_type i = 0; i < _size; ++i)
                {
                    Allocator::construct(new_data + i, std::move(_data[i]));
                    Allocator::destroy(_data + i);
                }
                Allocator::deallocate(_data, _capacity);
            }
            _data = new_data;
        }

        template <typename Iter, typename Dest>
        void copy_construct(Iter start, Iter end, Dest dest)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                if constexpr (!std::is_rvalue_reference_v<decltype(*start)>)
                    memcpy(dest, &(*start), (end - start) * sizeof(value_type));
                else
                    for (; start != end; ++start, ++dest) *dest = *start;
            }
            else
                for (; start != end; ++start, ++dest) Allocator::construct(dest, *start);
        }

        template <typename Dest>
        void copy_construct(Dest dest, size_type count, const_reference value)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
                for (size_type i = 0; i < count; ++i) dest[i] = value;
            else
                for (size_type i = 0; i < count; ++i) Allocator::construct(dest + i, value);
        }

        template <typename Iter, typename Dest>
        void move_construct(Iter start, Iter end, Dest dest)
        {
            if constexpr (std::is_trivially_move_constructible_v<value_type>)
                memmove(dest, &(*start), (end - start) * sizeof(value_type));
            else
                for (; start != end; ++start, ++dest) Allocator::construct(dest, std::move(*start));
        }

        template <typename Iter>
        void move_elements_backward(Iter start, Iter end, Iter destEnd)
        {
            if constexpr (std::is_trivially_move_constructible_v<value_type>)
                memmove(destEnd - (end - start), &(*start), (end - start) * sizeof(value_type));
            else
                while (end != start) Allocator::construct(--destEnd, std::move(*--end));
        }
    };

    template <typename T, typename Allocator>
    class vector<T, Allocator>::iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = vector::pointer;
        using reference = T &;
        using const_reference = const T &;

        iterator(pointer ptr = nullptr) : _ptr(ptr) {}

        reference operator*() { return *_ptr; }
        reference operator*() const { return *_ptr; }
        pointer operator->() { return _ptr; }
        pointer operator->() const { return _ptr; }

        iterator &operator++()
        {
            ++_ptr;
            return *this;
        }
        iterator operator++(int)
        {
            iterator temp = *this;
            ++(*this);
            return temp;
        }
        iterator &operator--()
        {
            --_ptr;
            return *this;
        }
        iterator operator--(int)
        {
            iterator temp = *this;
            --(*this);
            return temp;
        }

        friend bool operator==(const iterator &a, const iterator &b) { return a._ptr == b._ptr; }
        friend bool operator!=(const iterator &a, const iterator &b) { return a._ptr != b._ptr; }

        iterator &operator+=(difference_type movement)
        {
            _ptr += movement;
            return *this;
        }
        iterator &operator-=(difference_type movement)
        {
            _ptr -= movement;
            return *this;
        }
        iterator operator+(difference_type movement) const { return iterator(_ptr + movement); }
        iterator operator-(difference_type movement) const { return iterator(_ptr - movement); }

        difference_type operator-(const iterator &other) const { return _ptr - other._ptr; }

        friend bool operator<(const iterator &a, const iterator &b) { return a._ptr < b._ptr; }
        friend bool operator>(const iterator &a, const iterator &b) { return a._ptr > b._ptr; }
        friend bool operator<=(const iterator &a, const iterator &b) { return a._ptr <= b._ptr; }
        friend bool operator>=(const iterator &a, const iterator &b) { return a._ptr >= b._ptr; }

        reference operator[](difference_type offset) const { return *(*this + offset); }

    private:
        pointer _ptr;
    };

    template <typename T, typename Allocator>
    vector<T, Allocator>::iterator vector<T, Allocator>::erase(iterator pos)
    {
        if (pos >= begin() && pos < end())
        {
            std::ptrdiff_t index = pos - begin();
            std::move(_data + index + 1, _data + _size, _data + index);
            --_size;
            Allocator::destroy(_data + _size);
            return iterator(_data + index);
        }
        return end();
    }

    template <typename T, typename Allocator>
    vector<T, Allocator>::iterator vector<T, Allocator>::erase(iterator first, iterator last)
    {
        if (first >= begin() && last <= end() && first < last)
        {
            std::ptrdiff_t index = first - begin();
            std::ptrdiff_t count = last - first;
            std::move(_data + index + count, _data + _size, _data + index);
            _size -= count;
            if constexpr (!std::is_trivially_destructible_v<value_type>)
                for (std::ptrdiff_t i = 0; i < count; ++i) Allocator::destroy(_data + _size + i);
            return iterator(_data + index);
        }
        return end();
    }

    template <typename T, typename Allocator>
    vector<T, Allocator>::iterator vector<T, Allocator>::insert(iterator pos, const_reference value)
    {
        if (pos < begin() || pos > end()) return end();
        std::ptrdiff_t index = pos - begin();

        if (_size == _capacity) reserve(_capacity == 0 ? 1 : _capacity * 2);
        if constexpr (std::is_trivially_move_constructible_v<value_type>)
            std::move_backward(_data + index, _data + _size, _data + _size + 1);
        else
        {
            for (size_type i = _size; i > static_cast<size_type>(index); --i)
            {
                Allocator::construct(_data + i, std::move(_data[i - 1]));
                Allocator::destroy(_data + i - 1);
            }
        }

        if constexpr (std::is_trivially_copyable_v<T>)
            _data[index] = value;
        else
            Allocator::construct(_data + index, value);
        ++_size;
        return iterator(_data + index);
    }

    template <typename T, typename Allocator>
    typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(iterator pos, T &&value)
    {
        if (pos < begin() || pos > end()) return end();
        const std::ptrdiff_t index = pos - begin();

        if (_size == _capacity) reserve(_capacity == 0 ? 1 : _capacity * 2);

        if constexpr (std::is_trivially_move_constructible_v<value_type>)
            std::move_backward(_data + index, _data + _size, _data + _size + 1);
        else
        {
            for (size_t i = _size; i > static_cast<size_t>(index); --i)
            {
                Allocator::construct(_data + i, std::move(_data[i - 1]));
                Allocator::destroy(_data + i - 1);
            }
        }

        if constexpr (std::is_trivially_move_constructible_v<value_type>)
            _data[index] = std::move(value);
        else
            Allocator::construct(_data + index, std::move(value));
        ++_size;
        return iterator(_data + index);
    }

    template <typename T, typename Allocator>
    template <typename InputIt>
    void vector<T, Allocator>::insert(iterator pos, InputIt first, InputIt last)
    {
        size_type pos_index = pos - _data;
        size_type insert_count = std::distance(first, last);

        if (_size + insert_count > _capacity)
        {
            size_type new_capacity = std::max(_capacity * 2, _size + insert_count);
            pointer new_data = Allocator::allocate(new_capacity);
            if (!new_data) throw bad_alloc(new_capacity);

            move_construct(_data, _data + pos_index, new_data);
            copy_construct(first, last, new_data + pos_index);
            move_construct(_data + pos_index, _data + _size, new_data + pos_index + insert_count);

            Allocator::deallocate(_data, _capacity);

            _data = new_data;
            _capacity = new_capacity;
        }
        else
        {
            move_elements_backward(_data + pos_index, _data + _size, _data + _size + insert_count);
            copy_construct(first, last, _data + pos_index);
        }
        _size += insert_count;
    }

    template <typename T, typename Allocator>
    typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(iterator pos, size_type count,
                                                                         const_reference value)
    {
        if (pos < begin() || pos > end()) return end();
        const size_type index = pos - begin();

        if (_size + count > _capacity)
        {
            size_type new_capacity = std::max(_capacity * 2, _size + count);
            pointer new_data = Allocator::allocate(new_capacity);
            if (!new_data) throw bad_alloc(new_capacity);

            move_construct(_data, _data + index, new_data);
            copy_construct(new_data + index, count, value);
            move_construct(_data + index, _data + _size, new_data + index + count);

            Allocator::deallocate(_data, _capacity);
            _data = new_data;
            _capacity = new_capacity;
        }
        else
        {
            move_elements_backward(_data + index, _data + _size, _data + _size + count);
            copy_construct(_data + index, count, value);
        }

        _size += count;
        return iterator(_data + index);
    }

} // namespace acul

#endif