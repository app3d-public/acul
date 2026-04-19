#pragma once

#include <cstring>
#include <type_traits>
#include "memory/alloc.hpp"

#define ACUL_ANY_STACK 0x1
#define ACUL_ANY_HEAP  0x2

namespace acul
{
    class any
    {
    public:
        any() noexcept : heap(nullptr), flags(0), size(0) {}

        template <typename T>
        any(const T &t) noexcept : heap(nullptr), flags(0), size(0)
        {
            set(t);
        }

        any(const any &other) noexcept : heap(nullptr), flags(0), size(0) { copy_from(other); }

        any(any &&other) noexcept : heap(nullptr), flags(0), size(0) { move_from(other); }

        any &operator=(const any &other) noexcept
        {
            if (this == &other) return *this;
            reset();
            copy_from(other);
            return *this;
        }

        any &operator=(any &&other) noexcept
        {
            if (this == &other) return *this;
            reset();
            move_from(other);
            return *this;
        }

        template <typename T>
        any &operator=(const T &t) noexcept
        {
            set(t);
            return *this;
        }

        ~any() noexcept { reset(); }

        bool has_value() const noexcept { return flags != 0; }

        template <typename T>
        void set(const T &t) noexcept
        {
            static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>,
                          "Only POD types are allowed");
            const size_t new_size = sizeof(T);

            if (new_size <= sizeof(storage))
            {
                if (flags == ACUL_ANY_HEAP && heap)
                {
                    release(heap, size);
                    heap = nullptr;
                }
                flags = ACUL_ANY_STACK;
                size = new_size;
                std::memcpy(storage, &t, new_size);
            }
            else
            {
                if (flags == ACUL_ANY_HEAP && heap)
                {
                    if (size < new_size)
                    {
                        release(heap, size);
                        heap = alloc_n<unsigned char>(new_size);
                    }
                }
                else
                {
                    heap = alloc_n<unsigned char>(new_size);
                }
                flags = ACUL_ANY_HEAP;
                size = new_size;
                std::memcpy(heap, &t, new_size);
            }
        }

        template <typename T>
        T get() const noexcept
        {
            static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>,
                          "Only POD types are allowed");
            if (!has_value() || size != sizeof(T)) return T{};
            T result{};
            if (flags == ACUL_ANY_STACK) std::memcpy(&result, storage, sizeof(T));
            else std::memcpy(&result, heap, sizeof(T));
            return result;
        }

    private:
        void reset() noexcept
        {
            if (flags == ACUL_ANY_HEAP && heap)
            {
                release(heap, size);
                heap = nullptr;
            }
            flags = 0;
            size = 0;
        }

        void copy_from(const any &other) noexcept
        {
            flags = other.flags;
            size = other.size;

            if (other.flags == ACUL_ANY_STACK)
            {
                std::memcpy(storage, other.storage, sizeof(storage));
                heap = nullptr;
            }
            else if (other.flags == ACUL_ANY_HEAP)
            {
                heap = alloc_n<unsigned char>(size);
                std::memcpy(heap, other.heap, size);
            }
            else
            {
                heap = nullptr;
            }
        }

        void move_from(any &other) noexcept
        {
            flags = other.flags;
            size = other.size;

            if (other.flags == ACUL_ANY_STACK)
            {
                std::memcpy(storage, other.storage, sizeof(storage));
                heap = nullptr;
            }
            else
            {
                heap = other.heap;
                other.heap = nullptr;
            }

            other.flags = 0;
            other.size = 0;
        }

        union
        {
            alignas(16) unsigned char storage[sizeof(void *) * 2];
            unsigned char *heap;
        };
        unsigned char flags;
        size_t size;
    };
} // namespace acul
