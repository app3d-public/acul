#ifndef APP_CORE_STD_STRING_H
#define APP_CORE_STD_STRING_H

#include <glm/glm.hpp>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
    /**
     * printf like formatting for C++ with std::string
     * Original source: https://stackoverflow.com/a/26221725/11722
     */
    template <typename... Args>
    std::string formatInternal(const std::string &format, Args &&...args)
    {
        const auto size = snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...) + 1;
        if (size <= 0)
            throw std::runtime_error("Error during formatting.");

        std::unique_ptr<char[]> buf(new char[size]);
        snprintf(buf.get(), size, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1);
    }
} // namespace

/// @brief Convert string in UTF-8 encoding to string in UTF-16 encoding
/// @param src String in UTF-8 encoding
std::u16string convertUTF8toUTF16(const std::string &src);

/// @brief Convert string in UTF-16 encoding to string in UTF-8 encoding
/// @param src String in UTF-16 encoding
std::string convertUTF16toUTF8(const std::u16string &src);

std::u16string trim(const std::u16string &inputStr, size_t max = std::numeric_limits<size_t>::max());

std::string trim(const std::string &inputStr, size_t max = std::numeric_limits<size_t>::max());

/**
 * Convert all std::strings to const char* using constexpr if (C++17)
 */
template <typename T>
auto convert(T &&t)
{
    if constexpr (std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::string>::value)
        return std::forward<T>(t).c_str();
    else
        return std::forward<T>(t);
}

/**
 * @brief Formats a string using a format string and arguments.
 * @tparam Args The types of the arguments.
 * @param fmt The format string.
 * @param args The arguments to format the string.
 * @return The formatted string.
 */
template <typename... Args>
std::string f(const std::string& fmt, Args &&...args)
{
    return formatInternal(fmt, convert(std::forward<Args>(args))...);
}

/**
 * @brief Converts the integer to the C-style string
 * @param value Source value
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 */
int iToStr(int value, char *buffer, size_t bufferSize);

/**
 * @brief Converts the float to the C-style string
 * @param value Source value
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 **/
int fToStr(float value, char *buffer, size_t bufferSize, int precision);

/**
 * @brief Converts the 2-dimensional vector to the C-style string
 * @param vec Source vector
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 **/
int vec2ToStr(const glm::vec2 &vec, char *buffer, size_t bufferSize, size_t offset);

/**
 * @brief Converts the 3-dimensional vector to the C-style string
 * @param vec Source vector
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 **/
int vec3ToStr(const glm::vec3 &vec, char *buffer, size_t bufferSize, size_t offset);

/**
 * @brief Deserialize the C-style string to the integer
 * @param str Source string
 * @param value Destination value
 * @return True if successful. Otherwise false
 **/
bool strToI(const char *&str, int &value);

/**
 * @brief Deserialize the C-style string to the float
 * @param str Source string
 * @param value Destination value
 * @return True if successful. Otherwise false
 **/
bool strToF(const char *&str, float &value);

/**
 * @brief Deserialize the C-style string to the 2-dimensional vector.
 * All values to deserialize must be present in the string.
 * @param str Source string
 * @param vec Destination vector
 * @return True if successful. Otherwise false
 **/
inline bool strToV2(const char *&str, glm::vec2 &vec) { return strToF(str, vec.x) && strToF(str, vec.y); }

/**
 * @brief Deserialize the C-style string to the 2-dimensional vector.
 * This function deserializes line, attempting to fill as many components as possible based on the input.
 * @param str Source string
 * @param vec Destination vector
 * @return True if successful. Otherwise false
 **/
inline void strToV2Optional(const char *&str, glm::vec2 &vec)
{
    if (!strToF(str, vec.x))
        return;
    if (!strToF(str, vec.y))
        return;
}

/**
 * @brief Deserialize the C-style string to the 3-dimensional vector.
 * All values to deserialize must be present in the string.
 * @param str Source string
 * @param vec Destination vector
 * @return True if successful. Otherwise false
 **/
inline bool strToV3(const char *&str, glm::vec3 &vec)
{
    return strToF(str, vec.x) && strToF(str, vec.y) && strToF(str, vec.z);
}

/**
 * @brief Deserialize the C-style string to the 3-dimensional vector.
 * This function deserializes line, attempting to fill as many components as possible based on the input.
 * @param str Source string
 * @param vec Destination vector
 * @return True if successful. Otherwise false
 **/
inline void strToV3Optional(const char *&str, glm::vec3 &vec)
{
    if (!strToF(str, vec.x))
        return;
    if (!strToF(str, vec.y))
        return;
    if (!strToF(str, vec.z))
        return;
}

/**
 * @brief Extracts a substring from the input string, from the first space character to the last one.
 */
std::string getStrRange(const char *&str);

std::string trimEnd(const std::string &str);

#endif