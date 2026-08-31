#ifndef APP_ACUL_FILE_H
#define APP_ACUL_FILE_H

#include <cassert>
#include <cstdio>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include "../../op_result.hpp"
#include "../../vector.hpp"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <cstring>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace acul
{
    using fd_lock = u64;
    inline constexpr fd_lock invalid_fd_lock = static_cast<fd_lock>(-1);
} // namespace acul

namespace acul::fs
{
    /** Read-only mapping. The source must not be modified or truncated while it is mapped. */
    class mapped_file
    {
    public:
        mapped_file() noexcept = default;
        ~mapped_file() noexcept { close(); }
        mapped_file(const mapped_file &) = delete;
        mapped_file &operator=(const mapped_file &) = delete;

        mapped_file(mapped_file &&other) noexcept : _data(other._data), _size(other._size)
        {
            other._data = nullptr;
            other._size = 0u;
        }
        mapped_file &operator=(mapped_file &&other) noexcept
        {
            if (this != &other)
            {
                close();
                _data = other._data;
                _size = other._size;
                other._data = nullptr;
                other._size = 0u;
            }
            return *this;
        }

        ACUL_EXPORT op_result open(const string &filename);
        ACUL_EXPORT void close() noexcept;
        const char *data() const noexcept { return _data; }
        size_t size() const noexcept { return _size; }

    private:
        const char *_data = nullptr;
        size_t _size = 0u;
    };

    /**
     * @brief Checks if a file or directory exists at the given path.
     *
     * This function determines the existence of a file or directory by checking
     * the file attributes on Windows or calling stat on Unix-based systems.
     *
     * @param path The path of the file or directory to check.
     * @return True if the file or directory exists, false otherwise.
     */

    inline bool exists(const char *path) noexcept
    {
#ifdef _WIN32
        DWORD file_attr = GetFileAttributesA(path);
        return (file_attr != INVALID_FILE_ATTRIBUTES);
#else
        struct stat buffer;
        return (stat(path, &buffer) == 0);
#endif
    }

    ACUL_EXPORT bool is_directory(const char *path) noexcept;

    /**
     * @brief Tries to acquire an exclusive, non-blocking lock on a file.
     *
     * The file is created if it does not exist. The returned descriptor owns
     * the lock. The caller must retain it as long as the lock is needed and pass
     * it to unlock_file() to release it.
     * The operating system also releases the descriptor when the process exits.
     *
     * On Linux the lock is advisory, so all cooperating processes must use file
     * locking as well.
     *
     * @param filename Path to the file used as a lock.
     * @return An owning descriptor if the lock was acquired, invalid_fd_lock if it
     * is already held or the file could not be opened or locked.
     */
    ACUL_EXPORT fd_lock lock_file(const string &filename);

    /**
     * @brief Releases a file lock and closes its descriptor.
     *
     * @param id Descriptor returned by lock_file(). It must not be reused after
     * this call.
     * @return True if the lock was released and the descriptor was closed.
     */
    ACUL_EXPORT bool unlock_file(fd_lock id) noexcept;

    /**
     * @brief Checks whether another lock holder has exclusively locked a file.
     *
     * The check is non-blocking and does not retain a lock. The file is created
     * if it does not exist.
     *
     * @param filename Path to the file used as a lock.
     * @return True if an exclusive lock is currently held, false otherwise.
     */
    ACUL_EXPORT bool is_file_locked(const string &filename);

    /**
     * @brief Opens a file in binary read mode and returns its size.
     * @param filename Path to the file to open.
     * @param fd FILE pointer to store the opened file descriptor.
     * @return Size of the file in bytes.
     */
    ACUL_EXPORT size_t read_binary_fd(const string &filename, FILE *&fd);

    /**
     * @brief Seeks to a specific offset in a file.
     *
     * This function is a thin wrapper around the platform-specific fseek
     * functions. It takes a file descriptor, an offset in bytes, and an
     * origin (one of SEEK_SET, SEEK_CUR, or SEEK_END) and moves the file
     * pointer accordingly.
     *
     * @param fd Valid file descriptor.
     * @param offset Offset in bytes to move the file pointer to.
     * @param origin One of SEEK_SET, SEEK_CUR, or SEEK_END.
     * @return 0 on success, -1 on error.
     */
    inline int fseek(FILE *fd, u64 offset, int origin)
    {
        assert(fd && "Invalid file descriptor");
#ifdef _WIN32
        return _fseeki64(fd, static_cast<long long>(offset), origin);
#else
        return fseeko(fd, static_cast<off_t>(offset), origin);
#endif
    }

