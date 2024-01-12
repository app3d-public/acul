#include <core/std/stream.hpp>
#include <cstdio>
#include <cstring>
#include <stdexcept>

BinStream &BinStream::write(const char *data, size_t size)
{
    if (data == nullptr)
        throw std::invalid_argument("Null pointer passed to write");

    _data.insert(_data.end(), data, data + size);
    return *this;
}

BinStream &BinStream::read(char *data, size_t size)
{
    if (data == nullptr)
        throw std::invalid_argument("Null pointer passed to read");

    if (_pos + size > _data.size())
        throw std::runtime_error("Error reading from stream");

    std::memcpy(data, &_data[_pos], size);
    _pos += size;

    return *this;
}

void BinStream::pos(size_t pos)
{
    if (pos > _data.size())
        throw std::out_of_range("Invalid position");
    _pos = pos;
}