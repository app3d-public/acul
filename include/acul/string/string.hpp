#ifndef APP_ACUL_STD_STRING_H
#define APP_ACUL_STD_STRING_H

#include "../exception/exception.hpp"
#include "../fwd/string.hpp"
#include "../hash/utils.hpp"
#include "base.hpp"
#include "string_view.hpp"

#if defined(__GNUC__) && !defined(__clang_analyzer__) && !defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-overread"
#endif

namespace acul
{
    template <typename T, typename Allocator>
    class basic_string
    {
    public:
        using size_type = size_t;
        using self_view = basic_string_view<T>;
        using pointer = typename Allocator::pointer;
        using const_pointer = typename Allocator::const_pointer;
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;

        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        enum : size_type
        {
            npos = SIZE_MAX
        };

        constexpr basic_string() noexcept : _salloc{{ALLOC_STACK, 0}, {}} {}

        template <size_type N>
        constexpr basic_string(const value_type (&str)[N]) noexcept : _lalloc{{ALLOC_RDATA, N - 1}, (pointer)str, N}
        {
        }

        template <typename U,
                  typename = std::enable_if_t<std::is_same_v<U, const_pointer> || std::is_same_v<U, pointer>>>
        basic_string(U ptr) noexcept
        {
            size_type capacity = null_terminated_length(ptr);
            if (capacity >= _sso_size)
            {
                _lalloc.cap = capacity + 1;
                _lalloc.ptr = Allocator::allocate(_lalloc.cap);
                memcpy(_lalloc.ptr, ptr, _lalloc.cap * sizeof(value_type));
                _salloc.size.alloc_flags = ALLOC_HEAP;
                _lalloc.size.data = capacity;
            }
            else
            {
                memcpy(_salloc.data, ptr, (capacity + 1) * sizeof(value_type));
                _salloc.size.alloc_flags = ALLOC_STACK;
                _salloc.size.data = capacity;
            }
        }

        template <typename U,
                  typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, basic_string> &&
                                              !std::is_same_v<std::decay_t<U>, const T *> && is_string_like_v<T, U>>>
        explicit basic_string(const U &t) noexcept : basic_string(t.data(), t.size())
        {
        }

        basic_string(const basic_string &other) noexcept
        {
            switch (other._salloc.size.alloc_flags)
            {
                case ALLOC_STACK:
                    _salloc.size = other._salloc.size;
                    memcpy(_salloc.data, other._salloc.data, (_salloc.size.data + 1) * sizeof(value_type));
                    break;
                case ALLOC_RDATA:
                    _salloc.size.alloc_flags = ALLOC_RDATA;
                    _lalloc.size.data = other._lalloc.size.data;
                    _lalloc.cap = other._lalloc.cap;
                    _lalloc.ptr = other._lalloc.ptr;
                    break;
                case ALLOC_HEAP:
                    _salloc.size.alloc_flags = ALLOC_HEAP;
                    _lalloc.size.data = other._lalloc.size.data;
                    _lalloc.cap = other._lalloc.cap;
                    _lalloc.ptr = Allocator::allocate(other._lalloc.cap);
                    if (_lalloc.ptr) memcpy(_lalloc.ptr, other._lalloc.ptr, _lalloc.cap * sizeof(value_type));
                    break;
                default:
                    break;
            }
        }

        basic_string(const_pointer str, size_type len) noexcept
        {
            if (len >= _sso_size)
            {
                _salloc.size.alloc_flags = ALLOC_HEAP;
                _lalloc.size.data = len;
                _lalloc.cap = len + 1;
                _lalloc.ptr = Allocator::allocate(_lalloc.cap);
                memcpy(_lalloc.ptr, str, len * sizeof(value_type));
                _lalloc.ptr[len] = '\0';
            }
            else
            {
                _salloc.size.alloc_flags = ALLOC_STACK;
                _salloc.size.data = len;
                memcpy(_salloc.data, str, len * sizeof(value_type));
                _salloc.data[len] = '\0';
            }
        }

