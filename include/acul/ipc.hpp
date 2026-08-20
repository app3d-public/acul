#pragma once

#include <acul/string/string.hpp>
#include <cstddef>
#include "scalars.hpp"

#ifndef _WIN32
    #include <signal.h>
    #include <ucontext.h>
#endif

#define ACUL_CRASH_NOTIFY_MAGIC 0x48535243u

namespace acul
{
    struct crash_notify
    {
        u32 magic = ACUL_CRASH_NOTIFY_MAGIC;
        u32 code = 0;
        u32 pid = 0;
        u32 tid = 0;
        u64 addr = 0;
    };

    static_assert(sizeof(crash_notify) == 24, "crash_notify ABI size mismatch");
    static_assert(offsetof(crash_notify, addr) == 16, "crash_notify ABI layout mismatch");

    struct pipe
    {
#ifdef _WIN32
        HANDLE handle = INVALID_HANDLE_VALUE;
        bool connected = false;
        string name;
#else
        int fd = -1;
        string path;
#endif
    };

    ACUL_EXPORT bool create_pipe(pipe &dst, const char *name, const char *env, size_t size);
    ACUL_EXPORT void close_pipe(pipe &src, const char *env);

    // Local request/response IPC. On Windows endpoint is a named-pipe path;
    // on Unix it is a Unix-domain socket path.
    struct ipc_listener
    {
#ifdef _WIN32
        string endpoint;
        size_t buffer_size = 0;
#else
        int fd = -1;
        string endpoint;
#endif
    };

    struct ipc_connection
    {
#ifdef _WIN32
        HANDLE handle = INVALID_HANDLE_VALUE;
#else
        int fd = -1;
#endif
    };

    ACUL_EXPORT bool listen_ipc(ipc_listener &listener, const string &endpoint, size_t buffer_size);
    ACUL_EXPORT bool accept_ipc(ipc_listener &listener, ipc_connection &connection, u32 timeout_ms);
    ACUL_EXPORT bool connect_ipc(ipc_connection &connection, const string &endpoint, u32 timeout_ms);
    ACUL_EXPORT bool read_ipc(ipc_connection &connection, void *data, size_t size, u32 timeout_ms);
    ACUL_EXPORT bool write_ipc(ipc_connection &connection, const void *data, size_t size, u32 timeout_ms);
    ACUL_EXPORT bool transact_ipc(const string &endpoint, const void *request, size_t request_size, void *response,
                                  size_t response_size, u32 timeout_ms);
    ACUL_EXPORT void close_ipc(ipc_connection &connection) noexcept;
    ACUL_EXPORT void close_ipc(ipc_listener &listener) noexcept;
} // namespace acul
