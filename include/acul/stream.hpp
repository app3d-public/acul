#ifndef APP_ACUL_STD_STREAM_H
#define APP_ACUL_STD_STREAM_H

#include "api.hpp"
#include "list.hpp"
#include "string/string.hpp"
#include "vector.hpp"
#ifdef ACUL_GLM_ENABLE
    #include "math/types.hpp"
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
        using value_type = char;
        using pointer = char *;
        using const_pointer = const char *;
        using reference = char &;
        using const_reference = const char &;
        using size_type = size_t;
        using iterator = vector<char>::iterator;
        using const_iterator = vector<char>::const_iterator;

        /**
         * @brief Default constructor. Initializes the stream position to 0.
         */
        bin_stream() : _pos(0) {}

        /**
         * @brief Constructor that initializes the stream with provided data.
         *
         * @param data Binary data to initialize the stream with.
         */
        explicit bin_stream(vector<value_type> &&data) : _data(std::move(data)), _pos(0) {}

        void reserve(size_type n) { _data.reserve(n); }

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
            static_assert(std::is_standard_layout<T>::value);
            const_pointer byte_data = reinterpret_cast<const_pointer>(&val);
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

        bin_stream &write_pad(size_type n)
        {
            _data.insert(_data.end(), n, 0);
            return *this;
        }

        bin_stream &write_align(size_type a)
        {
            size_type pad = round_up(_data.size(), a) - _data.size();
            if (pad) write_pad(pad);
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
        bin_stream &write(const string &str)
        {
            _data.insert(_data.end(), str.begin(), str.end());
            _data.push_back('\0');
            return *this;
        }
#ifdef ACUL_MATH_TYPES

        bin_stream &write(const vec2 &vec) { return write(vec.x).write(vec.y); }

        bin_stream &write(const vec3 &vec) { return write(vec.x).write(vec.y).write(vec.z); }

        bin_stream &write(const mat4 &mat)
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
            size_type count;
            read(count);
            for (size_type i = 0; i < count; ++i)
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
        bin_stream &read(string &str)
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
#ifdef ACUL_MATH_TYPES
        bin_stream &read(vec2 &vec) { return read(vec.x).read(vec.y); }
        bin_stream &read(vec3 &vec) { return read(vec.x).read(vec.y).read(vec.z); }

        bin_stream &read(mat4 &mat)
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
        bin_stream &write(T *data, size_type size)
        {
            if (data == nullptr) throw runtime_error("Null pointer passed to write");

            _data.insert(_data.end(), reinterpret_cast<const_pointer>(data),
                         reinterpret_cast<const_pointer>(data) + size * sizeof(T));
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
        bin_stream &read(T *data, size_type size)
        {
            if (data == nullptr) throw runtime_error("Null pointer passed to read");

            size_type byte_size = size * sizeof(T);
            if (_pos + byte_size > _data.size()) throw runtime_error("Error reading from stream");

            memcpy(data, &_data[_pos], byte_size);
            _pos += byte_size;

            return *this;
        }

        /**
         * @brief Retrieves a constant pointer to the stream's data.
         * @return Constant pointer to the data.
         */
        const_pointer data() const { return _data.data(); }

        /**
         * @brief Retrieves a pointer to the stream's data.
         * @return Pointer to the data.
         */
        pointer data() { return _data.data(); }

        /**
         * @brief Gets the current position in the stream.
         * @return Current position.
         */
        size_type pos() const { return _pos; }

        /**
         * @brief Sets the current position in the stream.
         * @param pos Desired position.
         */
        void pos(size_type pos)
        {
            if (pos > _data.size()) throw out_of_range(_data.size(), pos);
            _pos = pos;
        }

        /**
         * @brief Gets the size of the data in the stream.
         * @return Size of the data.
         */
        size_type size() const { return _data.size(); }

        /**
         * @brief Shifts the stream position by the specified amount.
         * @param amount Amount to shift the position by.
         */
        void shift(size_type amount) { _pos += amount; }

        iterator begin() { return _data.begin(); }
        const_iterator begin() const { return _data.begin(); }
        const_iterator cbegin() const { return _data.begin(); }

        iterator end() { return _data.end(); }
        const_iterator end() const { return _data.end(); }
        const_iterator cend() const { return _data.end(); }

    private:
        vector<char> _data;
        size_type _pos;
    };
} // namespace acul

#endif