        basic_string(size_type len, value_type ch) noexcept
        {
            if (len >= _sso_size)
            {
                _salloc.size.alloc_flags = ALLOC_HEAP;
                _lalloc.size.data = len;
                _lalloc.cap = len + 1;
                _lalloc.ptr = Allocator::allocate(_lalloc.cap);

                if constexpr (std::is_integral_v<value_type> && sizeof(value_type) == 1)
                    std::memset(_lalloc.ptr, static_cast<unsigned char>(ch), static_cast<size_t>(len));
                else
                    for (size_t i = 0; i < len; ++i) _lalloc.ptr[i] = ch;
                _lalloc.ptr[len] = 0;
            }
            else
            {
                _salloc.size.alloc_flags = ALLOC_STACK;
                _salloc.size.data = len;
                if constexpr (std::is_integral_v<value_type> && sizeof(value_type) == 1)
                    std::memset(_salloc.data, static_cast<unsigned char>(ch), static_cast<size_t>(len));
                else
                    for (size_t i = 0; i < len; ++i) _salloc.data[i] = ch;
                _salloc.data[len] = 0;
            }
        }

        template <typename InputIt>
        basic_string(InputIt first, InputIt last) noexcept
        {
            size_type new_size = static_cast<size_type>(std::distance(first, last));
            if (new_size < _sso_size)
            {
                memcpy(_salloc.data, &(*first), new_size * sizeof(value_type));
                _salloc.data[new_size] = 0;
                _salloc.size.alloc_flags = ALLOC_STACK;
                _salloc.size.data = new_size;
            }
            else
            {
                _lalloc.ptr = Allocator::allocate(new_size + 1);
                memcpy(_lalloc.ptr, &(*first), new_size * sizeof(value_type));
                _lalloc.ptr[new_size] = 0;
                _salloc.size.alloc_flags = ALLOC_HEAP;
                _lalloc.size.data = new_size;
                _lalloc.cap = new_size + 1;
            }
        }

        basic_string(basic_string &&other) noexcept
        {
            switch (other._salloc.size.alloc_flags)
            {
                case ALLOC_STACK:
                    _salloc.size = other._salloc.size;
                    memcpy(_salloc.data, other._salloc.data, (_salloc.size.data + 1) * sizeof(value_type));
                    break;
                case ALLOC_RDATA:
                case ALLOC_HEAP:
                    _lalloc = other._lalloc;
                    other._lalloc.ptr = nullptr;
                    break;
                default:
                    break;
            }
            other._salloc.size.alloc_flags = ALLOC_STACK;
            other._salloc.size.data = 0;
        }

        ~basic_string()
        {
            if (_salloc.size.alloc_flags == ALLOC_HEAP) Allocator::deallocate(_lalloc.ptr);
        }

        basic_string &operator=(const basic_string &other) noexcept
        {
            if (this == &other) return *this;
            u8 self_alloc = _salloc.size.alloc_flags;
            if (other._salloc.size.alloc_flags == ALLOC_RDATA)
            {
                if (self_alloc == ALLOC_HEAP) Allocator::deallocate(_lalloc.ptr);
                _lalloc.ptr = other._lalloc.ptr;
                _lalloc.size = other._lalloc.size;
                _lalloc.cap = other._lalloc.cap;
                _salloc.size.alloc_flags = ALLOC_RDATA;
                return *this;
            }
            size_type other_size = other.size();
            size_type required_capacity = other.capacity();

            if (self_alloc == ALLOC_STACK && required_capacity <= _sso_size)
            {
                memcpy(_salloc.data, other._salloc.data, required_capacity * sizeof(value_type));
                _salloc.size = other._salloc.size;
                return *this;
            }

            if (!grow(required_capacity)) return *this;

            memcpy(_lalloc.ptr, other.c_str(), required_capacity * sizeof(value_type));
            _lalloc.size.data = other_size;

            return *this;
        }

