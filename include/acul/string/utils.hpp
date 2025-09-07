#pragma once

#include "../vector.hpp"
#include "string.hpp"

#ifdef _WIN32
    #define ACUL_C_STR(S)    L##S
    #define ACUL_NATIVE_CHAR wchar_t
#else
    #define ACUL_C_STR(S)    S
    #define ACUL_NATIVE_CHAR char
#endif

namespace acul
{

    /// @brief Convert string in UTF-8 encoding to string in UTF-16 encoding
    /// @param src String in UTF-8 encoding
    APPLIB_API u16string utf8_to_utf16(const string &src);

    /// @brief Convert string in UTF-16 encoding to string in UTF-8 encoding
    /// @param src String in UTF-16 encoding
    APPLIB_API string utf16_to_utf8(const u16string &src);

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
     * @param buffer_size Buffer size
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

    /// Removes control whitespace characters (\f, \n, \r, \t, \v) from the input string,
    /// trims leading and trailing spaces, and optionally truncates the result to `max` length.
    /// Internal spaces (' ') are preserved.
    /// Example:
    ///   strip_controls("   hello  \t\r world \n\t") -> "hello   world"
    APPLIB_API string strip_controls(const string &input_str, size_t max = std::numeric_limits<size_t>::max());

    /// Reads the next word (sequence of non-space characters) from a C-string,
    /// skipping leading spaces. Advances the input pointer to the end of the word.
    /// Example:
    ///   const char* s = "   hello world";
    ///   read_word(s) -> "hello", s points to " world"
    APPLIB_API string read_word(const char *&str);

    /// Removes leading and trailing whitespace characters from the given string-like object.
    // Internal whitespace is preserved. Accepts any type `S` that provides `.size()` and `.substr()`.
    ///
    /// Example:
    ///   trim("   hello  \t world \n") -> "hello  \t world"
    template <typename S>
    inline S trim(const S &s)
    {
        size_t a = 0;
        while (a < s.size() && isspace((unsigned char)s[a])) ++a;
        size_t b = s.size();
        while (b > a && isspace((unsigned char)s[b - 1])) --b;
        return s.substr(a, b - a);
    }

    /// Removes leading spaces and control whitespace characters (\f, \n, \r, \t, \v)
    /// from the given C-string. Trailing spaces are preserved.
    /// Example:
    ///   trim_start("   \t\nhello  ") -> "hello  "
    inline string trim_start(const char *str, size_t len)
    {
        if (!str) return "";
        const char *begin = str, *end = str + len;
        while (begin < end && isspace((unsigned char)*begin)) ++begin;
        return string(begin, end - begin);
    }

    inline size_t find_insensitive_case(const char *str, size_t len, const char *find)
    {
        if (!str || !find) return (size_t)-1;

#ifdef _WIN32
        size_t n = len;
        size_t m = null_terminated_length(find);
        if (m == 0 || m > n) return (size_t)-1;

        auto lower = [](unsigned char c) -> unsigned char { return (c >= 'A' && c <= 'Z') ? c + 32 : c; };

        for (size_t i = 0; i + m <= n; ++i)
        {
            size_t j = 0;
            for (; j < m; ++j)
            {
                if (lower((unsigned char)str[i + j]) != lower((unsigned char)find[j])) { break; }
            }
            if (j == m) return i;
        }
        return (size_t)-1;

#else
        const char *p = strcasestr(str, find);
        return p ? static_cast<size_t>(p - str) : (size_t)-1;
#endif
    }

    inline size_t find_insensitive_case(const string &str, const char *find)
    {
        return find_insensitive_case(str.c_str(), str.size(), find);
    }

    /// Replaces all occurrences of `old_char` in the given C-string with `new_char`.
    inline string replace(const char *str, size_t len, char old_char, char new_char)
    {
        string result;
        result.resize(len);
        for (size_t i = 0; i < len; ++i) result[i] = str[i] == old_char ? new_char : str[i];
        return result;
    }

    /// Replaces all occurrences of `old_char` in the given string with `new_char`.
    template <typename S, typename = std::enable_if_t<is_string_like_v<char, S>>>
    inline string replace(const S &str, char old_char, char new_char)
    {
        return replace(str.data(), str.size(), old_char, new_char);
    }

