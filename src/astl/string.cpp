#include <astl/string.hpp>
#include <cmath>
#include <cstdarg>
#include <cstring>

#ifdef _WIN32
    #include <winnls.h>
#endif

namespace astl
{
    std::u16string utf8_to_utf16(const std::string &src)
    {
        std::u16string out;
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
                    out.append(1, static_cast<char16_t>(0xd800 + (codepoint >> 10)));
                    out.append(1, static_cast<char16_t>(0xdc00 + (codepoint & 0x03ff)));
                }
                else if (codepoint < 0xd800 || codepoint >= 0xe000)
                    out.append(1, static_cast<char16_t>(codepoint));
            }
        }
        return out;
    }

    std::string utf16_to_utf8(const std::u16string &src)
    {
        std::string result;
        for (auto cp : src)
        {
            if (cp < 0x80)
                result.push_back(static_cast<uint8_t>(cp));
            else if (cp < 0x800)
            {
                uint8_t chs[] = {static_cast<uint8_t>((cp >> 6) | 0xc0), static_cast<uint8_t>((cp & 0x3f) | 0x80)};
                result.append(std::begin(chs), std::end(chs));
            }
            else
            {
                uint8_t chs[] = {static_cast<uint8_t>((cp >> 12) | 0xe0),
                                 static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80),
                                 static_cast<uint8_t>((cp & 0x3f) | 0x80)};
                result.append(std::begin(chs), std::end(chs));
            }
        }
        return result;
    }

#ifdef _WIN32
    std::filesystem::path make_path(const std::string &path)
    {
        int wide_size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        if (wide_size <= 0) return {};

        std::wstring wstr;
        wstr.resize(wide_size - 1);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wstr[0], wide_size);

        return std::filesystem::path(wstr);
    }
