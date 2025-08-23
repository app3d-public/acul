#pragma once
#include "string.hpp"

namespace acul
{

    /// @brief Convert string in UTF-8 encoding to string in UTF-16 encoding
    /// @param src String in UTF-8 encoding
    APPLIB_API u16string utf8_to_utf16(const string &src);

    /// @brief Convert string in UTF-16 encoding to string in UTF-8 encoding
    /// @param src String in UTF-16 encoding
    APPLIB_API string utf16_to_utf8(const u16string &src);

    APPLIB_API u16string trim(const u16string &input_str, size_t max = std::numeric_limits<size_t>::max());

    APPLIB_API string trim(const string &input_str, size_t max = std::numeric_limits<size_t>::max());

    /**
     * @brief Formats a string using a format string and arguments.
     * @param format The format string.
     * @param args The arguments to format the string.
     * @return The formatted string.
     */
    APPLIB_API string format(const char *format, ...) noexcept;

    APPLIB_API string format_va_list(const char *format, va_list args) noexcept;

    template <typename T>
    constexpr size_t num_to_strbuf_size()
    {
        return std::numeric_limits<T>::digits10 + 2; // Numbers + -/+ + \0
    }

    /**
     * @brief Converts the integer to the C-style string
     * @param value Source value
     * @param buffer Destination buffer
     * @param bufferSize Buffer size
     * @return The number of characters written
     */
    template <typename T>
    int to_string(T value, char *buffer)
    {
        size_t buffer_size = num_to_strbuf_size<T>();
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

        size_t num_digits = 0;
        int tmp = value;
        while (tmp > 0)
        {
            num_digits++;
            tmp /= 10;
        }
        if (num_digits > buffer_size - 1) return 0;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif
        // Reverse order array for storing digits
        char reverse_order[num_digits];
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
        int i = 0;
        do {
            reverse_order[i++] = value % 10;
            value /= 10;
        } while (value);

        // Writing digits to the buffer in the correct order
        while (i--)
        {
            *ptr = '0' + reverse_order[i];
            ptr++;
        }
        return ptr - buffer;
    }

    /**
     * @brief Converts the float to the C-style string
     * @param value Source value
     * @param buffer Destination buffer
     * @param bufferSize Buffer size
     * @param precision Precision
     * @return The number of characters written
     **/
    APPLIB_API int to_string(f32 value, char *buffer, size_t buffer_size, int precision);

    template <typename T>
    inline auto to_string(T value) -> std::enable_if_t<std::is_integral_v<T>, acul::string>
    {
        char buffer[num_to_strbuf_size<T>()];
        int len = to_string(value, buffer);
        return string(buffer, len);
    }

    template <typename T>
    inline auto to_u16string(T value) -> std::enable_if_t<std::is_integral_v<T>, acul::u16string>
    {
        string str = to_string(value);
        return utf8_to_utf16(str);
    }

    /**
     * @brief Deserialize the C-style string to the integer
     * @param str Source string
     * @param value Destination value
     * @return True if successful. Otherwise false
     **/
    APPLIB_API bool stoi(const char *&str, int &value);

    /**
     * @brief Deserialize the C-style string to the unsigned long long
     * @param str Source string
     * @param value Destination value
     * @return True if successful. Otherwise false
     **/
    APPLIB_API bool stoull(const char *&str, unsigned long long &value);

    /**
     * @brief Deserialize the C-style string to the pointer in hex format
     * @param str Source string
     * @param value Destination value
     * @return True if successful. Otherwise false
     **/
    APPLIB_API bool stoull(const char *&str, void *&value);

    /**
     * @brief Deserialize the C-style string to the float32
     * @param str Source string
     * @param value Destination value
     * @return True if successful. Otherwise false
     **/
    APPLIB_API bool stof(const char *&str, f32 &value);

    /**
     * @brief Extracts a substring from the input string, from the first space character to the last one.
     */
    APPLIB_API string str_range(const char *&str);

    APPLIB_API string trim_end(const char *str);
} // namespace acul