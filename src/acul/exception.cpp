#include <acul/exception.hpp>
#include <acul/hash/utils.hpp>
#include <acul/io/path.hpp>
#include <acul/string/utils.hpp>

namespace acul
{
    runtime_error::runtime_error(const string &message) noexcept : exception(), _message(message.c_str()) {}
    bad_cast::bad_cast(const string &message) noexcept : exception(), _message(message.c_str()) {}

    bad_alloc::bad_alloc(size_t size) noexcept
    {
        string temp = format("bad alloc: failed to allocate %zu bytes", size);
        _message = temp.c_str();
    }

    out_of_range::out_of_range(size_t max_range, size_t attempt) noexcept : exception()
    {
        string temp = format("out of range: %zu >= %zu", attempt, max_range);
        _message = temp.c_str();
    }
} // namespace acul