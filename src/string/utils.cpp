#include <acul/string/utils.hpp>
#include <cassert>
#include <cstdarg>
#include <cmath>

#ifdef _WIN32
    #include <winnls.h>
#endif

namespace acul
{
    u16string utf8_to_utf16(const string &src)
    {
        u16string out;
        for (size_t i = 0; i < src.size();)
        {
            const auto first = static_cast<u8>(src[i++]);
            u32 cp = first;
            size_t count = 0;
            u32 minimum = 0;
            if (first >= 0xC2 && first <= 0xDF)
            {
                cp = first & 0x1F;
                count = 1;
                minimum = 0x80;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                cp = first & 0x0F;
                count = 2;
                minimum = 0x800;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                cp = first & 0x07;
                count = 3;
                minimum = 0x10000;
            }
            else if (first >= 0x80) throw runtime_error("Invalid UTF-8 string.");
            if (count > src.size() - i) throw runtime_error("Invalid UTF-8 string.");
            for (size_t j = 0; j < count; ++j)
            {
                const auto next = static_cast<u8>(src[i++]);
                if ((next & 0xC0) != 0x80) throw runtime_error("Invalid UTF-8 string.");
                cp = (cp << 6) | (next & 0x3F);
            }
            if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                throw runtime_error("Invalid UTF-8 string.");
            if (cp < 0x10000) out.push_back(static_cast<char16_t>(cp));
            else
            {
                cp -= 0x10000;
                out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
                out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
            }
        }
        return out;
    }

    string utf16_to_utf8(const u16string &src)
    {
        string out;
        for (size_t i = 0; i < src.size(); ++i)
        {
            u32 cp = src[i];
            if (cp >= 0xD800 && cp <= 0xDBFF)
            {
                if (++i >= src.size() || src[i] < 0xDC00 || src[i] > 0xDFFF)
                    throw runtime_error("Invalid UTF-16 string.");
                cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i] - 0xDC00);
            }
            else if (cp >= 0xDC00 && cp <= 0xDFFF) throw runtime_error("Invalid UTF-16 string.");
            if (cp < 0x80) out.push_back(static_cast<char>(cp));
            else if (cp < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        return out;
    }

    string strip_controls(const string &input_str, size_t max)
    {
        string output;
        int from = -1;
        int length = -1;
        for (auto v : input_str)
        {
            if (v != '\f' && v != '\n' && v != '\r' && v != '\t' && v != '\v')
            {
                output += v;
                if (v != ' ')
                {
                    if (from == -1) from = output.size() - 1;
                    length = output.size() - 1;
                }
            }
        }
        if (from == -1 || length == -1) return {};
        size_t to = length - from + 1;
        if (to > max) to = max;
        return output.substr(from, to);
    }

#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif
    string format(const char *format, ...) noexcept
    {
        string result;
        va_list args;
        va_start(args, format);

        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(nullptr, 0, format, args_copy) + 1;
        va_end(args_copy);

        if (size <= 1) result = "";
        else
        {
            char buf[size];
            vsnprintf(buf, size, format, args);
            result = string(buf, buf + size - 1);
        }

        va_end(args);
        return result;
    }

    string format_va_list(const char *format, va_list args) noexcept
    {
        string result;
        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(nullptr, 0, format, args_copy) + 1;
        va_end(args_copy);
        if (size <= 1) result = "";
        else
        {
            char buf[size];
            va_copy(args_copy, args);
            vsnprintf(buf, size, format, args_copy);
            va_end(args_copy);
            result = string(buf, buf + size - 1);
        }
        return result;
    }

