#include <acul/ipc.hpp>
#include <cstdio>

namespace acul
{
    bool create_pipe(pipe &dst, const char *name, const char *env, size_t size)
    {
        char full_name[128];
        snprintf(full_name, sizeof(full_name), "\\\\.\\pipe\\%s-%lu", name,
                 static_cast<unsigned long>(GetCurrentProcessId()));
        dst.name = (const char *)full_name;
        dst.handle = CreateNamedPipeA(dst.name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, static_cast<DWORD>(size),
                                      static_cast<DWORD>(size), 0, nullptr);
        if (dst.handle == INVALID_HANDLE_VALUE) return false;
        return SetEnvironmentVariableA(env, dst.name.c_str()) == TRUE;
    }

    void close_pipe(pipe &src, const char *env)
    {
        SetEnvironmentVariableA(env, nullptr);
        if (src.handle != INVALID_HANDLE_VALUE) CloseHandle(src.handle);
        src.handle = INVALID_HANDLE_VALUE;
    }
} // namespace acul
