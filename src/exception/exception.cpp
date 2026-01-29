#include <acul/exception/exception.hpp>
#include <acul/hash/utils.hpp>
#include <acul/string/utils.hpp>
#ifndef _MSC_VER
    #include <cxxabi.h>
#endif

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

#ifndef _MSC_VER
    string demangle(const char *mangled_name)
    {
        int status = 0;
        char *demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
        if (status == 0 && demangled)
        {
            string result(demangled);
            free(demangled);
            return result;
        }
        return mangled_name;
    }
#endif
} // namespace acul