    template <typename T>
    int float_to_string(T value, char *buffer, size_t buffer_size, int precision)
    {
        assert(buffer != nullptr && "buffer must not be null");
        assert(precision >= 0 && precision <= 9 && "precision must be between 0 and 9");
        if (buffer_size < 2 || !std::isfinite(value)) return 0;

        char *ptr = buffer;
        char *end = buffer + buffer_size;

        // Sign
        if (value < static_cast<T>(0))
        {
            if (ptr == end) return 0;

            *ptr++ = '-';
            value = -value;
        }

        if (static_cast<long double>(value) > static_cast<long double>(UINT64_MAX)) return 0;

        static constexpr u64 powers_of_10[] = {1ULL,           10ULL,           100ULL,       1'000ULL,
                                               10'000ULL,      100'000ULL,      1'000'000ULL, 10'000'000ULL,
                                               100'000'000ULL, 1'000'000'000ULL};

        const u64 scale = powers_of_10[precision];

        u64 integer = static_cast<u64>(value);
        T fraction = value - static_cast<T>(integer);
        u64 fractional = static_cast<u64>(fraction * static_cast<T>(scale) + static_cast<T>(0.5));

        if (fractional >= scale && precision != 0)
        {
            fractional = 0;
            ++integer;
        }
        else if (precision == 0 && fraction >= static_cast<T>(0.5)) ++integer;

        // Integer part.
        char reverse[32];
        int digits = 0;

        do
        {
            reverse[digits++] = static_cast<char>('0' + integer % 10);
            integer /= 10;
        } while (integer != 0);

        if (static_cast<size_t>(end - ptr) < static_cast<size_t>(digits)) return 0;

        while (digits != 0) *ptr++ = reverse[--digits];

        if (precision == 0) return static_cast<int>(ptr - buffer);
        if (static_cast<size_t>(end - ptr) < static_cast<size_t>(precision + 1)) return 0;
        *ptr++ = '.';

        char *fraction_begin = ptr;
        ptr += precision;

        for (int i = 0; i < precision; ++i)
        {
            *--ptr = static_cast<char>('0' + fractional % 10);
            fractional /= 10;
        }

        ptr = fraction_begin + precision;
        return static_cast<int>(ptr - buffer);
    }

    template <typename T>
    int decimal_to_string(T value, char *buffer)
    {
        static_assert(std::is_integral_v<T>);

        using U = std::make_unsigned_t<T>;

        constexpr size_t max_digits = std::numeric_limits<U>::digits10 + 1;

        U unsigned_value;
        bool negative = false;

        if constexpr (std::is_signed_v<T>)
        {
            if (value < 0)
            {
                negative = true;
                unsigned_value = U(0) - static_cast<U>(value);
            }
            else unsigned_value = static_cast<U>(value);
        }
        else unsigned_value = value;

        char reverse[max_digits + (std::is_signed_v<T> ? 1 : 0)];
        char *end = reverse + sizeof(reverse);
        char *ptr = end;

        do
        {
            *--ptr = static_cast<char>('0' + unsigned_value % 10);
            unsigned_value /= 10;
        } while (unsigned_value != 0);

        if (negative) *--ptr = '-';

        const size_t length = static_cast<size_t>(end - ptr);
        std::memcpy(buffer, ptr, length);
        return static_cast<int>(length);
    }

    int to_string(int value, char *buffer) { return decimal_to_string(value, buffer); }
    int to_string(unsigned value, char *buffer) { return decimal_to_string(value, buffer); }
    int to_string(long value, char *buffer) { return decimal_to_string(value, buffer); }
    int to_string(unsigned long value, char *buffer) { return decimal_to_string(value, buffer); }
    int to_string(long long value, char *buffer) { return decimal_to_string(value, buffer); }
    int to_string(unsigned long long value, char *buffer) { return decimal_to_string(value, buffer); }

    int to_string(f32 value, char *buffer, size_t buffer_size, int precision)
    {
        return float_to_string(value, buffer, buffer_size, precision);
    }

    int to_string(f64 value, char *buffer, size_t buffer_size, int precision)
    {
        return float_to_string(value, buffer, buffer_size, precision);
    }
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif

    bool stoi(const char *&str, int &value)
    {
        const char *ptr = str;
        int sign = 1;

        while (is_space(*ptr)) ++ptr;

        if (*ptr == '-' || *ptr == '+') sign = (*ptr++ == '-') ? -1 : 1;

        int result = 0;
        while (is_digit(*ptr)) result = (result * 10) + (*ptr++ - '0');

        value = result * sign;
        str = ptr;
        return (sign == 1) ? (result != 0) : true;
    }

    bool stoull(const char *&str, unsigned long long &value)
    {
        const char *ptr = str;

        // Skip white spaces
        while (is_space(*ptr)) ++ptr;

        // Parse integer part
        unsigned long long result = 0;
        while (is_digit(*ptr)) result = (result * 10) + (*ptr++ - '0');

        value = result;
        str = ptr;
        return true;
    }

    bool stoull_hex(const char *&str, unsigned long long &value)
    {
        const char *ptr = str;

        while (is_space(*ptr)) ++ptr;

        if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) ptr += 2;
        if (!isxdigit(*ptr)) return false;

        unsigned long long result = 0;

        while (isxdigit(*ptr))
        {
            unsigned long long digit = 0;

            if (*ptr >= '0' && *ptr <= '9') digit = *ptr - '0';
            else if (*ptr >= 'a' && *ptr <= 'f') digit = 10 + (*ptr - 'a');
            else if (*ptr >= 'A' && *ptr <= 'F') digit = 10 + (*ptr - 'A');

            if (result > (ULLONG_MAX - digit) / 16) return false;

            result = result * 16 + digit;
            ++ptr;
        }

        value = result;
        str = ptr;
        return true;
    }

    bool stof(const char *&str, f32 &value)
    {
        const char *ptr = str;
        bool negative = false;

        // Skip white spaces
        while (is_space(static_cast<unsigned char>(*ptr))) ++ptr;

        // Check optional sign
        if (*ptr == '+')
        {
            ++ptr;
        }
        else if (*ptr == '-')
        {
            negative = true;
            ++ptr;
        }

        // Parse integer part
        if (!is_digit(static_cast<unsigned char>(*ptr))) return false;

        f32 result = 0.0f;

        while (is_digit(static_cast<unsigned char>(*ptr)))
        {
            result *= 10.0f;
            result += static_cast<f32>(*ptr - '0');
            ++ptr;
        }

        // Parse fractional part
        if (*ptr == '.')
        {
            ++ptr;

            f32 scale = 0.1f;

            while (is_digit(static_cast<unsigned char>(*ptr)))
            {
                result += static_cast<f32>(*ptr - '0') * scale;
                scale *= 0.1f;
                ++ptr;
            }
        }

        // Parse exponential part
        if (*ptr == 'e' || *ptr == 'E')
        {
            ++ptr;

            bool exponent_negative = false;

            if (*ptr == '+')
            {
                ++ptr;
            }
            else if (*ptr == '-')
            {
                exponent_negative = true;
                ++ptr;
            }

            if (!is_digit(static_cast<unsigned char>(*ptr))) return false;

            int exponent_part = 0;

            while (is_digit(static_cast<unsigned char>(*ptr)))
            {
                exponent_part *= 10;
                exponent_part += *ptr - '0';
                ++ptr;
            }

            const f32 exponent_scale = std::pow(10.0f, static_cast<f32>(exponent_part));

            if (exponent_negative) result /= exponent_scale;
            else result *= exponent_scale;
        }

        value = negative ? -result : result;

        str = ptr;
        return true;
    }

    string read_word(const char *&str)
    {
        const char *begin = str;
        while (is_space(*begin)) ++begin;
        const char *end = begin;
        while (*end && !is_space(*end)) ++end;
        str = end;
        return string(begin, end);
    }
} // namespace acul
