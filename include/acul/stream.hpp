#ifndef APP_ACUL_STD_STREAM_H
#define APP_ACUL_STD_STREAM_H

#include "api.hpp"
#include "list.hpp"
#include "string/string.hpp"
#include "vector.hpp"
#ifdef ACUL_GLM_ENABLE
    #include <glm/glm.hpp>
#endif

namespace acul
{
    /**
     * @brief Utility class for binary stream manipulation.
     *
     * The `bin_stream` class provides a way to read and write binary data to and from a stream.
     * The stream is based by a `Array<char>`, enabling dynamic resizing and efficient
     * random access. It's suitable for serializing and deserializing complex data structures
     * in binary format.
     */
    class APPLIB_API bin_stream
    {
    public:
        /**
         * @brief Default constructor. Initializes the stream position to 0.
         */
        bin_stream() : _pos(0) {}

        /**
         * @brief Constructor that initializes the stream with provided data.
         *
         * @param data Binary data to initialize the stream with.
         */
        explicit bin_stream(vector<char> &&data) : _data(std::move(data)), _pos(0) {}

        /**
         * @brief Writes a value of type T to the stream.
         *
         * @tparam T Type of the value to write.
         * @param val Value to write.
         * @return Reference to the stream to support chained calls.
         */
        template <typename T>
        bin_stream &write(const T &val)
        {
            const char *byte_data = reinterpret_cast<const char *>(&val);
            _data.insert(_data.end(), byte_data, byte_data + sizeof(val));
            return *this;
        }

        template <typename T>
        bin_stream &write(const list<T> &list)
        {
            write(list.size());
            for (const auto &item : list) write(item);
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
        bin_stream &write(const acul::string &str)
        {
            _data.insert(_data.end(), str.begin(), str.end());
            _data.push_back('\0');
            return *this;
        }
#ifdef ACUL_GLM_ENABLE

        bin_stream &write(const glm::vec2 &vec) { return write(vec.x).write(vec.y); }

        bin_stream &write(const glm::vec3 &vec) { return write(vec.x).write(vec.y).write(vec.z); }

        bin_stream &write(const glm::mat4 &mat)
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
#endif

        /**
         * @brief Reads a value of type T from the stream.
         *
         * @tparam T Type of the value to read.
         * @param val Reference where the read value will be stored.
         * @return Reference to the stream to support chained calls.
         * @throws runtime_error If there's not enough data in the stream.
         */
        template <typename T>
        bin_stream &read(T &val)
        {
            if (_pos + sizeof(T) <= _data.size())
            {
                val = *reinterpret_cast<const T *>(&_data[_pos]);
                _pos += sizeof(T);
            }
            else
                throw runtime_error("Error reading from stream");
            return *this;
        }

        template <typename T>
        bin_stream &read(list<T> &list)
        {
            size_t count;
            read(count);
            for (size_t i = 0; i < count; ++i)
            {
                T item;
                read(item);
                list.push_back(item);
            }
            return *this;
        }

        /**
         * @brief Reads a string from the stream.
         *
         * @param str Reference where the read string will be stored.
         * @return Reference to the stream to support chained calls.
         */
        bin_stream &read(acul::string &str)
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
#ifdef ACUL_GLM_ENABLE

        bin_stream &read(glm::vec2 &vec) { return read(vec.x).read(vec.y); }
        bin_stream &read(glm::vec3 &vec) { return read(vec.x).read(vec.y).read(vec.z); }

        bin_stream &read(glm::mat4 &mat)
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
#endif

        /**
         * @brief Writes raw data to the stream.
         *
         * @param data Pointer to the raw data.
         * @param size Size of the data in bytes.
         * @return Reference to the stream to support chained calls.
         */
        template <typename T>
        bin_stream &write(T *data, size_t size)
        {
            if (data == nullptr) throw runtime_error("Null pointer passed to write");

            _data.insert(_data.end(), reinterpret_cast<const char *>(data),
                         reinterpret_cast<const char *>(data) + size * sizeof(T));
            return *this;
        }

        /**
         * @brief Reads raw data from the stream.
         *
         * @param data Pointer where the read data will be stored.
         * @param size Size of the data in bytes.
         * @return Reference to the stream to support chained calls.
         */
        template <typename T>
        bin_stream &read(T *data, size_t size)
        {
            if (data == nullptr) throw runtime_error("Null pointer passed to read");

            size_t byteSize = size * sizeof(T);
            if (_pos + byteSize > _data.size()) throw runtime_error("Error reading from stream");

            memcpy(data, &_data[_pos], byteSize);
            _pos += byteSize;

            return *this;
        }

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
        void pos(size_t pos)
        {
            if (pos > _data.size()) throw out_of_range(_data.size(), pos);
            _pos = pos;
        }

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
        vector<char> _data;
        size_t _pos;
    };
} // namespace acul

#endif