#include <acul/ipc.hpp>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdio>

namespace acul
{
    bool create_pipe(pipe &dst, const char *name, const char *env, size_t size)
    {
        char path[128];
        snprintf(path, sizeof(path), "/tmp/%s-%lu.fifo", name, static_cast<unsigned long>(getpid()));
        dst.path = path;
        unlink(dst.path.c_str());
        if (mkfifo(dst.path.c_str(), 0600) == -1) return false;
        dst.fd = open(dst.path.c_str(), O_RDWR | O_NONBLOCK);
        if (dst.fd == -1) return false;
        return setenv(env, dst.path.c_str(), 1) == 0;
    }

    void close_pipe(pipe &src, const char *env)
    {
        unsetenv(env);
        if (src.fd != -1) close(src.fd);
        src.fd = -1;
        if (!src.path.empty()) unlink(src.path.c_str());
    }

    namespace
    {
        bool socket_address(const string &endpoint, sockaddr_un &address)
        {
            if (endpoint.empty() || endpoint.size() >= sizeof(address.sun_path)) return false;
            memset(&address, 0, sizeof(address));
            address.sun_family = AF_UNIX;
            memcpy(address.sun_path, endpoint.c_str(), endpoint.size() + 1);
            return true;
        }

        bool wait_fd(int fd, short events, u32 timeout_ms)
        {
            pollfd descriptor{fd, events, 0};
            int result;
            do
                result = poll(&descriptor, 1, static_cast<int>(timeout_ms));
            while (result < 0 && errno == EINTR);
            return result > 0 && (descriptor.revents & events);
        }
    } // namespace

    bool listen_ipc(ipc_listener &listener, const string &endpoint, size_t)
    {
        close_ipc(listener);
        sockaddr_un address{};
        if (!socket_address(endpoint, address)) return false;

        const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) return false;
        unlink(endpoint.c_str());
        if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 || listen(fd, 8) != 0)
        {
            close(fd);
            unlink(endpoint.c_str());
            return false;
        }

        listener.fd = fd;
        listener.endpoint = endpoint;
        return true;
    }

    bool accept_ipc(ipc_listener &listener, ipc_connection &connection, u32 timeout_ms)
    {
        close_ipc(connection);
        if (listener.fd < 0 || !wait_fd(listener.fd, POLLIN, timeout_ms)) return false;
        connection.fd = accept(listener.fd, nullptr, nullptr);
        return connection.fd >= 0;
    }

    bool connect_ipc(ipc_connection &connection, const string &endpoint, u32 timeout_ms)
    {
        close_ipc(connection);
        sockaddr_un address{};
        if (!socket_address(endpoint, address)) return false;

        const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd < 0) return false;
        int result = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
        if (result != 0 && errno == EINPROGRESS)
        {
            if (wait_fd(fd, POLLOUT, timeout_ms))
            {
                int error = 0;
                socklen_t size = sizeof(error);
                result = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0 ? 0 : -1;
            }
        }
        if (result != 0)
        {
            close(fd);
            return false;
        }
        connection.fd = fd;
        return true;
    }

    bool read_ipc(ipc_connection &connection, void *data, size_t size, u32 timeout_ms)
    {
        if (connection.fd < 0 || !data) return false;
        char *cursor = static_cast<char *>(data);
        size_t remaining = size;
        while (remaining)
        {
            if (!wait_fd(connection.fd, POLLIN, timeout_ms)) return false;
            const ssize_t count = recv(connection.fd, cursor, remaining, 0);
            if (count <= 0) return false;
            cursor += count;
            remaining -= static_cast<size_t>(count);
        }
        return true;
    }

    bool write_ipc(ipc_connection &connection, const void *data, size_t size, u32 timeout_ms)
    {
        if (connection.fd < 0 || !data) return false;
        const char *cursor = static_cast<const char *>(data);
        size_t remaining = size;
        while (remaining)
        {
            if (!wait_fd(connection.fd, POLLOUT, timeout_ms)) return false;
            const ssize_t count = send(connection.fd, cursor, remaining, MSG_NOSIGNAL);
            if (count <= 0) return false;
            cursor += count;
            remaining -= static_cast<size_t>(count);
        }
        return true;
    }

    void close_ipc(ipc_connection &connection) noexcept
    {
        if (connection.fd >= 0) close(connection.fd);
        connection.fd = -1;
    }

    void close_ipc(ipc_listener &listener) noexcept
    {
        if (listener.fd >= 0) close(listener.fd);
        listener.fd = -1;
        if (!listener.endpoint.empty()) unlink(listener.endpoint.c_str());
        listener.endpoint.clear();
    }
} // namespace acul
