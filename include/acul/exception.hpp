#pragma once

#include <fstream>
#include "api.hpp"
#include "fwd/string.hpp"
#include "scalars.hpp"
#include "string/refstring.hpp"

#ifdef _WIN32
    #include <windows.h>
    // Windows first
    #include <dbghelp.h>
#endif

#define EXCEPTION_MESSAGE_SIZE 256

namespace acul
{
#ifdef _WIN32
    struct except_addr_t
    {
        DWORD64 addr;
        DWORD64 offset;
    };

    struct except_info_t
    {
        HANDLE hProcess = NULL;
        HANDLE hThread = NULL;
        CONTEXT context;
        except_addr_t *addresses = NULL;
        size_t addresses_count = 0;
    };

    APPLIB_API void write_frame_registers(std::ofstream &stream, const CONTEXT &context);

    APPLIB_API void write_exception_info(_EXCEPTION_RECORD *, std::ofstream &);

    APPLIB_API void write_stack_trace(std::ofstream &stream, const except_info_t &except_info);

    APPLIB_API void create_mini_dump(EXCEPTION_POINTERS *pep, const string &path);
#endif

    class APPLIB_API exception : public std::exception
    {
    public:
        APPLIB_API static string dump_folder;
        except_info_t except_info;
        u64 id;

        exception() noexcept;
        exception(const exception &) noexcept = delete;
        exception &operator=(const exception &) noexcept = delete;

        virtual ~exception() noexcept;
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