        basic_string &operator=(basic_string &&other) noexcept
        {
            if (this == &other) return *this;
            if (_salloc.size.alloc_flags == ALLOC_HEAP) Allocator::deallocate(_lalloc.ptr);
            switch (other._salloc.size.alloc_flags)
            {
                case ALLOC_STACK:
                    _salloc = other._salloc;
                    break;

                case ALLOC_RDATA:
                case ALLOC_HEAP:
                    _lalloc = other._lalloc;
                    other._lalloc.ptr = nullptr;
                    other._lalloc.cap = 0;
                    other._salloc.size.alloc_flags = ALLOC_STACK;
                    other._lalloc.size.data = 0;
                    break;
            }
            return *this;
        }

        bool operator==(const basic_string &other) const noexcept
        {
            size_type len = size();
            return len == other.size() && compare_string(c_str(), size(), other.c_str(), other.size()) == 0;
        }

        bool operator!=(const basic_string &other) const noexcept { return !(*this == other); }

        constexpr int compare(const basic_string &str) const noexcept
        {
            return compare_string(c_str(), size(), str.c_str(), str.size());
        }

        constexpr int compare(size_type pos, size_type len, const basic_string &str) const
        {
            return compare(pos, len, str.c_str(), str.size());
        }

        constexpr int compare(size_type pos, size_type len, const basic_string &str, size_type subpos,
                              size_type sublen = npos) const
        {
            return compare(pos, len, str.c_str() + subpos, std::min(sublen, str.size() - subpos));
        }

        constexpr int compare(const_pointer s) const
        {
            return compare_string(c_str(), size(), s, null_terminated_length(s));
        }

        constexpr int compare(size_type pos, size_type len, const T *s) const
        {
            return compare(pos, len, s, null_terminated_length(s));
        }

        constexpr int compare(size_t pos, size_t len, const T *s, size_t n) const
        {
            size_type size = this->size();
            if (pos > size) throw out_of_range(size, pos);
            size_type compare_len = std::min(len, size - pos);
            return compare_string(c_str() + pos, compare_len, s, n);
        }

        void swap(basic_string &other) noexcept
        {
            if (this == &other) return;
            auto alloc = _salloc.size.alloc_flags;
            auto other_alloc = other._salloc.size.alloc_flags;

            if (alloc == ALLOC_STACK && other_alloc == ALLOC_STACK)
                std::swap(_salloc, other._salloc);
            else if (alloc != ALLOC_STACK && other_alloc != ALLOC_STACK)
                std::swap(_lalloc, other._lalloc);
            else
            {
                if (alloc == ALLOC_STACK)
                {
                    alloc_long_t temp = other._lalloc;
                    other._lalloc = std::move(_lalloc);
                    _lalloc = temp;
                    std::swap(_salloc.size, other._salloc.size);
                }
                else
                {
                    alloc_short_t temp = other._salloc;
                    other._salloc = std::move(_salloc);
                    _salloc = temp;
                    std::swap(_lalloc.size, other._lalloc.size);
                }
            }
        }

        basic_string operator+(const basic_string &other) const noexcept
        {
            size_type new_size = size() + other.size();
            basic_string result;

            if (new_size < _sso_size)
            {
                memcpy(result._salloc.data, c_str(), size() * sizeof(value_type));
                memcpy(result._salloc.data + size(), other.c_str(), (other.size() + 1) * sizeof(value_type));
                result._salloc.size.alloc_flags = ALLOC_STACK;
                result._salloc.size.data = new_size;
            }
            else if (_salloc.size.alloc_flags == ALLOC_HEAP && _lalloc.cap >= new_size + 1)
            {
                memcpy(_lalloc.ptr + size(), other.c_str(), (other.size() + 1) * sizeof(value_type));
                result = *this;
                result._lalloc.size.data = new_size;
            }
            else if (result.grow(new_size + 1))
            {
                memcpy(result._lalloc.ptr, c_str(), size() * sizeof(value_type));
                memcpy(result._lalloc.ptr + size(), other.c_str(), (other.size() + 1) * sizeof(value_type));
                result._lalloc.size.data = new_size;
            }

            return result;
        }

