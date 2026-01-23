#pragma once
#include <cassert>
#include "../scalars.hpp"
#include "alloc.hpp"

namespace acul
{
    // Pointer Array or Stack
    template <typename T>
    struct paos
    {
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using iterator = pointer;
        using const_iterator = const_pointer;

        paos() : _stack{}, _count(1) {}
        ~paos() = default;

        pointer allocate_array(u32 count)
        {
            _array = mem_allocator<value_type>::allocate(count);
            _count = count;
            return _array;
        }

        void deallocate()
        {
            if (_count > 1) mem_allocator<value_type>::deallocate(_array, _count);
        }

        u32 size() const { return _count; }

        reference operator[](u32 index)
        {
            assert(_count > 1 && "paos is not an array");
            return _array[index];
        }
        const_reference operator[](u32 index) const
        {
            assert(_count > 1 && "paos is not an array");
            return _array[index];
        }

        reference at(u32 index) noexcept { return _count <= 1 ? _stack : _array[index]; }
        const_reference at(u32 index) const noexcept { return _count <= 1 ? _stack : _array[index]; }

        // Get value on stack
        reference value()
        {
            assert(_count <= 1 && "paos is an array");
            return _stack;
        }

        // Get value on stack
        const_reference value() const
        {
            assert(_count <= 1 && "paos is an array");
            return _stack;
        }

        void value(const_reference value)
        {
            assert(_count <= 1 && "paos is an array");
            _stack = value;
        }

        // Get data
        pointer data() { return _count <= 1 ? &_stack : _array; }
        const_pointer data() const { return _count <= 1 ? &_stack : _array; }

        iterator begin() { return data(); }
        const_pointer begin() const { return data(); }
        const_pointer cbegin() const { return data(); }

        iterator end() { return data() + _count; }
        const_iterator end() const { return data() + _count; }
        const_iterator cend() const { return data() + _count; }

    private:
        union
        {
            pointer _array;
            value_type _stack;
        };
        u32 _count;
    };
} // namespace acul