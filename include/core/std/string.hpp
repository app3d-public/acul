#ifndef APP_CORE_STD_STRING_H
#define APP_CORE_STD_STRING_H

#include <core/api.hpp>
#include <glm/glm.hpp>
#include <string>

/// @brief Convert string in UTF-8 encoding to string in UTF-16 encoding
/// @param src String in UTF-8 encoding
APPLIB_API std::u16string convertUTF8toUTF16(const std::string &src);

/// @brief Convert string in UTF-16 encoding to string in UTF-8 encoding
/// @param src String in UTF-16 encoding
APPLIB_API std::string convertUTF16toUTF8(const std::u16string &src);

APPLIB_API std::u16string trim(const std::u16string &inputStr, size_t max = std::numeric_limits<size_t>::max());

APPLIB_API std::string trim(const std::string &inputStr, size_t max = std::numeric_limits<size_t>::max());

/**
 * @brief Formats a string using a format string and arguments.
 * @param format The format string.
 * @param args The arguments to format the string.
 * @return The formatted string.
 */
APPLIB_API std::string f(const char *format, ...) noexcept;

APPLIB_API std::string f(const char* format, va_list args) noexcept;

/**
 * @brief Converts the integer to the C-style string
 * @param value Source value
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 */
APPLIB_API int iToStr(int value, char *buffer, size_t bufferSize);

/**
 * @brief Converts the float to the C-style string
 * @param value Source value
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 **/
APPLIB_API int fToStr(float value, char *buffer, size_t bufferSize, int precision);

/**
 * @brief Converts the 2-dimensional vector to the C-style string
 * @param vec Source vector
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 **/
APPLIB_API int vec2ToStr(const glm::vec2 &vec, char *buffer, size_t bufferSize, size_t offset);

/**
 * @brief Converts the 3-dimensional vector to the C-style string
 * @param vec Source vector
 * @param buffer Destination buffer
 * @param bufferSize Buffer size
 * @return The number of characters written
 **/
APPLIB_API int vec3ToStr(const glm::vec3 &vec, char *buffer, size_t bufferSize, size_t offset);

/**
 * @brief Deserialize the C-style string to the integer
 * @param str Source string
 * @param value Destination value
 * @return True if successful. Otherwise false
 **/
APPLIB_API bool strToI(const char *&str, int &value);

/**
 * @brief Deserialize the C-style string to the float
 * @param str Source string
 * @param value Destination value
 * @return True if successful. Otherwise false
 **/
APPLIB_API bool strToF(const char *&str, float &value);

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
    if (!strToF(str, vec.x)) return;
    if (!strToF(str, vec.y)) return;
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
    if (!strToF(str, vec.x)) return;
    if (!strToF(str, vec.y)) return;
    if (!strToF(str, vec.z)) return;
}

/**
 * @brief Extracts a substring from the input string, from the first space character to the last one.
 */
APPLIB_API std::string getStrRange(const char *&str);

APPLIB_API std::string trimEnd(const std::string &str);

#endif