        basic_string &operator+=(const basic_string &other) noexcept
        {
            if (other.size() == 0) return *this;

            size_type old_size = size();
            size_type new_size = old_size + other.size();
            u8 alloc = _salloc.size.alloc_flags;

            if (alloc != ALLOC_HEAP && new_size < _sso_size)
            {
                if (alloc == ALLOC_RDATA)
                {
                    memcpy(_salloc.data, _lalloc.ptr, (old_size + 1) * sizeof(value_type));
                    _salloc.size.alloc_flags = ALLOC_STACK;
                }

                memcpy(_salloc.data + old_size, other.c_str(), (other.size() + 1) * sizeof(value_type));
                _salloc.size.data = new_size;
            }
            else
            {
                if (alloc != ALLOC_HEAP || _lalloc.cap < new_size + 1)
                    if (!grow(new_size + 1)) return *this;

                memcpy(_lalloc.ptr + old_size, other.c_str(), (other.size() + 1) * sizeof(value_type));
                _lalloc.size.data = new_size;
            }

            return *this;
        }

        basic_string operator+(value_type ch) const noexcept
        {
            size_type old_size = size();
            size_type new_size = old_size + 1;

            basic_string result;

            if (new_size < _sso_size)
            {
                memcpy(result._salloc.data, c_str(), old_size * sizeof(value_type));
                result._salloc.data[old_size] = ch;
                result._salloc.data[new_size] = '\0';
                result._salloc.size.data = new_size;
                result._salloc.size.alloc_flags = ALLOC_STACK;
            }
            else
            {
                if (!result.grow(new_size + 1)) return result;
                memcpy(result._lalloc.ptr, c_str(), old_size * sizeof(value_type));
                result._lalloc.ptr[old_size] = ch;
                result._lalloc.ptr[new_size] = '\0';
                result._lalloc.size.data = new_size;
            }

            return result;
        }

        basic_string &operator+=(value_type c) noexcept
        {
            size_type old_size = size();
            size_type new_size = old_size + 1;
            u8 alloc = _salloc.size.alloc_flags;

            if (alloc != ALLOC_HEAP && new_size < _sso_size)
            {
                if (alloc == ALLOC_RDATA)
                {
                    memcpy(_salloc.data, _lalloc.ptr, old_size * sizeof(value_type));
                    _salloc.size.data = old_size;
                    _salloc.size.alloc_flags = ALLOC_STACK;
                }

                _salloc.data[old_size] = c;
                _salloc.data[new_size] = '\0';
                _salloc.size.data = new_size;
            }
            else
            {
                if (alloc != ALLOC_HEAP || _lalloc.cap < new_size + 1)
                    if (!grow(new_size + 1)) return *this;

                _lalloc.ptr[old_size] = c;
                _lalloc.ptr[new_size] = '\0';
                _lalloc.size.data = new_size;
            }

            return *this;
        }

        basic_string &operator+=(const self_view &other) noexcept
        {
            size_type old_size = size();
            size_type other_size = other.size();
            size_type new_size = old_size + other_size;
            u8 alloc = _salloc.size.alloc_flags;

            if (alloc != ALLOC_HEAP && new_size < _sso_size)
            {
                if (alloc == ALLOC_RDATA)
                {
                    memcpy(_salloc.data, _lalloc.ptr, old_size * sizeof(value_type));
                    _salloc.data[old_size] = '\0';
                    _salloc.size.data = old_size;
                    _salloc.size.alloc_flags = ALLOC_STACK;
                }

                memcpy(_salloc.data + old_size, other.data(), other_size * sizeof(value_type));
                _salloc.data[new_size] = '\0';
                _salloc.size.data = new_size;
            }
            else
            {
                if (alloc != ALLOC_HEAP || _lalloc.cap < new_size + 1)
                    if (!grow(new_size + 1)) return *this;

                memcpy(_lalloc.ptr + old_size, other.data(), other_size * sizeof(value_type));
                _lalloc.ptr[new_size] = '\0';
                _lalloc.size.data = new_size;
            }

            return *this;
        }

