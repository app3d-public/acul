#pragma once

#include <iterator>
#include "buffer.hpp"

namespace acul::gpu
{
    template <typename T>
    class vector
    {
    public:
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;

        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        vector() = default;

        vector(device &dev, const buffer &buf) : _device(&dev), _data(buf), _size(buf.instance_count) { construct(); }

        vector(device &dev, const buffer &buf, const_reference value)
            : _device(&dev), _data(buf), _size(buf.instance_count)
        {
            construct();
            fill(value);
        }

        template <typename InputIt, std::enable_if_t<is_input_iterator<InputIt>::value, int> = 0>
        vector(InputIt first, InputIt last, device &dev, const buffer &buf) : _device(&dev), _data(buf)
        {
            for (; first != last; ++first) push_back(*first);
        }

        template <typename ForwardIt, std::enable_if_t<is_forward_iterator_based<ForwardIt>::value, int> = 0>
        vector(ForwardIt first, ForwardIt last, device &dev, const buffer &buf) : _device(&dev), _data(buf)
        {
            _size = std::distance(first, last);
            _data.instance_count = static_cast<u32>(_size);
            construct();

            pointer tmp = alloc_n<value_type>(_size);
            size_type i = 0;
            for (auto it = first; it != last; ++it, ++i) tmp[i] = *it;
            write_to_buffer(_data, tmp, get_required_mem(_size));
            release(tmp);
        }

        vector(std::initializer_list<value_type> ilist, device &dev, const buffer &buf)
            : _device(&dev), _data(buf), _size(ilist.size())
        {
            _data.instance_count = static_cast<u32>(_size);
            construct();

            pointer tmp = alloc_n<value_type>(_size);
            size_type i = 0;
            for (const auto &v : ilist) tmp[i++] = v;
            write_to_buffer(_data, tmp, get_required_mem(_size));
            release(tmp);
        }

        vector(const vector &) = delete;
        vector &operator=(const vector &) = delete;

        vector(vector &&other) : _device(other._device), _data(other._data), _size(other._size)
        {
            if (this != &other)
            {
                other._device = nullptr;
                other._size = 0;
                other._data = {};
            }
        }

        ~vector() { destroy(); }

        bool reserve(size_type new_capacity)
        {
            if (new_capacity <= capacity()) return true;
            _data.instance_count = new_capacity;
            return reallocate(false);
        }

        bool resize(size_type new_size)
        {
            if (new_size > capacity())
            {
                _data.instance_count = new_size;
                if (!reallocate(false)) return false;
            }
            _size = new_size;
            return true;
        }

        bool push_back(const_reference value)
        {
            if (_size >= capacity() && !reallocate(_size + 1)) return false;
            write_to_buffer(_data, (void *)&value, sizeof(value_type), _size * _data.alignment_size);
            ++_size;
            return true;
        }

        void pop_back()
        {
            assert(_size > 0);
            --_size;
        }

        template <typename... Args>
        bool emplace_back(Args &&...args)
        {
            value_type val(std::forward<Args>(args)...);
            return push_back(val);
        }

        void clear() { _size = 0; }

        void destroy()
        {
            if (_data.vk_buffer) destroy_buffer(_data, *_device);
            _size = 0;
        }

        bool empty() const { return _size == 0; }

        reference operator[](size_type index)
        {
            assert(index < _size);
            return *(pointer)((char *)_data.mapped + index * _data.alignment_size);
        }

        const_reference operator[](size_type index) const
        {
            assert(index < _size);
            return *(const_pointer)((const char *)_data.mapped + index * _data.alignment_size);
        }

