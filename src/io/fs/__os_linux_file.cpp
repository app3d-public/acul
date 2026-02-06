#include <acul/functional/unique_function.hpp>
#include <acul/io/fs/file.hpp>
#include <acul/string/string.hpp>
#include <cassert>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace acul::fs
{
    op_result write_by_block(const string &filename, const char *buffer, size_t block_size)
    {
        int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return make_op_error(ACUL_OP_WRITE_ERROR, errno);

        size_t buffer_size = null_terminated_length(buffer);
        size_t num_blocks = (buffer_size + block_size - 1) / block_size;

        for (size_t block_index = 0; block_index < num_blocks; ++block_index)
        {
            size_t offset = block_index * block_size;
            size_t size = std::min(buffer_size - offset, block_size);

            ssize_t written = write(fd, buffer + offset, size);
            if (written < 0 || static_cast<size_t>(written) < size)
            {
                int err = errno;
                close(fd);
                return make_op_error(ACUL_OP_WRITE_ERROR, err);
            }
        }

        close(fd);
        return make_op_success();
    }

    op_result copy_file(const char *src, const char *dst, bool overwrite) noexcept
    {
        int src_fd = open(src, O_RDONLY);
        if (src_fd < 0) return make_op_error(ACUL_OP_READ_ERROR, errno);

        int flags = O_WRONLY | O_CREAT;
        flags |= overwrite ? O_TRUNC : O_EXCL;

        int dst_fd = open(dst, flags, 0644);
        if (dst_fd < 0)
        {
            int err = errno;
            close(src_fd);

            if (!overwrite && err == EEXIST) return op_result(ACUL_OP_SUCCESS, ACUL_OP_DOMAIN, ACUL_OP_CODE_SKIPPED);

            return make_op_error(ACUL_OP_WRITE_ERROR, err);
        }

        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0)
        {
            ssize_t bytes_written = write(dst_fd, buffer, bytes_read);
            if (bytes_written != bytes_read)
            {
                int err = errno;
                close(src_fd);
                close(dst_fd);
                return make_op_error(ACUL_OP_WRITE_ERROR, err);
            }
        }

        if (bytes_read < 0)
        {
            int err = errno;
            close(src_fd);
            close(dst_fd);
            return make_op_error(ACUL_OP_READ_ERROR, err);
        }

        close(src_fd);
        close(dst_fd);
        return make_op_success();
    }

    op_result create_directory(const char *path)
    {
        if (mkdir(path, 0755) == 0) return make_op_success();

        int error = errno;
        if (error == EEXIST)
        {
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
                return op_result(ACUL_OP_SUCCESS, ACUL_OP_DOMAIN, ACUL_OP_CODE_SKIPPED);
        }

        return make_op_error(ACUL_OP_WRITE_ERROR, error);
    }

    op_result remove_file(const char *path)
    {
        return unlink(path) == 0 ? make_op_success() : make_op_error(ACUL_OP_DELETE_ERROR, errno);
    }

    op_result list_files(const string &base_path, vector<string> &dst, bool recursive)
    {
        assert(!base_path.empty() && "base_path is null");

        DIR *dir = opendir(base_path.c_str());
        if (!dir) return make_op_error(ACUL_OP_READ_ERROR, errno);

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            const char *name = entry->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

            string full_path = base_path + "/" + name;

            struct stat st;
            if (stat(full_path.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode))
            {
                if (recursive) list_files(full_path, dst, true);
            }
            else { dst.push_back(full_path); }
        }

        closedir(dir);
        return make_op_success();
    }

    op_result read_by_block(const string &filename, unique_function<void(char *, size_t)> callback)
    {
        int fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0) return make_op_error(ACUL_OP_READ_ERROR, errno);

        struct stat st;
        if (fstat(fd, &st) < 0)
        {
            int err = errno;
            close(fd);
            return make_op_error(ACUL_OP_INVALID_SIZE, err);
        }

        if (st.st_size == 0)
        {
            close(fd);
            return make_op_success();
        }

        void *mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED)
        {
            int err = errno;
            close(fd);
            return make_op_error(ACUL_OP_MAP_ERROR, err);
        }

        u16 state = ACUL_OP_SUCCESS;
        try
        {
            callback(static_cast<char *>(mapped), st.st_size);
        }
        catch (...)
        {
            state = ACUL_OP_ERROR_GENERIC;
        }

        munmap(mapped, st.st_size);
        close(fd);
        return {state, ACUL_OP_DOMAIN};
    }
} // namespace acul::fs