        basic_string &operator+=(const_pointer other) noexcept { return *this += self_view(other); }

        operator self_view() const noexcept { return self_view(c_str(), size()); }

        size_type size() const noexcept
        {
            return _salloc.size.alloc_flags == ALLOC_STACK ? _salloc.size.data : _lalloc.size.data;
        }

        size_type length() const noexcept
        {
            return _salloc.size.alloc_flags == ALLOC_STACK ? _salloc.size.data : _lalloc.size.data;
        }

        size_type capacity() const noexcept
        {
            return _salloc.size.alloc_flags == ALLOC_STACK ? _salloc.size.data + 1 : _lalloc.cap;
        }

        const_pointer c_str() const noexcept
        {
            return _salloc.size.alloc_flags == ALLOC_STACK ? _salloc.data : _lalloc.ptr;
        }

        const_pointer data() const noexcept { return c_str(); }
        pointer data() noexcept { return _salloc.size.alloc_flags == ALLOC_STACK ? _salloc.data : _lalloc.ptr; }

        void clear() noexcept
        {
            if (_salloc.size.alloc_flags == ALLOC_HEAP) Allocator::deallocate(_lalloc.ptr);
            memset(_salloc.data, 0, _sso_size);
            _salloc.size.alloc_flags = ALLOC_STACK;
            _salloc.size.data = 0;
        }

        bool empty() const noexcept { return size() == 0; }

        basic_string substr(size_type pos, size_type len = npos) const noexcept
        {
            if (pos >= size()) return basic_string();
            return basic_string(c_str() + pos, std::min(len, size() - pos));
        }

        inline size_type find(value_type ch, size_type pos = 0) const noexcept
        {
            if (pos >= size()) return npos;
            const_pointer start = c_str() + pos;
            const_pointer p = strchr(start, ch);
            return p ? static_cast<size_type>(p - start) + pos : npos;
        }

        inline size_type find(const basic_string &str, size_type pos = 0) const noexcept
        {
            if (pos >= size() || str.size() > size() - pos) return npos;
            const_pointer start = c_str() + pos;
            const_pointer p = strstr(start, str.c_str());
            return p ? static_cast<size_type>(p - c_str()) : npos;
        }

        inline size_type rfind(value_type ch, size_type pos = npos) const noexcept
        {
            size_type len = size();
            return find_last_of(data(), pos >= len ? len : pos + 1, ch);
        }

        inline size_type rfind(const_pointer s, size_type pos, size_type count) const noexcept
        {
            if (count == 0)
            {
                if (size() == 0) return 0;
                if (pos == npos) return size();
                return (pos > size()) ? size() : pos;
            }
            if (count > size()) return npos;

            size_type start;
            if (pos == npos || pos + 1 < count)
                start = size() - count;
            else
                start = (pos >= size() ? size() - count : pos - count + 1);

            const_pointer base = c_str();
            for (;;)
            {
                if (memcmp(base + start, s, count) == 0) return start;
                if (start == 0) break;
                --start;
            }
            return npos;
        }

        inline size_type rfind(const_pointer s, size_type pos = npos) const noexcept
        {
            const size_type n = s ? static_cast<size_type>(null_terminated_length(s)) : 0;
            return rfind(s, pos, n);
        }

        template <typename S,
                  std::enable_if_t<is_string_like_v<value_type, S> && !std::is_convertible_v<const S &, const_pointer>,
                                   int> = 0>
        inline size_type rfind(const S &needle, size_type pos = npos) const noexcept
        {
            return rfind(needle.data(), pos, static_cast<size_type>(needle.size()));
        }

