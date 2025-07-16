#ifndef APP_ACUL_FILE_H
#define APP_ACUL_FILE_H

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include "../api.hpp"
#include "../string/string_pool.hpp"
#include "../vector.hpp"

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
    namespace io
    {
        namespace file
        {
            enum op_state
            {
                unknown,
                success,
                error,
                checksum_mismatch,
                skipped_existing
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
                DWORD fileAttr = GetFileAttributesA(path);
                return (fileAttr != INVALID_FILE_ATTRIBUTES);
#else
                struct stat buffer;
                return (stat(path, &buffer) == 0);
#endif
            }

            /**
             * @brief Reads a file as binary buffer
             * @param filename The name of the file to read.
             * @param buffer A reference to a variable to store the data.
             * @return Success if the file was successfully read, error otherwise.
             **/
            APPLIB_API op_state read_binary(const string &filename, vector<char> &buffer);

            /**
             * @brief Reads a virtual file as binary buffer.
             * Needs for files wihtout tellg access (/proc/<PID>/task for example)
             * @param filename The name of the file to read.
             * @param buffer A reference to a variable to store the data.
             * @return Success if the file was successfully read, error otherwise.
             **/
            APPLIB_API op_state read_virtual(const string &filename, vector<char> &buffer);

            /**
             * @brief Writes a binary buffer to a file
             * @param filename The name of the file to write.
             * @param buffer The data to write.
             * @param size The size of the data.
             * @return Success if the file was successfully written, error otherwise.
             **/
            APPLIB_API bool write_binary(const string &filename, const char *buffer, size_t size);

            /**
             * Fills the line buffer by parsing the input data and splitting it into lines based on newline characters.
             *
             * @param data Pointer to the start of the data buffer
             * @param size Size of the data buffer
             * @param dst Dynamic array to store the parsed lines
             */
            APPLIB_API void fill_line_buffer(const char *data, size_t size, acul::string_pool<char> &dst);

            /**
             * Reads a file in blocks and processes it using a callback function.
             * @param filename The name of the file to read.
             * @param callback The callback function that will be called with the processed file data.
             * @return op_state::success if the file was successfully read and processed,
             * op_state::error otherwise.
             */

            APPLIB_API op_state read_by_block(const string &filename,
                                              const std::function<void(char *, size_t)> &callback);

            /**
             * Writes data to a file in blocks.
             *
             * @param filename The name of the file to write to.
             * @param buffer A pointer to the data to write.
             * @param blockSize The size of each block to write.
             * @param error A reference to a string to store any error messages.
             * @return True if the data was successfully written to the file, false
             * otherwise.
             */
            APPLIB_API bool write_by_block(const acul::string &filename, const char *buffer, size_t blockSize,
                                           acul::string &error);

            /**
             * @brief Copy a file from source path to destination path.
             *
             * @param src The source path of the file to be copied.
             * @param dst The destination path where the file will be copied.
             * @param overwrite If true, the destination file will be overwritten if it already exists.
             * @return Returns a status code indicating the success or failure of the copy operation.
             */
            APPLIB_API op_state copy(const char *src, const char *dst, bool overwrite) noexcept;

            /**
             * @brief Creates a directory at the specified path.
             *
             * @param path The path of the directory to be created.
             * @return Returns a status code indicating the success or failure of the directory creation operation.
             */
            APPLIB_API op_state create_directory(const char *path);

            APPLIB_API op_state remove_file(const char *path);

            APPLIB_API op_state list_files(const acul::string &base_path, vector<acul::string> &dst,
                                           bool recursive = false);

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
             * @return Returns true if the compression was successful, false otherwise.
             */
            APPLIB_API bool compress(const char *data, size_t size, vector<char> &compressed, int quality);

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
             * @return Returns true if the decompression was successful, false otherwise.
             */
            APPLIB_API bool decompress(const char *data, size_t size, vector<char> &decompressed);

        } // namespace file
    } // namespace io
} // namespace acul

#endif