    /// Removes leading spaces and control whitespace characters (\f, \n, \r, \t, \v)
    /// from the given C-string. Trailing spaces are preserved.
    /// Example:
    ///   trim_start("   \t\nhello  ") -> "hello  "
    template <typename S>
    inline string trim_start(const S &s)
    {
        return trim_start(s.data(), s.size());
    }

    /// Returns a copy of the input C-string with trailing spaces removed.
    /// Leading spaces are preserved.
    /// Example:
    ///   trim_end("   hello   ") -> "   hello"
    inline string trim_end(const char *str, size_t len)
    {
        if (!str) return "";
        const char *end = str + len;
        while (end != str && isspace(*(end - 1))) --end;
        return string(str, end);
    }

    /// Returns a copy of the input C-string with trailing spaces removed.
    /// Leading spaces are preserved.
    /// Example:
    ///   trim_end("   hello   ") -> "   hello"
    template <typename S, typename = std::enable_if_t<is_string_like_v<char, S>>>
    inline string trim_end(const S &s)
    {
        return trim_end(s.data(), s.size());
    }

    /// Checks if the given C-string `str` starts with the specified `prefix`.
    /// Returns false if either pointer is null.
    /// Example:
    ///   starts_with("foobar", "foo") -> true
    ///   starts_with("foobar", "bar") -> false
    inline bool starts_with(const char *str, const char *prefix) noexcept
    {
        if (!str || !prefix) return false;
        const size_t n = null_terminated_length(prefix);
        if (n == 0) return true;
        return strncmp(str, prefix, n) == 0;
    }

    /// Checks if the given string `str` starts with the specified `prefix`.
    /// Uses the string length for a fast memcmp comparison.
    /// Example:
    ///   starts_with(string("hello"), "he") -> true
    ///   starts_with(string("hello"), "lo") -> false
    inline bool starts_with(const string &str, const char *prefix) noexcept
    {
        if (!prefix) return false;
        const size_t n = null_terminated_length(prefix);
        if (str.size() < n) return false;
        return memcmp(str.data(), prefix, n) == 0;
    }

    /// Checks if the given C-string `str` of known length `len` ends with the specified `suffix`.
    /// Returns false if either pointer is null, or if the suffix is longer than the string.
    /// Example:
    ///   ends_with("foobar", "bar", 6) -> true
    ///   ends_with("foobar", "foo", 6) -> false
    inline bool ends_with(const char *str, const char *suffix, size_t len) noexcept
    {
        if (!str || !suffix) return false;
        const size_t lq = null_terminated_length(suffix);
        if (lq == 0) return true;
        if (lq > len) return false;
        return memcmp(str + (len - lq), suffix, lq) == 0;
    }

    /// Checks if the given string `str` ends with the specified `suffix`.
    /// Uses the string length directly and delegates to the char* overload.
    /// Example:
    ///   ends_with(string("hello"), "lo") -> true
    ///   ends_with(string("hello"), "he") -> false
    inline bool ends_with(const string &str, const char *suffix) noexcept
    {
        return ends_with(str.c_str(), suffix, str.size());
    }

    /// Splits the given string `str` into substrings using the specified delimiter `delim`.
    /// Each substring between occurrences of `delim` is added to the result vector.
    /// The delimiter characters themselves are not included in the output.
    /// Example:
    ///   split(string("a,b,c"), ',') -> {"a", "b", "c"}
    ///   split(string("one:two:three"), ':') -> {"one", "two", "three"}
    inline vector<string> split(const string &str, char delim)
    {
        vector<string> result;
        size_t pos = 0;
        while (true)
        {
            size_t found = str.find(delim, pos);
            if (found == string::npos)
            {
                if (pos != str.size()) result.push_back(str.substr(pos));
                break;
            }
            result.push_back(str.substr(pos, found - pos));
            pos = found + 1;
        }
        return result;
    }

    inline string to_lower(const string &s)
    {
        string out;
        out.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i) { out[i] = tolower(s[i]); }
        return out;
    }

    inline string to_upper(const string &s)
    {
        string out;
        out.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i) { out[i] = toupper(s[i]); }
        return out;
    }
} // namespace acul