        basic_string &replace(size_type pos, size_type len, const basic_string &str) noexcept
        {
            size_type old_size = size();
            if (pos > old_size) return *this;

            const_pointer old_data = c_str();
            size_type actual_len = std::min(len, old_size - pos);
            size_type new_size = old_size - actual_len + str.size();

            u8 alloc = _salloc.size.alloc_flags;
            pointer new_ptr = nullptr;

            if (alloc != ALLOC_HEAP && new_size < _sso_size)
            {
                if (alloc == ALLOC_RDATA)
                {
                    memcpy(_salloc.data, _lalloc.ptr, old_size * sizeof(value_type));
                    _salloc.data[old_size] = '\0';
                    _salloc.size.data = old_size;
                    _salloc.size.alloc_flags = ALLOC_STACK;
                }
                new_ptr = _salloc.data;
            }
            else
            {
                if (alloc != ALLOC_HEAP || _lalloc.cap < new_size + 1)
                    if (!grow(new_size + 1)) return *this;
                new_ptr = _lalloc.ptr;
            }

            memmove(new_ptr, old_data, pos * sizeof(value_type));
            memcpy(new_ptr + pos, str.c_str(), str.size() * sizeof(value_type));

            if (pos + actual_len < old_size)
                memmove(new_ptr + pos + str.size(), old_data + pos + actual_len,
                        (old_size - (pos + actual_len)) * sizeof(value_type));

            new_ptr[new_size] = '\0';

            if (new_ptr == _salloc.data)
            {
                _salloc.size.data = new_size;
                _salloc.size.alloc_flags = ALLOC_STACK;
            }
            else
            {
                _lalloc.size.data = new_size;
                _salloc.size.alloc_flags = ALLOC_HEAP;
            }

            return *this;
        }

        basic_string &append(const basic_string &str) noexcept { return *this += str; }
        basic_string &append(const basic_string &str, size_type pos, size_type len) noexcept
        {
            return *this += str.substr(pos, len);
        }
        basic_string &append(const_pointer s) noexcept { return *this += s; }
        basic_string &append(const_pointer s, size_type n) noexcept { return *this += self_view(s, n); }
        basic_string &append(size_type n, value_type c) noexcept
        {
            for (size_type i = 0; i < n; ++i) *this += c;
            return *this;
        }
        template <class InputIt>
        basic_string &append(InputIt first, InputIt last) noexcept
        {
            for (auto it = first; it != last; ++it) *this += *it;
            return *this;
        }

        basic_string &erase(size_type pos = 0, size_type n = npos)
        {
            size_type sz = size();
            if (pos > sz) throw out_of_range(sz, pos);
            if (n == npos || pos + n > sz) n = sz - pos;
            if (n == 0) return *this;
            if (_salloc.size.alloc_flags == ALLOC_RDATA)
            {
                size_type old_size = sz;
                const value_type *old_ptr = _lalloc.ptr;

                if (old_size < _sso_size)
                {
                    memcpy(_salloc.data, old_ptr, (old_size + 1) * sizeof(value_type));
                    _salloc.size.alloc_flags = ALLOC_STACK;
                    _salloc.size.data = old_size;
                }
                else
                {
                    if (!grow(old_size + 1)) return *this;
                    memcpy(_lalloc.ptr, old_ptr, (old_size + 1) * sizeof(value_type));
                    _lalloc.size.data = old_size;
                    _lalloc.size.alloc_flags = ALLOC_HEAP;
                }
            }
            const u8 alloc = _salloc.size.alloc_flags;
            if (alloc == ALLOC_STACK)
            {
                size_type old_size = _salloc.size.data;
                size_type erase_count = (pos + n > old_size) ? old_size - pos : n;
                memmove(_salloc.data + pos, _salloc.data + pos + erase_count,
                        (old_size - (pos + erase_count) + 1) * sizeof(value_type));

                size_type new_size = old_size - erase_count;
                _salloc.size.data = static_cast<u8>(new_size);
            }
            else
            {
                size_type old_size = _lalloc.size.data;
                size_type erase_count = (pos + n > old_size) ? old_size - pos : n;

                memmove(_lalloc.ptr + pos, _lalloc.ptr + pos + erase_count,
                        (old_size - (pos + erase_count) + 1) * sizeof(value_type));

                size_type new_size = old_size - erase_count;
                _lalloc.size.data = new_size;
            }
            return *this;
        }

