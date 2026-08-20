#include <acul/ipc.hpp>
#include <algorithm>
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

    namespace
    {
        bool overlapped_io(HANDLE handle, bool write, void *data, size_t size, u32 timeout_ms)
        {
            char *cursor = static_cast<char *>(data);
            size_t remaining = size;

            while (remaining)
            {
                OVERLAPPED overlapped{};
                overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
                if (!overlapped.hEvent) return false;

                const DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, MAXDWORD));
                DWORD transferred = 0;
                BOOL started = write ? WriteFile(handle, cursor, chunk, nullptr, &overlapped)
                                     : ReadFile(handle, cursor, chunk, nullptr, &overlapped);
                bool ok = started != FALSE;
                if (ok)
                    ok = GetOverlappedResult(handle, &overlapped, &transferred, TRUE) != FALSE;
                else if (GetLastError() == ERROR_IO_PENDING)
                {
                    if (WaitForSingleObject(overlapped.hEvent, timeout_ms) == WAIT_OBJECT_0)
                        ok = GetOverlappedResult(handle, &overlapped, &transferred, FALSE) != FALSE;
                    else
                    {
                        CancelIoEx(handle, &overlapped);
                        GetOverlappedResult(handle, &overlapped, &transferred, TRUE);
                    }
                }

                CloseHandle(overlapped.hEvent);
                if (!ok || transferred == 0) return false;
                cursor += transferred;
                remaining -= transferred;
            }
            return true;
        }
    } // namespace

    bool listen_ipc(ipc_listener &listener, const string &endpoint, size_t buffer_size)
    {
        if (endpoint.empty() || buffer_size == 0) return false;
        listener.endpoint = endpoint;
        listener.buffer_size = buffer_size;
        return true;
    }

    bool accept_ipc(ipc_listener &listener, ipc_connection &connection, u32 timeout_ms)
    {
        close_ipc(connection);
        if (listener.endpoint.empty() || listener.buffer_size == 0) return false;

        const DWORD buffer_size = static_cast<DWORD>(std::min<size_t>(listener.buffer_size, MAXDWORD));
        HANDLE handle = CreateNamedPipeA(listener.endpoint.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
                                         buffer_size, buffer_size, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;

        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent)
        {
            CloseHandle(handle);
            return false;
        }

        BOOL connected = ConnectNamedPipe(handle, &overlapped);
        bool ok = connected != FALSE;
        if (!ok)
        {
            const DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED)
                ok = true;
            else if (error == ERROR_IO_PENDING && WaitForSingleObject(overlapped.hEvent, timeout_ms) == WAIT_OBJECT_0)
            {
                DWORD transferred = 0;
                ok = GetOverlappedResult(handle, &overlapped, &transferred, FALSE) != FALSE;
            }
        }

        if (!ok)
        {
            CancelIoEx(handle, &overlapped);
            DWORD ignored = 0;
            GetOverlappedResult(handle, &overlapped, &ignored, TRUE);
        }
        CloseHandle(overlapped.hEvent);
        if (!ok)
        {
            CloseHandle(handle);
            return false;
        }

        connection.handle = handle;
        return true;
    }

    bool connect_ipc(ipc_connection &connection, const string &endpoint, u32 timeout_ms)
    {
        close_ipc(connection);
        if (endpoint.empty()) return false;

        const ULONGLONG deadline = GetTickCount64() + timeout_ms;
        do
        {
            connection.handle = CreateFileA(endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                            FILE_FLAG_OVERLAPPED, nullptr);
            if (connection.handle != INVALID_HANDLE_VALUE) return true;

            const DWORD error = GetLastError();
            if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND) return false;

            const ULONGLONG now = GetTickCount64();
            if (now >= deadline) break;
            const DWORD remaining = static_cast<DWORD>(deadline - now);
            if (error == ERROR_PIPE_BUSY)
                WaitNamedPipeA(endpoint.c_str(), remaining);
            else
                Sleep(std::min<DWORD>(remaining, 10));
        } while (GetTickCount64() < deadline);
        return false;
    }

    bool read_ipc(ipc_connection &connection, void *data, size_t size, u32 timeout_ms)
    {
        return connection.handle != INVALID_HANDLE_VALUE && data && overlapped_io(connection.handle, false, data, size,
                                                                                  timeout_ms);
    }

    bool write_ipc(ipc_connection &connection, const void *data, size_t size, u32 timeout_ms)
    {
        return connection.handle != INVALID_HANDLE_VALUE && data &&
               overlapped_io(connection.handle, true, const_cast<void *>(data), size, timeout_ms);
    }

    void close_ipc(ipc_connection &connection) noexcept
    {
        if (connection.handle != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(connection.handle);
            DisconnectNamedPipe(connection.handle);
            CloseHandle(connection.handle);
        }
        connection.handle = INVALID_HANDLE_VALUE;
    }

    void close_ipc(ipc_listener &listener) noexcept
    {
        listener.endpoint.clear();
        listener.buffer_size = 0;
    }
} // namespace acul
