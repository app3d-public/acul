#ifndef APP_CORE_STD_STRING_H
#define APP_CORE_STD_STRING_H

#include <acul/api.hpp>
#include <filesystem>
#ifdef CORE_GLM_ENABLE
    #include <glm/glm.hpp>
#endif
#include <string>

namespace acul
{
    /// @brief Convert string in UTF-8 encoding to string in UTF-16 encoding
    /// @param src String in UTF-8 encoding
    APPLIB_API std::u16string utf8_to_utf16(const std::string &src);

    /// @brief Convert string in UTF-16 encoding to string in UTF-8 encoding
    /// @param src String in UTF-16 encoding
    APPLIB_API std::string utf16_to_utf8(const std::u16string &src);

#ifdef _WIN32
    APPLIB_API std::filesystem::path make_path(const std::string &path);
#else
    inline std::filesystem::path make_path(const std::string &path) { return std::filesystem::path(path); }
#endif

    APPLIB_API std::u16string trim(const std::u16string &inputStr, size_t max = std::numeric_limits<size_t>::max());

    APPLIB_API std::string trim(const std::string &inputStr, size_t max = std::numeric_limits<size_t>::max());

    /**
     * @brief Formats a string using a format string and arguments.
     * @param format The format string.
     * @param args The arguments to format the string.
     * @return The formatted string.
     */
    APPLIB_API std::string format(const char *format, ...) noexcept;

    std::string format_va_list(const char *format, va_list args) noexcept;

    /**
     * @brief Converts the integer to the C-style string
     * @param value Source value
     * @param buffer Destination buffer
     * @param bufferSize Buffer size
     * @return The number of characters written
     */
    APPLIB_API int to_string(int value, char *buffer, size_t buffer_size);

    /**
     * @brief Converts the float to the C-style string
     * @param value Source value
     * @param buffer Destination buffer
     * @param bufferSize Buffer size
     * @return The number of characters written
     **/
    APPLIB_API int to_string(float value, char *buffer, size_t buffer_size, int precision);

    /**
     * @brief Deserialize the C-style string to the integer
     * @param str Source string
     * @param value Destination value
     * @return True if successful. Otherwise false
     **/
    APPLIB_API bool stoi(const char *&str, int &value);

    /**
     * @brief Deserialize the C-style string to the float
     * @param str Source string
     * @param value Destination value
     * @return True if successful. Otherwise false
     **/
    APPLIB_API bool stof(const char *&str, float &value);

#ifdef CORE_GLM_ENABLE

    /**
     * @brief Converts the 2-dimensional vector to the C-style string
     * @param vec Source vector
     * @param buffer Destination buffer
     * @param bufferSize Buffer size
     * @return The number of characters written
     **/
    APPLIB_API int to_string(const glm::vec2 &vec, char *buffer, size_t buffer_size, size_t offset);

    /**
     * @brief Converts the 3-dimensional vector to the C-style string
     * @param vec Source vector
     * @param buffer Destination buffer
     * @param bufferSize Buffer size
     * @return The number of characters written
     **/
    APPLIB_API int to_string(const glm::vec3 &vec, char *buffer, size_t buffer_size, size_t offset);

    /**
     * @brief Deserialize the C-style string to the 2-dimensional vector.
     * All values to deserialize must be present in the string.
     * @param str Source string
     * @param vec Destination vector
     * @return True if successful. Otherwise false
     **/
    inline bool stov2(const char *&str, glm::vec2 &vec) { return stof(str, vec.x) && stof(str, vec.y); }

    /**
     * @brief Deserialize the C-style string to the 2-dimensional vector.
     * This function deserializes line, attempting to fill as many components as possible based on the input.
     * @param str Source string
     * @param vec Destination vector
     * @return True if successful. Otherwise false
     **/
    inline void stov2_opt(const char *&str, glm::vec2 &vec)
    {
        if (!stof(str, vec.x)) return;
        if (!stof(str, vec.y)) return;
    }

    /**
     * @brief Deserialize the C-style string to the 3-dimensional vector.
     * All values to deserialize must be present in the string.
     * @param str Source string
     * @param vec Destination vector
     * @return True if successful. Otherwise false
     **/
    inline bool stov3(const char *&str, glm::vec3 &vec)
    {
        return stof(str, vec.x) && stof(str, vec.y) && stof(str, vec.z);
    }

    /**
     * @brief Deserialize the C-style string to the 3-dimensional vector.
     * This function deserializes line, attempting to fill as many components as possible based on the input.
     * @param str Source string
     * @param vec Destination vector
     * @return True if successful. Otherwise false
     **/
    inline void stov3_opt(const char *&str, glm::vec3 &vec)
    {
        if (!stof(str, vec.x)) return;
        if (!stof(str, vec.y)) return;
        if (!stof(str, vec.z)) return;
    }

#endif

    /**
     * @brief Extracts a substring from the input string, from the first space character to the last one.
     */
    APPLIB_API std::string str_range(const char *&str);

    APPLIB_API std::string trim_end(const char *str);
} // namespace acul
#endif