#endif

    std::u16string trim(const std::u16string &inputStr, size_t max)
    {
        std::u16string output;
        int from = -1;
        int length = -1;
        for (char16_t v : inputStr)
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

    std::string trim(const std::string &inputStr, size_t max)
    {
        std::u16string u16string = trim(utf8_to_utf16(inputStr), max);
        return utf16_to_utf8(u16string);
    }

    std::string format(const char *format, ...) noexcept
    {
        std::string result;
        va_list args;
        va_start(args, format);
        int size = vsnprintf(nullptr, 0, format, args) + 1;
        if (size <= 1)
            result = "";
        else
        {
            char buf[size];
            vsnprintf(buf, size, format, args);
            result = std::string(buf, buf + size - 1);
        }
        va_end(args);
        return result;
    }

    std::string format_va_list(const char *format, va_list args) noexcept
    {
        std::string result;
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
            result = std::string(buf, buf + size - 1);
        }
        return result;
    }

    int to_string(int value, char *buffer, size_t buffer_size)
    {
        if (buffer_size < 2) return 0;

        char *ptr = buffer;

        // Sign
        if (value < 0)
        {
            *ptr = '-';
            value = -value;
            ptr++;
            buffer_size--;
        }

        // If zero
        if (value == 0)
        {
            if (buffer_size >= 2)
            {
                *ptr = '0';
                ++ptr;
                return 1;
            }
            else
                return 0;
        }

        int numDigits = 0;
        int tempValue = value;
        while (tempValue > 0)
        {
            numDigits++;
            tempValue /= 10;
        }
        if (numDigits > buffer_size - 1) return 0;

        // Reverse order array for storing digits
        char reverseOrder[numDigits];

        int i = 0;
        do {
            reverseOrder[i++] = value % 10;
            value /= 10;
        } while (value);

        // Writing digits to the buffer in the correct order
        while (i--)
        {
            *ptr = '0' + reverseOrder[i];
            ptr++;
        }
        return ptr - buffer;
    }

    int to_string(float value, char *buffer, size_t buffer_size, int precision)
    {
        if (buffer_size < 2) return 0;

        char *ptr = buffer;

        // Обработка знака числа
        if (value < 0)
        {
            *ptr = '-';
            value = -value;
            ptr++;
            buffer_size--;
        }

        // Обработка случая, когда значение меньше единицы
        if (fabs(value) < pow(10, -precision))
        {
            if (buffer_size >= 2)
            {
                *ptr = '0';
                ptr++;
                buffer_size--;
                if (precision > 0 && buffer_size > precision + 1)
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

        float tempValue = value;
        int numDigits = 0;
        if (tempValue >= 1)
        {
            while (tempValue >= 1)
            {
                numDigits++;
                tempValue /= 10;
            }
        }
        int intValue = (int)value;
        if (numDigits > buffer_size - 1) return 0;

        // Reverse order array for storing digits
        char reverseOrder[numDigits];

        int i = 0;
        do {
            reverseOrder[i++] = intValue % 10;
            intValue /= 10;
        } while (intValue);

        // Writing digits to the buffer in the correct order
        while (i--)
        {
            *ptr = '0' + reverseOrder[i];
            ptr++;
        }

        if (value - (int)value > 0 || precision > 0)
        {
            if (buffer_size - numDigits - 2 < 0) return 0;

            *ptr = '.';
            ptr++;

            float fraction = value - (int)value;

            int numFractionalDigits = 0;
            while (fraction > 0 && (numFractionalDigits < precision || precision == 0) && buffer_size > 2)
            {
                fraction *= 10;
                int digit = (int)(fraction + 0.5f);
                fraction -= digit;
                *ptr = '0' + digit;
                ptr++;
                numFractionalDigits++;
            }

            // Добавление нулей для достижения точности
            while (numFractionalDigits < precision)
            {
                *ptr = '0';
                ptr++;
                numFractionalDigits++;
            }
        }

        return ptr - buffer;
    }
#ifdef CORE_GLM_ENABLE
    int to_string(const glm::vec2 &vec, char *buffer, size_t buffer_size, size_t offset)
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

    int to_string(const glm::vec3 &vec, char *buffer, size_t buffer_size, size_t offset)
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

    bool stof(const char *&str, float &value)
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

        float integerPart = 0.0;
        while (isdigit(*ptr))
        {
            integerPart *= 10.0;
            integerPart += *ptr - '0';
            ptr++;
        }

        // Parse fractional part
        float fractionalPart = 0.0;
        float fractionalDiv = 1.0;
        if (*ptr == '.')
        {
            ptr++;
            while (isdigit(*ptr))
            {
                fractionalPart *= 10.0;
                fractionalPart += *ptr - '0';
                fractionalDiv *= 10.0;
                ptr++;
            }
        }

        // Parse exponential part
        int exponentPart = 0;
        if (*ptr == 'e' || *ptr == 'E')
        {
            ptr++;
            bool exponentNegative = false;
            if (*ptr == '+')
                ptr++;
            else if (*ptr == '-')
            {
                exponentNegative = true;
                ptr++;
            }

            if (!isdigit(*ptr)) return false; // At least one digit is required

            while (isdigit(*ptr))
            {
                exponentPart *= 10;
                exponentPart += *ptr - '0';
                ptr++;
            }

            if (exponentNegative) exponentPart = -exponentPart;
        }

        float result = (integerPart + fractionalPart / fractionalDiv) * std::pow(10.0, exponentPart);
        value = negative ? -result : result;

        str = ptr; // Update the input pointer
        return true;
    }

    std::string str_range(const char *&str)
    {
        const char *begin = str;
        while (isspace(*begin)) ++begin;
        const char *end = begin;
        while (*end && !isspace(*end)) ++end;
        str = end;
        return std::string(begin, end);
    }

    std::string trim_end(const char *str)
    {
        if (!str) return "";
        const char *end = str + std::strlen(str);
        while (end != str && (*(end - 1) == '\n' || *(end - 1) == '\r')) --end;
        return std::string(str, end);
    }
} // namespace astl