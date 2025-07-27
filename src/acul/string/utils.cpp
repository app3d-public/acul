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
        unsigned int codepoint;
        for (size_t i = 0; i < src.size();)
        {
            unsigned char ch = static_cast<unsigned char>(src[i]);
            if (ch <= 0x7f)
                codepoint = ch;
            else if (ch <= 0xbf)
                codepoint = (codepoint << 6) | (ch & 0x3f);
            else if (ch <= 0xdf)
                codepoint = ch & 0x1f;
            else if (ch <= 0xef)
                codepoint = ch & 0x0f;
            else
                codepoint = ch & 0x07;
            i++;
            if (((src[i] & 0xc0) != 0x80) && (codepoint <= 0x10ffff))
            {
                if (codepoint > 0xffff)
                {
                    c16 buf[2] = {static_cast<c16>((codepoint >> 10) + 0xd800),
                                  static_cast<c16>((codepoint & 0x03ff) + 0xdc00)};
                    out.append(buf, 2);
                }
                else if (codepoint < 0xd800 || codepoint >= 0xe000)
                    out.push_back(codepoint);
            }
        }
        return out;
    }

    string utf16_to_utf8(const u16string &src)
    {
        string result;
        for (auto cp : src)
        {
            if (cp < 0x80)
                result.push_back(static_cast<u8>(cp));
            else if (cp < 0x800)
            {
                u8 chs[] = {static_cast<u8>((cp >> 6) | 0xc0), static_cast<u8>((cp & 0x3f) | 0x80)};
                result.append(std::begin(chs), std::end(chs));
            }
            else
            {
                u8 chs[] = {static_cast<u8>((cp >> 12) | 0xe0), static_cast<u8>(((cp >> 6) & 0x3f) | 0x80),
                            static_cast<u8>((cp & 0x3f) | 0x80)};
                result.append(std::begin(chs), std::end(chs));
            }
        }
        return result;
    }

    u16string trim(const u16string &input_str, size_t max)
    {
        u16string output;
        int from = -1;
        int length = -1;
        for (c16 v : input_str)
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
        if (from == -1 || length == -1) return u"";
        size_t to = length - from + 1;
        if (to > max) to = max;
        return output.substr(from, to);
    }

    string trim(const string &input_str, size_t max)
    {
        u16string u16string = trim(utf8_to_utf16(input_str), max);
        return utf16_to_utf8(u16string);
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
#ifdef ACUL_MATH_TYPES
    int to_string(const vec2 &vec, char *buffer, size_t buffer_size, size_t offset)
    {
        int written = to_string(vec.x, buffer + offset, buffer_size - offset, 5);
        if (written == 0) return 0;
        offset += written;
        if (offset < buffer_size - 1)
            buffer[offset++] = ' ';
        else
            return 0;

        written = to_string(vec.y, buffer + offset, buffer_size - offset, 5);
        if (written == 0) return 0;
        offset += written;

        return offset;
    }

    int to_string(const vec3 &vec, char *buffer, size_t buffer_size, size_t offset)
    {
        int written = to_string(vec.x, buffer + offset, buffer_size - offset, 5);
        if (written == 0) return 0;
        offset += written;
        if (offset < buffer_size - 1)
            buffer[offset++] = ' ';
        else
            return 0;

        written = to_string(vec.y, buffer + offset, buffer_size - offset, 5);
        if (written == 0) return 0;
        offset += written;
        if (offset < buffer_size - 1)
            buffer[offset++] = ' ';
        else
            return 0;

        written = to_string(vec.z, buffer + offset, buffer_size - offset, 5);
        if (written == 0) return 0;
        offset += written;

        return offset;
    }
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

    bool stoull(const char *&str, void *&value)
    {
        const char *ptr = str;
        unsigned long long result = 0;

        while (isspace(*ptr)) ++ptr;

        if (!(ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X'))) return false;
        ptr += 2;
        if (!isxdigit(*ptr)) return false;

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

        value = reinterpret_cast<void *>(result);
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

    string str_range(const char *&str)
    {
        const char *begin = str;
        while (isspace(*begin)) ++begin;
        const char *end = begin;
        while (*end && !isspace(*end)) ++end;
        str = end;
        return string(begin, end);
    }

    string trim_end(const char *str)
    {
        if (!str) return "";
        const char *end = str + null_terminated_length(str);
        while (end != str && isspace(*(end - 1))) --end;
        return string(str, end);
    }
} // namespace acul