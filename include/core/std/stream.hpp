#ifndef APP_CORE_STD_STREAM_H
#define APP_CORE_STD_STREAM_H

#include <core/api.hpp>
#include <core/std/darray.hpp>
#include <glm/glm.hpp>
#include <stdexcept>
#include <string>

/**
 * @brief Utility class for binary stream manipulation.
 *
 * The `BinStream` class provides a way to read and write binary data to and from a stream.
 * The stream is based by a `Array<char>`, enabling dynamic resizing and efficient
 * random access. It's suitable for serializing and deserializing complex data structures
 * in binary format.
 */
class APPLIB_API BinStream
{
public:
    /**
     * @brief Default constructor. Initializes the stream position to 0.
     */
    BinStream() : _pos(0) {}

    /**
     * @brief Constructor that initializes the stream with provided data.
     *
     * @param data Binary data to initialize the stream with.
     */
    explicit BinStream(DArray<char> &&data) : _data(std::move(data)), _pos(0) {}

    /**
     * @brief Writes a value of type T to the stream.
     *
     * @tparam T Type of the value to write.
     * @param val Value to write.
     * @return Reference to the stream to support chained calls.
     */
    template <typename T>
    BinStream &write(const T &val)
    {
        const char *byte_data = reinterpret_cast<const char *>(&val);
        _data.insert(_data.end(), byte_data, byte_data + sizeof(val));
        return *this;
    }

    /**
     * @brief Writes a string to the stream.
     *
     * The length of the string is written first, followed by the string data.
     *
     * @param str String to write.
     * @return Reference to the stream to support chained calls.
     */
    BinStream &write(const std::string &str)
    {
        _data.insert(_data.end(), str.begin(), str.end());
        _data.push_back('\0');
        return *this;
    }

    BinStream &write(const glm::vec2 &vec) { return write(vec.x).write(vec.y); }

    BinStream &write(const glm::vec3 &vec) { return write(vec.x).write(vec.y).write(vec.z); }

    BinStream &write(const glm::mat4 &mat)
    {
        return write(mat[0][0])
            .write(mat[0][1])
            .write(mat[0][2])
            .write(mat[0][3])
            .write(mat[1][0])
            .write(mat[1][1])
            .write(mat[1][2])
            .write(mat[1][3])
            .write(mat[2][0])
            .write(mat[2][1])
            .write(mat[2][2])
            .write(mat[2][3])
            .write(mat[3][0])
            .write(mat[3][1])
            .write(mat[3][2])
            .write(mat[3][3]);
    }

    /**
     * @brief Reads a value of type T from the stream.
     *
     * @tparam T Type of the value to read.
     * @param val Reference where the read value will be stored.
     * @return Reference to the stream to support chained calls.
     * @throws std::runtime_error If there's not enough data in the stream.
     */
    template <typename T>
    BinStream &read(T &val)
    {
        if (_pos + sizeof(T) <= _data.size())
        {
            val = *reinterpret_cast<const T *>(&_data[_pos]);
            _pos += sizeof(T);
        }
        else
            throw std::runtime_error("Error reading from stream");
        return *this;
    }

    /**
     * @brief Reads a string from the stream.
     *
     * @param str Reference where the read string will be stored.
     * @return Reference to the stream to support chained calls.
     */
    BinStream &read(std::string &str)
    {
        str.clear();
        while (_pos < _data.size())
        {
            char ch = _data[_pos++];
            if (ch == '\0') break;
            str += ch;
        }
        return *this;
    }

    BinStream &read(glm::vec2 &vec) { return read(vec.x).read(vec.y); }
    BinStream &read(glm::vec3 &vec) { return read(vec.x).read(vec.y).read(vec.z); }

    BinStream &read(glm::mat4 &mat)
    {
        return read(mat[0][0])
            .read(mat[0][1])
            .read(mat[0][2])
            .read(mat[0][3])
            .read(mat[1][0])
            .read(mat[1][1])
            .read(mat[1][2])
            .read(mat[1][3])
            .read(mat[2][0])
            .read(mat[2][1])
            .read(mat[2][2])
            .read(mat[2][3])
            .read(mat[3][0])
            .read(mat[3][1])
            .read(mat[3][2])
            .read(mat[3][3]);
    }

    /**
     * @brief Writes raw data to the stream.
     *
     * @param data Pointer to the raw data.
     * @param size Size of the data in bytes.
     * @return Reference to the stream to support chained calls.
     */
    BinStream &write(const char *data, size_t size);

    /**
     * @brief Reads raw data from the stream.
     *
     * @param data Pointer where the read data will be stored.
     * @param size Size of the data in bytes.
     * @return Reference to the stream to support chained calls.
     */
    BinStream &read(char *data, size_t size);

    /**
     * @brief Retrieves a constant pointer to the stream's data.
     * @return Constant pointer to the data.
     */
    const char *data() const { return _data.data(); }

    /**
     * @brief Retrieves a pointer to the stream's data.
     * @return Pointer to the data.
     */
    char *data() { return _data.data(); }

    /**
     * @brief Gets the current position in the stream.
     * @return Current position.
     */
    size_t pos() const { return _pos; }

    /**
     * @brief Sets the current position in the stream.
     * @param pos Desired position.
     */
    void pos(size_t pos);

    /**
     * @brief Gets the size of the data in the stream.
     * @return Size of the data.
     */
    size_t size() const { return _data.size(); }

    /**
     * @brief Shifts the stream position by the specified amount.
     * @param amount Amount to shift the position by.
     */
    void shift(size_t amount) { _pos += amount; }

private:
    DArray<char> _data;
    size_t _pos;
};

#endif