        iterator erase(const_iterator pos)
        {
            iterator start = begin();
            size_type diff = static_cast<size_type>(pos - start);
            erase(diff, 1);
            return begin() + diff;
        }
        iterator erase(const_iterator first, const_iterator last)
        {
            iterator start = begin();
            size_type diff = static_cast<size_type>(first - start);
            erase(diff, static_cast<size_type>(last - first));
            return begin() + diff;
        }

        basic_string &assign(const basic_string &str)
        {
            clear();
            return append(str);
        }

        basic_string &assign(const basic_string &str, size_type subpos, size_type sublen = npos)
        {
            clear();
            return append(str.substr(subpos, sublen));
        }

        basic_string &assign(const_pointer s)
        {
            clear();
            return append(s);
        }

        basic_string &assign(const_pointer s, size_type n)
        {
            clear();
            return append(s, n);
        }

        basic_string &assign(size_type n, value_type c)
        {
            clear();
            return append(n, c);
        }

        template <class InputIterator>
        basic_string &assign(InputIterator first, InputIterator last)
        {
            clear();
            return append(first, last);
        }

        basic_string &assign(std::initializer_list<value_type> il)
        {
            clear();
            return append(il.begin(), il.end());
        }

        basic_string &assign(basic_string &&str) noexcept
        {
            if (this != &str)
            {
                clear();
                swap(str);
            }
            return *this;
        }

        void push_back(value_type c) noexcept { *this += c; }

        void resize(size_type new_size, value_type fill = '\0') noexcept
        {
            size_type old_size = size();
            if (new_size == old_size) return;

            if (_salloc.size.alloc_flags == ALLOC_STACK && new_size < _sso_size)
            {
                if (new_size > old_size) memset(_salloc.data + old_size, fill, new_size - old_size);
                _salloc.data[new_size] = '\0';
                _salloc.size.data = new_size;
                return;
            }
            if (_salloc.size.alloc_flags != ALLOC_HEAP || _lalloc.cap < new_size + 1)
                if (!grow(new_size + 1)) return;

            if (new_size > old_size) memset(_lalloc.ptr + old_size, fill, new_size - old_size);
            _lalloc.ptr[new_size] = '\0';
            _lalloc.size.data = new_size;
        }

        void reserve(size_type required_capacity) noexcept
        {
            if (required_capacity < _sso_size) return;
            if (_salloc.size.alloc_flags == ALLOC_HEAP && _lalloc.cap >= required_capacity + 1) return;
            grow(required_capacity + 1);
        }

        iterator begin() noexcept { return (iterator)c_str(); }
        const_iterator begin() const noexcept { return c_str(); }
        const_iterator cbegin() const noexcept { return c_str(); }

        iterator end() noexcept { return begin() + size(); }
        const_iterator end() const noexcept { return begin() + size(); }
        const_iterator cend() const noexcept { return begin() + size(); }

        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

        reference operator[](size_type index) noexcept
        {
            return _salloc.size.alloc_flags == ALLOC_STACK ? _salloc.data[index] : _lalloc.ptr[index];
        }
        const_reference operator[](size_type index) const noexcept { return c_str()[index]; }

        reference at(size_type index)
        {
            if (index >= size()) throw acul::out_of_range(capacity(), index);
            return this->operator[](index);
        }

