#include <acul/string/utils.hpp>
#include <cmath>
#include <cstdarg>
#include <cstring>

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

        if (size <= 1) { result = ""; }
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
        if (size <= 1)
            result = "";
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

    int to_string(f32 value, char *buffer, size_t buffer_size, int precision)
    {
        if (buffer_size < 2) return 0;

        char *ptr = buffer;
        if (value < 0)
        {
            *ptr = '-';
            value = -value;
            ptr++;
            buffer_size--;
        }

        if (fabs(value) < pow(10, -precision))
        {
            if (buffer_size >= 2)
            {
                *ptr = '0';
                ptr++;
                --buffer_size;
                if (precision > 0 && buffer_size > static_cast<size_t>(precision + 1))
                {
                    *ptr = '.';
                    ptr++;

                    for (int i = 0; i < precision; i++)
                    {
                        *ptr = '0';
                        ptr++;
                    }

                    return ptr - buffer;
                }
                return ptr - buffer - 1;
            }
            else
                return 0;
        }

        f32 temp_value = value;
        int num_digits = 0;
        if (temp_value >= 1)
        {
            while (temp_value >= 1)
            {
                ++num_digits;
                temp_value /= 10;
            }
        }
        int int_value = (int)value;
        if (num_digits < 0 || (size_t)num_digits > buffer_size - 1) return 0;

        // Reverse order array for storing digits
        char reverse_order[num_digits];

        int i = 0;
        do {
            reverse_order[i++] = int_value % 10;
            int_value /= 10;
        } while (int_value);

        // Writing digits to the buffer in the correct order
        while (i--)
        {
            *ptr = '0' + reverse_order[i];
            ptr++;
        }

        if (value - (int)value > 0 || precision > 0)
        {
            if (buffer_size - num_digits - 2 < 0) return 0;

            *ptr = '.';
            ptr++;

            f32 fraction = value - (int)value;

            int num_fractional_digits = 0;
            while (fraction > 0 && (num_fractional_digits < precision || precision == 0) && buffer_size > 2)
            {
                fraction *= 10;
                int digit = (int)(fraction + 0.5f);
                fraction -= digit;
                *ptr = '0' + digit;
                ptr++;
                ++num_fractional_digits;
            }

            while (num_fractional_digits < precision)
            {
                *ptr = '0';
                ++ptr;
                ++num_fractional_digits;
            }
        }

        return ptr - buffer;
    }
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif

    bool stoi(const char *&str, int &value)
    {
        const char *ptr = str;
        int sign = 1;

        // Skip white spaces
        while (isspace(*ptr)) ++ptr;

        // Check sign
        if (*ptr == '-' || *ptr == '+') sign = (*ptr++ == '-') ? -1 : 1;

        // Parse integer part
        int result = 0;
        while (isdigit(*ptr)) result = (result * 10) + (*ptr++ - '0');

        value = result * sign;
        str = ptr; // Update the input pointer
        return (sign == 1) ? (result != 0) : true;
    }

    bool stoull(const char *&str, unsigned long long &value)
    {
        const char *ptr = str;

        // Skip white spaces
        while (isspace(*ptr)) ++ptr;

        // Parse integer part
        unsigned long long result = 0;
        while (isdigit(*ptr)) result = (result * 10) + (*ptr++ - '0');

        value = result;
        str = ptr;
        return true;
    }

    bool stoull_hex(const char *&str, unsigned long long &value)
    {
        const char *ptr = str;

        while (isspace(*ptr)) ++ptr;

        if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) ptr += 2;
        if (!isxdigit(*ptr)) return false;

        unsigned long long result = 0;

        while (isxdigit(*ptr))
        {
            unsigned long long digit = 0;

            if (*ptr >= '0' && *ptr <= '9')
                digit = *ptr - '0';
            else if (*ptr >= 'a' && *ptr <= 'f')
                digit = 10 + (*ptr - 'a');
            else if (*ptr >= 'A' && *ptr <= 'F')
                digit = 10 + (*ptr - 'A');

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
        while (isspace(*ptr)) ptr++;

        // Check optional sign
        if (*ptr == '+')
            ptr++;
        else if (*ptr == '-')
        {
            negative = true;
            ptr++;
        }

        // Parse integer part
        if (!isdigit(*ptr)) return false; // At least one digit is required

        f32 integer_part = 0.0;
        while (isdigit(*ptr))
        {
            integer_part *= 10.0;
            integer_part += *ptr - '0';
            ptr++;
        }

        // Parse fractional part
        f32 fractional_part = 0.0;
        f32 fractional_div = 1.0;
        if (*ptr == '.')
        {
            ptr++;
            while (isdigit(*ptr))
            {
                fractional_part *= 10.0;
                fractional_part += *ptr - '0';
                fractional_div *= 10.0;
                ptr++;
            }
        }

        // Parse exponential part
        int exponent_part = 0;
        if (*ptr == 'e' || *ptr == 'E')
        {
            ptr++;
            bool exponent_negative = false;
            if (*ptr == '+')
                ptr++;
            else if (*ptr == '-')
            {
                exponent_negative = true;
                ptr++;
            }

            if (!isdigit(*ptr)) return false; // At least one digit is required

            while (isdigit(*ptr))
            {
                exponent_part *= 10;
                exponent_part += *ptr - '0';
                ptr++;
            }

            if (exponent_negative) exponent_part = -exponent_part;
        }

        f32 result = (integer_part + fractional_part / fractional_div) * pow(10.0, exponent_part);
        value = negative ? -result : result;

        str = ptr; // Update the input pointer
        return true;
    }

    string read_word(const char *&str)
    {
        const char *begin = str;
        while (isspace(*begin)) ++begin;
        const char *end = begin;
        while (*end && !isspace(*end)) ++end;
        str = end;
        return string(begin, end);
    }
} // namespace acul
