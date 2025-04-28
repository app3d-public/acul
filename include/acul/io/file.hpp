#ifndef APP_ACUL_FILE_H
#define APP_ACUL_FILE_H

#include <acul/log.hpp>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include "../api.hpp"
#include "../string/string_pool.hpp"
#include "../string/utils.hpp"
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
                undefined,
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
                return (stat(path.c_str(), &buffer) == 0);
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
             * Reads a file in blocks using in mutithread context, splits it into lines, and processes each line using a
             * callback function.
             *
             * @param filename The name of the file to read.
             * @param error A reference to a string to store any error messages.
             * @param dst A reference to a variable to store the processed data.
             * @param callback A function pointer to the callback function that will be
             * called for each line.
             * @return True if the file was successfully read and processed, false
             * otherwise.
             */
            template <typename T>
            op_state read_by_block(const string &filename, T &dst, void (*callback)(T &, const char *, int))
            {
#ifdef _WIN32
                u16string lFilename = utf8_to_utf16(filename);
                HANDLE fileHandle = CreateFileW((LPCWSTR)lFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (fileHandle == INVALID_HANDLE_VALUE)
                {
                    logError("Failed to open file: %s", filename.c_str());
                    return op_state::error;
                }

                LARGE_INTEGER fileSize;
                if (!GetFileSizeEx(fileHandle, &fileSize))
                {
                    CloseHandle(fileHandle);
                    return op_state::error;
                }

                HANDLE mappingHandle = CreateFileMapping(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
                if (mappingHandle == NULL)
                {
                    CloseHandle(fileHandle);
                    return op_state::error;
                }

                char *fileData =
                    static_cast<char *>(MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, fileSize.QuadPart));
                if (fileData == NULL)
                {
                    CloseHandle(mappingHandle);
                    CloseHandle(fileHandle);
                    return op_state::error;
                }

                // Parse the file in parallel
                op_state res = op_state::success;
                try
                {
                    acul::string_pool<char> stringPool(fileSize.QuadPart);
                    fill_line_buffer(fileData, fileSize.QuadPart, stringPool);
                    oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<size_t>(0, stringPool.size(), 512),
                                              [&](const oneapi::tbb::blocked_range<size_t> &range) {
                                                  for (size_t i = range.begin(); i != range.end(); ++i)
                                                      callback(dst, stringPool[i], i);
                                              });
                }
                catch (...)
                {
                    res = op_state::error;
                }

                // Unmap the file from memory
                UnmapViewOfFile(fileData);
                CloseHandle(mappingHandle);
                CloseHandle(fileHandle);
#else
                int fd = open(filename.c_str(), O_RDONLY);
                if (fd == -1) return ReadState::Error;

                struct stat fileInfo;
                if (fstat(fd, &fileInfo) != 0)
                {
                    close(fd);
                    return ReadState::Error;
                }
                size_t fileSize = fileInfo.st_size;

                char *fileData = static_cast<char *>(mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0));
                if (fileData == MAP_FAILED)
                {
                    close(fd);
                    return ReadState::MapError;
                }
                // Parse the file in parallel
                ReadState res = ReadState::Success;
                try
                {
                    acul::string_pool<char> stringPool(fileSize);
                    fill_line_buffer(fileData, fileSize, stringPool);
                    oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<size_t>(0, stringPool.size(), 512),
                                              [&](const oneapi::tbb::blocked_range<size_t> &range) {
                                                  for (size_t i = range.begin(); i != range.end(); ++i)
                                                      callback(dstBuffer, stringPool[i], i);
                                              });
                }
                catch (const std::exception &e)
                {
                    res = ReadState::Error;
                }

                munmap(fileData, fileSize);
                close(fd);
#endif
                return res;
            }

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