        reference front() noexcept { return *begin(); }
        const_reference front() const noexcept { return *begin(); }

        reference back() noexcept { return *(end() - 1); }
        const_reference back() const noexcept { return *(end() - 1); }

    private:
        enum
        {
            ALLOC_STACK = 0x0,
            ALLOC_RDATA = 0x1,
            ALLOC_HEAP = 0x2
        };

        struct alloc_long_t
        {
            struct
            {
                size_type alloc_flags : 2;
                size_type data : sizeof(size_type) * CHAR_BIT - 2;
            } size;
            pointer ptr;
            size_type cap;
        };

        enum
        {
            _sso_size = sizeof(alloc_long_t) / sizeof(value_type) - 1
        };

        struct alloc_short_t
        {
            struct
            {
                u8 alloc_flags : 2;
                u8 data : 6;
            } size;
            value_type data[_sso_size];
        };

        union
        {
            alloc_long_t _lalloc;
            alloc_short_t _salloc;
        };

        bool grow(size_type required_capacity) noexcept
        {
            size_type old_capacity, old_size;
            const_pointer old_data;
            pointer old_ptr;
            if (_salloc.size.alloc_flags == ALLOC_STACK)
            {
                old_size = _salloc.size.data;
                old_capacity = old_size + 1;
                old_data = _salloc.data;
                old_ptr = nullptr;
            }
            else // ALLOC_HEAP ot ALLOC_RDATA
            {
                old_size = _lalloc.size.data;
                old_capacity = _lalloc.cap;
                old_data = _lalloc.ptr;
                old_ptr = (_salloc.size.alloc_flags == ALLOC_HEAP) ? _lalloc.ptr : nullptr;
            }

            size_type new_capacity = get_growth_size(old_capacity, required_capacity);
            pointer new_ptr =
                old_ptr ? Allocator::reallocate(old_ptr, new_capacity) : Allocator::allocate(new_capacity);
            if (!new_ptr) return false;
            if (!old_ptr) memcpy(new_ptr, old_data, (old_size + 1) * sizeof(value_type));

            _salloc.size.alloc_flags = ALLOC_HEAP;
            _lalloc.ptr = new_ptr;
            _lalloc.cap = new_capacity;
            _lalloc.size.data = old_size;
            return true;
        }
    };

    template <typename T, typename Allocator>
    basic_string<T, Allocator> operator+(const T *lhs, const basic_string<T, Allocator> &rhs)
    {
        return basic_string<T, Allocator>(lhs) + rhs;
    }

#if defined(__cpp_impl_three_way_comparison) && __cpp_impl_three_way_comparison >= 201907
    template <typename T, typename Allocator>
    constexpr auto operator<=>(const basic_string<T, Allocator> &lhs, const basic_string<T, Allocator> &rhs) noexcept
    {
        return compare_string(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size());
    }

    template <typename T, typename Allocator>
    constexpr auto operator<=>(const basic_string<T, Allocator> &lhs, basic_string_view<T> rhs) noexcept
    {
        return compare_string(lhs.c_str(), lhs.size(), rhs.data(), rhs.size());
    }

    template <typename T, typename Allocator>
    constexpr auto operator<=>(basic_string_view<T> lhs, const basic_string<T, Allocator> &rhs) noexcept
    {
        return compare_string(lhs.data(), lhs.size(), rhs.c_str(), rhs.size());
    }
#endif
} // namespace acul

namespace std
{
    template <typename T, typename Allocator>
    struct hash<acul::basic_string<T, Allocator>>
    {
        size_t operator()(const acul::basic_string<T, Allocator> &s) const noexcept
        {
            return acul::cityhash64((const char *)s.c_str(), s.size());
        }
    };
} // namespace std

#if defined(__GNUC__) && !defined(__clang_analyzer__) && !defined(__clang__)
    #pragma GCC diagnostic pop
#endif
#endif