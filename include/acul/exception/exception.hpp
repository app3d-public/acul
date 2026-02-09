#pragma once

#include "../api.hpp"
#include "../fwd/string.hpp"
#include "../string/refstring.hpp"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
    #include <execinfo.h>
    #include <ucontext.h>
#endif

namespace acul
{
#ifdef _WIN32
    struct except_addr
    {
        DWORD64 addr;
        DWORD64 offset;
    };

    struct except_info
    {
        HANDLE hProcess;
        HANDLE hThread = NULL;
        CONTEXT context;
        except_addr *addresses = NULL;
        size_t addresses_count = 0;
    };

    extern HANDLE exception_hprocess;
    APPLIB_API HANDLE get_exception_process();
    void destroy_exception_context(HANDLE hProcess = NULL);
#else
    using except_addr = void *;
    struct except_info
    {
        pid_t pid;
        ucontext_t context;
        except_addr *addresses = NULL;
        size_t addresses_count = 0;
    };
#endif

    APPLIB_API void capture_stack_trace(except_info &except_info);

    class APPLIB_API exception : public std::exception
    {
    public:
        struct except_info except_info;

#ifdef _WIN32
        exception() noexcept : except_info{get_exception_process(), GetCurrentThread()}
        {
    #ifndef PROCESS_UNITTEST
            RtlCaptureContext(&except_info.context);
            capture_stack_trace(except_info);
    #endif
        }
#else
        exception() noexcept : except_info(0)
        {
    #ifndef PROCESS_UNITTEST
            if (getcontext(&except_info.context) == 0) capture_stack_trace(except_info);
    #endif
        }
#endif
        exception(const exception &) noexcept = delete;
        exception &operator=(const exception &) noexcept = delete;

        virtual ~exception() noexcept { release(except_info.addresses); }

        virtual const char *what() const noexcept = 0;
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