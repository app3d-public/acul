#include <acul/ipc.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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
} // namespace acul
