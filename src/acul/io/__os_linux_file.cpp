#include <acul/io/file.hpp>
#include <acul/log.hpp>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

namespace acul
{
    namespace io
    {
        namespace file
        {
            bool write_by_block(const string &filename, const char *buffer, size_t block_size, string &error)
            {
                int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0)
                {
                    error = "Failed to open file: " + filename + ": " + strerror(errno);
                    return false;
                }

                size_t buffer_size = std::strlen(buffer);
                size_t num_blocks = (buffer_size + block_size - 1) / block_size;

                for (size_t block_index = 0; block_index < num_blocks; ++block_index)
                {
                    size_t offset = block_index * block_size;
                    size_t size = std::min(buffer_size - offset, block_size);

                    ssize_t written = write(fd, buffer + offset, size);
                    if (written < 0 || static_cast<size_t>(written) < size)
                    {
                        close(fd);
                        error = "Failed to write block to file: " + filename + ": " + strerror(errno);
                        return false;
                    }
                }

                close(fd);
                return true;
            }

            op_state copy(const char *src, const char *dst, bool overwrite) noexcept
            {
                int src_fd = open(src, O_RDONLY);
                if (src_fd < 0) return op_state::Error;

                int flags = O_WRONLY | O_CREAT;
                flags |= overwrite ? O_TRUNC : O_EXCL;

                int dst_fd = open(dst, flags, 0644);
                if (dst_fd < 0)
                {
                    int err = errno;
                    close(src_fd);

                    if (!overwrite && err == EEXIST) return op_state::SkippedExisting;

                    return op_state::Error;
                }

                char buffer[4096];
                ssize_t bytes_read, bytes_written;
                bool copy_success = true;

                while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0)
                {
                    bytes_written = write(dst_fd, buffer, bytes_read);
                    if (bytes_written != bytes_read)
                    {
                        copy_success = false;
                        break;
                    }
                }

                if (bytes_read < 0) copy_success = false;

                close(src_fd);
                close(dst_fd);

                return copy_success ? op_state::Success : op_state::Error;
            }

            op_state create_directory(const char *path)
            {
                if (mkdir(path, 0755) == 0)
                    return op_state::Success;
                else
                {
                    int error = errno;
                    if (error == EEXIST) return op_state::SkippedExisting;

                    LOG_ERROR("Failed to create directory %s. Error code: %d", path, error);
                    return op_state::Error;
                }
            }

            op_state remove_file(const char *path)
            {
                if (unlink(path) == 0)
                    return op_state::Success;
                else
                {
                    int error = errno;
                    LOG_ERROR("Failed to remove file %s. Error code: %d", path, error);
                    return op_state::Error;
                }
            }

            op_state list_files(const string &base_path, vector<string> &dst, bool recursive)
            {
                assert(!base_path.empty() && "base_path is null");

                DIR *dir = opendir(base_path.c_str());
                if (!dir)
                {
                    LOG_ERROR("Failed to open directory: %s. Error code: %d", base_path.c_str(), errno);
                    return op_state::Error;
                }

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
                return op_state::Success;
            }

            op_state read_by_block(const string &filename, const std::function<void(char *, size_t)> &callback)
            {
                int fd = open(filename.c_str(), O_RDONLY);
                if (fd < 0) return op_state::Error;

                struct stat st;
                if (fstat(fd, &st) < 0)
                {
                    close(fd);
                    return op_state::Error;
                }

                if (st.st_size == 0)
                {
                    close(fd);
                    return op_state::Success;
                }

                void *mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                if (mapped == MAP_FAILED)
                {
                    close(fd);
                    return op_state::Error;
                }

                op_state res = op_state::Success;
                try
                {
                    callback(static_cast<char *>(mapped), st.st_size);
                }
                catch (...)
                {
                    res = op_state::Error;
                }

                munmap(mapped, st.st_size);
                close(fd);
                return res;
            }
        } // namespace file
    } // namespace io
} // namespace acul