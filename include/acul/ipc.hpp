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

    static_assert(sizeof(crash_notify) == 24, "CrashNotify ABI size mismatch");
    static_assert(offsetof(crash_notify, addr) == 16, "CrashNotify ABI layout mismatch");

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

    APPLIB_API bool create_pipe(pipe &dst, const char *name, const char *env, size_t size);

    APPLIB_API void close_pipe(pipe& src, const char* env);
} // namespace acul