        reference at(size_type index)
        {
            if (index >= _size) throw out_of_range(_size, index);
            return *(reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) + index * _data.alignment_size));
        }

        const_reference at(size_type index) const
        {
            if (index >= _size) throw out_of_range(_size, index);
            return *(reinterpret_cast<const_pointer>(static_cast<const char *>(_data.mapped) +
                                                     index * _data.alignment_size));
        }

        constexpr size_type max_size() const noexcept { return sizeof(value_type); }

        bool is_inited() const { return _device != nullptr && _data.vk_buffer; }

        void init(device &dev, const buffer &buf)
        {
            _device = &dev;
            _data = buf;
            _size = 0;
            construct();
        }

        size_type size() const { return _size; }
        size_type capacity() const { return _data.instance_count; }

        buffer &data() { return _data; }
        const buffer &data() const { return _data; }

        reference front() { return (*this)[0]; }
        const_reference front() const { return (*this)[0]; }

        reference back() { return (*this)[_size - 1]; }
        const_reference back() const { return (*this)[_size - 1]; }

        iterator begin()
        {
            if (!_data.mapped) return nullptr;
            return reinterpret_cast<iterator>(_data.mapped);
        }
        const_iterator begin() const
        {
            if (!_data.mapped) return nullptr;
            return reinterpret_cast<const_iterator>(_data.mapped);
        }
        const_iterator cbegin() const { return begin(); }

        reverse_iterator rbegin() { return reverse_iterator(end()); }
        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }

        iterator end()
        {
            if (!_data.mapped) return nullptr;
            return reinterpret_cast<iterator>(static_cast<char *>(_data.mapped) + _size * _data.alignment_size);
        }

        const_iterator end() const
        {
            if (!_data.mapped) return nullptr;
            return reinterpret_cast<const_iterator>(static_cast<const char *>(_data.mapped) +
                                                    _size * _data.alignment_size);
        }
        const_iterator cend() const { return end(); }

        reverse_iterator rend() { return reverse_iterator(begin()); }
        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
        const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

        iterator erase(iterator pos)
        {
            if (!_data.mapped || pos >= end()) return end();
            size_type index = std::distance(begin(), pos);
            if (index >= _size) return end();

            pointer src = begin() + index + 1;
            pointer dst = begin() + index;
            size_type num_elements_to_move = _size - index - 1;

            if (num_elements_to_move > 0) memmove(dst, src, num_elements_to_move * _data.alignment_size);

            --_size;
            return begin() + index;
        }

        iterator erase(iterator first, iterator last)
        {
            if (!_data.mapped || first >= last || first >= end()) return end();

            size_type index_first = std::distance(begin(), first);
            size_type index_last = std::distance(begin(), last);

            if (index_first >= _size || index_last > _size) return end();

            size_type num_elements_to_move = _size - index_last;
            pointer src = begin() + index_last;
            pointer dst = begin() + index_first;

            if (num_elements_to_move > 0) memmove(dst, src, num_elements_to_move * _data.alignment_size);

            _size -= (index_last - index_first);
            return begin() + index_first;
        }

        iterator insert(iterator pos, const_reference value)
        {
            size_type index = std::distance(begin(), pos);
            if (index > _size) return end();

            if (_size + 1 > capacity() && !reserve(_size + 1)) return end();

            if (index < _size)
            {
                pointer dst =
                    reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) + (index + 1) * _data.alignment_size);
                pointer src =
                    reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) + index * _data.alignment_size);
                size_type move_count = _size - index;
                memmove(dst, src, move_count * _data.alignment_size);
            }

            *(reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) + index * _data.alignment_size)) = value;
            ++_size;
            return begin() + index;
        }

        template <typename InputIt>
        void insert(iterator pos, InputIt first, InputIt last)
        {
            size_type index = std::distance(begin(), pos);
            size_type count = std::distance(first, last);
            if (count == 0) return;

            if (_size + count > capacity() && !reserve(_size + count)) return;

            if (index < _size)
            {
                pointer dst = reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) +
                                                        (index + count) * _data.alignment_size);
                pointer src =
                    reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) + index * _data.alignment_size);
                size_type move_count = _size - index;
                memmove(dst, src, move_count * _data.alignment_size);
            }

            for (; first != last; ++first)
            {
                *(reinterpret_cast<pointer>(static_cast<char *>(_data.mapped) + index * _data.alignment_size)) = *first;
                ++index;
            }

            _size += count;
        }

        template <typename InputIt>
        void assign(InputIt first, InputIt last)
        {
            size_type new_size = std::distance(first, last);
            if (new_size > capacity() && !reserve(new_size)) return;

            pointer tmp = alloc_n<value_type>(new_size);
            size_type i = 0;
            for (InputIt it = first; it != last; ++it, ++i) tmp[i] = *it;

            write_to_buffer(_data, tmp, get_required_mem(new_size));
            release(tmp);
            _size = new_size;
        }

        void assign(std::initializer_list<value_type> ilist) { assign(ilist.begin(), ilist.end()); }

        void assign(size_type count, const_reference value)
        {
            if (count > capacity() && !reserve(count)) return;

            pointer tmp = alloc_n<value_type>(count);
            for (size_type i = 0; i < count; ++i) tmp[i] = value;

            write_to_buffer(_data, tmp, get_required_mem(count));
            release(tmp);
            _size = count;
        }

    private:
        device *_device = nullptr;
        buffer _data;
        size_type _size = 0;

        void construct()
        {
            construct_buffer(_data, sizeof(value_type));
            if (_data.instance_count > 0 && !allocate()) throw bad_alloc(_data.buffer_size);
        }

        bool allocate()
        {
            if (!allocate_buffer(_data, *_device)) return false;
            if (!map_buffer(_data, *_device))
            {
                destroy_buffer(_data, *_device);
                return false;
            }
            return true;
        }

        void fill(const_reference val)
        {
            pointer tmp = alloc_n<value_type>(_size);
            for (size_type i = 0; i < _size; ++i) tmp[i] = val;
            write_to_buffer(_data, tmp, get_required_mem(_size));
            release(tmp);
        }

        size_type get_required_mem(size_type n) const { return n * _data.alignment_size; }

        bool reallocate(bool adjustCapacity = true)
        {
            assert(_device);
            if (adjustCapacity) _data.instance_count = get_growth_size(_data.instance_count, _data.instance_count + 1);
            buffer new_buffer = _data;
            new_buffer.instance_count = static_cast<u32>(_data.instance_count);
            construct_buffer(new_buffer, sizeof(value_type));

            if (!allocate_buffer(new_buffer, *_device)) return false;
            if (_data.vk_buffer) copy_buffer(*_device, _data.vk_buffer, new_buffer.vk_buffer, get_required_mem(_size));
            if (!map_buffer(new_buffer, *_device))
            {
                destroy_buffer(new_buffer, *_device);
                return false;
            }
            destroy_buffer(_data, *_device);
            _data = new_buffer;
            return true;
        }
    };

} // namespace acul::gpu