    /**
     * @brief Retrieves the current position of the file pointer.
     *
     * @param fd Valid file descriptor.
     * @return The current offset of the file pointer in bytes.
     */

    inline i64 ftell(FILE *fd)
    {
        assert(fd && "Invalid file descriptor");
#ifdef _WIN32
        return static_cast<i64>(_ftelli64(fd));
#else
        return static_cast<i64>(ftello(fd));
#endif
    }

    /**
     * @brief Reads a file as binary buffer
     * @param filename The name of the file to read.
     * @param buffer A reference to a variable to store the data.
     * @return Success if the file was successfully read, error otherwise.
     **/
    ACUL_EXPORT bool read_binary(const string &filename, vector<char> &buffer);

    /**
     * @brief Reads a virtual file as binary buffer.
     * Needs for files wihtout tellg access (/proc/<PID>/task for example)
     * @param filename The name of the file to read.
     * @param buffer A reference to a variable to store the data.
     * @return Success if the file was successfully read, error otherwise.
     **/
    ACUL_EXPORT bool read_virtual(const string &filename, vector<char> &buffer);

    /**
     * @brief Writes a binary buffer to a file
     * @param filename The name of the file to write.
     * @param buffer The data to write.
     * @param size The size of the data.
     * @return Success if the file was successfully written, error otherwise.
     **/
    ACUL_EXPORT bool write_binary(const string &filename, const char *buffer, size_t size);

    /**
     * Reads a file in blocks and processes it using a callback function.
     * @param filename The name of the file to read.
     * @param callback The callback function that will be called with the processed file data.
     * @return op_state::success if the file was successfully read and processed,
     * op_state::error otherwise.
     */

    ACUL_EXPORT op_result read_by_block(const string &filename, unique_function<void(char *, size_t)> callback);

    /**
     * Writes data to a file in blocks.
     *
     * @param filename The name of the file to write to.
     * @param buffer A pointer to the data to write.
     * @param block_size The size of each block to write.
     * @return Returns op_result
     * otherwise.
     */
    ACUL_EXPORT op_result write_by_block(const acul::string &filename, const char *buffer, size_t block_size);

    /**
     * @brief Copy a file from source path to destination path.
     *
     * @param src The source path of the file to be copied.
     * @param dst The destination path where the file will be copied.
     * @param overwrite If true, the destination file will be overwritten if it already exists.
     * @return Returns a status code indicating the success or failure of the copy operation.
     */
    ACUL_EXPORT op_result copy_file(const char *src, const char *dst, bool overwrite) noexcept;

    /**
     * @brief Creates a directory at the specified path.
     *
     * @param path The path of the directory to be created.
     * @return Returns a status code indicating the success or failure of the directory creation operation.
     */
    ACUL_EXPORT op_result create_directory(const char *path);

    /**
     * @brief Removes an empty directory at the specified path.
     *
     * @param path The path of the directory to be removed.
     * @return Success if the directory was removed, otherwise an error with the
     * platform-specific error code.
     */
    ACUL_EXPORT op_result remove_directory(const char *path);

    ACUL_EXPORT op_result remove_file(const char *path);
    ACUL_EXPORT op_result list_files(const acul::string &base_path, vector<acul::string> &dst, bool recursive = false);

    /**
     * @brief Compresses the given data using zstd.
     *
     * This function compresses the provided data buffer using the zstd compression
     * algorithm.
     *
     * @param data Pointer to the data buffer to be compressed.
     * @param size The size of the data buffer.
     * @param compressed The resulting compressed data will be stored in this
     * vector.
     * @param quality The compression quality level, which can be in the range [1,
     * 22]. Typically, values from 1 to 12 are used. A value of 1 results in the
     * fastest compression speed (but less compression), while a value of 22
     * provides maximum compression (at the cost of speed).
     * @return Returns the op result
     */
    ACUL_EXPORT op_result compress(const char *data, size_t size, vector<char> &compressed, int quality);

    /**
     * @brief Decompresses the given data using zstd.
     *
     * This function decompresses the provided compressed data buffer using the zstd
     * decompression algorithm.
     *
     * @param data Pointer to the compressed data buffer.
     * @param size The size of the compressed data buffer.
     * @param decompressed The resulting decompressed data will be stored in this
     * vector.
     * @return Returns the op result
     */
    ACUL_EXPORT op_result decompress(const char *data, size_t size, vector<char> &decompressed);

} // namespace acul::fs

#endif
