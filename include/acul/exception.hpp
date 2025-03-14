#pragma once

#include <fstream>
#include "api.hpp"
#include "hash.hpp"
#include "scalars.hpp"
#include "string.hpp"
#include "vector.hpp"

#ifdef _WIN32
    #include <windows.h>
    // Windows first
    #include <dbghelp.h>

namespace acul
{
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
        acul::vector<except_addr_t> addresses;
    };

    APPLIB_API void writeFrameRegisters(std::ofstream &stream, const CONTEXT &context);

    APPLIB_API void writeExceptionInfo(_EXCEPTION_RECORD *, std::ofstream &);

    APPLIB_API void writeStackTrace(std::ofstream &stream, const except_info_t &except_info);

    APPLIB_API void createMiniDump(EXCEPTION_POINTERS *pep, const std::filesystem::path &path);

    class APPLIB_API exception
    {
    public:
        APPLIB_API static std::filesystem::path dump_folder;
        except_info_t except_info;
        u64 id;

        exception() noexcept : except_info({GetCurrentProcess(), GetCurrentThread()}), id(IDGen()())
        {
            if (!dump_folder.empty()) createMiniDump(nullptr, dump_folder / format("%llx.dmp", id));
            RtlCaptureContext(&except_info.context);
            captureStack();
        }
        exception(const exception &) noexcept = delete;
        exception &operator=(const exception &) noexcept = delete;

        virtual ~exception() noexcept = default;
        virtual const char *what() const noexcept = 0;

    private:
        void captureStack();
    };

    class APPLIB_API runtime_error final : public exception
    {
    public:
        runtime_error(const char *message) noexcept : exception(), _message(message) {}

        const char *what() const noexcept override { return _message; }

        ~runtime_error() noexcept override = default;

    private:
        const char *_message;
    };

    class APPLIB_API bad_alloc final : public exception
    {
    public:
        bad_alloc(size_t size) noexcept : exception()
        {
            _message = format("bad_alloc: failed to allocate %zu bytes", size);
        }

        const char *what() const noexcept override { return _message.c_str(); }

        ~bad_alloc() noexcept override = default;

    private:
        std::string _message;
    };

#else
    #error "Platform not supported"
#endif
}