#include <acul/exception.hpp>
#include <acul/hash/utils.hpp>
#include <acul/io/path.hpp>
#include <acul/string/utils.hpp>

namespace acul
{
    string exception::dump_folder;

    exception::exception() noexcept : except_info({GetCurrentProcess(), GetCurrentThread()}), id(id_gen()())
    {
        if (!dump_folder.empty())
        {
            string relative = format("%llx.dmp", id);
            io::path p = dump_folder;
            create_mini_dump(nullptr, p / relative);
        }
        RtlCaptureContext(&except_info.context);
        captureStack();
    }

    exception::~exception() noexcept { release(except_info.addresses); }

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