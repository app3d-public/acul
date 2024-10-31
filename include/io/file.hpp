#ifndef APP_CORE_FILE_H
#define APP_CORE_FILE_H

#include <filesystem>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <string>
#include "../astl/string_pool.hpp"
#include "../astl/vector.hpp"
#include "../core/api.hpp"

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

namespace io
{
    namespace file
    {
        enum class ReadState
        {
            Undefined,
            Success,
            Error,
            ChecksumMismatch,
            SizeError,
            MapError
        };

        /**
         * @brief Reads a file as binary buffer
         * @param filename The name of the file to read.
         * @param buffer A reference to a variable to store the data.
         * @return Success if the file was successfully read, error otherwise.
         **/
        APPLIB_API ReadState readBinary(const std::string &filename, astl::vector<char> &buffer);

        /**
         * @brief Writes a binary buffer to a file
         * @param filename The name of the file to write.
         * @param buffer The data to write.
         * @param size The size of the data.
         * @return Success if the file was successfully written, error otherwise.
         **/
        APPLIB_API bool writeBinary(const std::string &filename, const char *buffer, size_t size);

        /**
         * Fills the line buffer by parsing the input data and splitting it into lines based on newline characters.
         * Note: Works in mutithread context
         *
         * @param data Pointer to the start of the data buffer
         * @param size Size of the data buffer
         * @param dst Dynamic array to store the parsed lines
         */
        void fillLineBuffer(const char *data, size_t size, astl::string_pool<char> &dst);

        /**
         * Reads a file in blocks using in mutithread context, splits it into lines, and processes each line using a
         * callback function.
         *
         * @param filename The name of the file to read.
         * @param error A reference to a string to store any error messages.
         * @param dstBuffer A reference to a variable to store the processed data.
         * @param callback A function pointer to the callback function that will be
         * called for each line.
         * @return True if the file was successfully read and processed, false
         * otherwise.
         */
        template <typename T>
        ReadState readByBlock(const std::string &filename, T &dstBuffer, void (*callback)(T &, const char *, int))
        {
#ifdef _WIN32
            HANDLE fileHandle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                           FILE_ATTRIBUTE_NORMAL, NULL);
            if (fileHandle == INVALID_HANDLE_VALUE) return ReadState::Error;

            LARGE_INTEGER fileSize;
            if (!GetFileSizeEx(fileHandle, &fileSize))
            {
                CloseHandle(fileHandle);
                return ReadState::SizeError;
            }

            HANDLE mappingHandle = CreateFileMapping(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
            if (mappingHandle == NULL)
            {
                CloseHandle(fileHandle);
                return ReadState::MapError;
            }

            char *fileData = static_cast<char *>(MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, fileSize.QuadPart));
            if (fileData == NULL)
            {
                CloseHandle(mappingHandle);
                CloseHandle(fileHandle);
                return ReadState::MapError;
            }

            // Parse the file in parallel
            ReadState res = ReadState::Success;
            try
            {
                astl::string_pool<char> stringPool(fileSize.QuadPart);
                fillLineBuffer(fileData, fileSize.QuadPart, stringPool);
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

            // Unmap the file from memory
            UnmapViewOfFile(fileData);
            CloseHandle(mappingHandle);
            CloseHandle(fileHandle);
#else
            int fd = open(filename.c_str(), O_RDONLY);
            if (fd == -1)
            {
                error = "Failed to open file: " + filename + " " + strerror(errno);
                return false;
            }

            struct stat fileInfo;
            if (fstat(fd, &fileInfo) != 0)
            {
                close(fd);
                error = "Failed to get file size: " + filename + " " + strerror(errno);
                return false;
            }
            size_t fileSize = fileInfo.st_size;

            char *fileData = static_cast<char *>(mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0));
            if (fileData == MAP_FAILED)
            {
                close(fd);
                error = "Failed to map file to memory: " + filename + " " + strerror(errno);
                return false;
            }

            try
            {
                astl::vector<std::string_view> lines;
                fillLineBuffer(fileData, fileSize, lines);
                oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<size_t>(0, lines.size(), 512),
                                          [&](const oneapi::tbb::blocked_range<size_t> &range) {
                                              for (size_t i = range.begin(); i != range.end(); ++i)
                                                  callback(dstBuffer, lines[i], i);
                                          });
            }
            catch (const std::exception &e)
            {
                error = "Failed to process file with error: " + std::string(e.what());
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
        APPLIB_API bool writeFileByBlock(const std::string &filename, const char *buffer, size_t blockSize,
                                         std::string &error);

        /**
         * @brief Copy a file from source path to destination path.
         *
         * @param src The source path of the file to be copied.
         * @param dst The destination path where the file will be copied.
         * @param options The copy options to be applied during the copy operation.
         * @return true if the file is successfully copied, false otherwise.
         */
        APPLIB_API bool copyFile(const std::filesystem::path &src, const std::filesystem::path &dst,
                                 const std::filesystem::copy_options options);

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
        APPLIB_API bool compress(const char *data, size_t size, astl::vector<char> &compressed, int quality);

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
        APPLIB_API bool decompress(const char *data, size_t size, astl::vector<char> &decompressed);
    } // namespace file
} // namespace io

#endif