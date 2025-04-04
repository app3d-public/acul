#pragma once

#include <fstream>
#include "api.hpp"
#include "fwd/sstream.hpp"
#include "fwd/string.hpp"
#include "string/refstring.hpp"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace acul
{
#ifdef _WIN32
    extern APPLIB_API struct exception_context
    {
        HANDLE hProcess;
    } exception_context;

    struct except_addr
    {
        DWORD64 addr;
        DWORD64 offset;
    };

    struct except_info
    {
        HANDLE hThread = NULL;
        CONTEXT context;
        except_addr *addresses = NULL;
        size_t addresses_count = 0;
    };

    APPLIB_API void init_exception_context();
    APPLIB_API void destroy_exception_context();

    APPLIB_API void write_frame_registers(acul::stringstream &stream, const CONTEXT &context);

    APPLIB_API void write_exception_info(_EXCEPTION_RECORD *, std::ofstream &);

    APPLIB_API void write_stack_trace(acul::stringstream &stream, const except_info &except_info);

    APPLIB_API void create_mini_dump(EXCEPTION_POINTERS *pep, const string &path);
#endif

    class APPLIB_API exception : public std::exception
    {
    public:
        except_info except_info;

        exception() noexcept : except_info(GetCurrentThread())
        {
            RtlCaptureContext(&except_info.context);
            captureStack();
        }
        exception(const exception &) noexcept = delete;
        exception &operator=(const exception &) noexcept = delete;

        virtual ~exception() noexcept { release(except_info.addresses); }

        virtual const char *what() const noexcept = 0;

    private:
        void captureStack();
    };

    class APPLIB_API runtime_error final : public exception
    {
    public:
        runtime_error(const string &message) noexcept;
        runtime_error(const char *message) noexcept : exception(), _message(message) {}

        const char *what() const noexcept override { return _message.c_str(); }

        ~runtime_error() noexcept override = default;

    private:
        refstring _message;
    };

    class APPLIB_API bad_alloc final : public exception
    {
    public:
        bad_alloc(size_t size) noexcept;

        const char *what() const noexcept override { return _message.c_str(); }

        ~bad_alloc() noexcept override = default;

    private:
        refstring _message;
    };

    class APPLIB_API bad_cast final : public exception
    {
    public:
        bad_cast(const string &message) noexcept;
        bad_cast(const char *message) noexcept : exception(), _message(message) {}

        const char *what() const noexcept override { return _message.c_str(); }

        ~bad_cast() noexcept override = default;

    private:
        refstring _message;
    };

    class APPLIB_API out_of_range final : public exception
    {
    public:
        out_of_range(size_t max_range, size_t attempt) noexcept;

        const char *what() const noexcept override { return _message.c_str(); }

        ~out_of_range() noexcept override = default;

    private:
        refstring _message;
    };
} // namespace acul