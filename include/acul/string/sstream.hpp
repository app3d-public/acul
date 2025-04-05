#pragma once

#include "../fwd/sstream.hpp"
#include "string.hpp"
#include "string_view.hpp"
#include "utils.hpp"

namespace acul
{
    template <typename T, typename Allocator>
    class basic_stringstream
    {
    public:
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;
        using stringtype = basic_string<T, Allocator>;

        explicit basic_stringstream() noexcept = default;

        basic_stringstream &operator<<(const stringtype &str)
        {
            _data.append(str);
            return *this;
        }

        basic_stringstream &operator<<(const basic_string_view<T> &str)
        {
            _data.append(str.data(), str.size());
            return *this;
        }

        basic_stringstream &operator<<(const_pointer str)
        {
            _data.append(str);
            return *this;
        }

        basic_stringstream &operator<<(value_type ch)
        {
            _data.push_back(ch);
            return *this;
        }

        template <typename Integer, std::enable_if_t<is_nonchar_integer<Integer>::value, int> = 0>
        basic_stringstream &operator<<(Integer num)
        {
            char buf[num_to_strbuf_size<Integer>()];
            int len = to_string(num, buf);
            if (len != 0) _data.append(buf, len);
            return *this;
        }

        template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, int> = 0>
        basic_stringstream &operator<<(Float num)
        {
            char buf[num_to_strbuf_size<Float>()];
            int len = to_string(num, buf, sizeof(buf), 0);
            if (len != 0) _data.append(buf, len);
            return *this;
        }

        basic_stringstream &operator<<(std::streambuf *sb)
        {
            if (!sb) return *this;
            std::array<T, BUFSIZ> buffer;
            while (true)
            {
                std::streamsize count = sb->sgetn(buffer.data(), BUFSIZ);
                if (count <= 0) break;
                _data.append(buffer.data(), static_cast<size_t>(count));
                if (count < BUFSIZ) break;
            }
            return *this;
        }

        basic_stringstream &write(const void *data, size_type size)
        {
            if (!data || size == 0) return *this;
            const_pointer ptr = static_cast<const_pointer>(data);
            _data.append(ptr, size);
            return *this;
        }

        basic_string<T, Allocator> str() const noexcept { return _data; }

        void clear() noexcept { _data.clear(); }

    private:
        basic_string<T, Allocator> _data;
    };
} // namespace acul
