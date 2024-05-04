#ifndef APP_CORE_FILE_H
#define APP_CORE_FILE_H

#include <filesystem>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_group.h>
#include <string>
#include <string_view>
#include "../std/darray.hpp"
#include "../api.hpp"
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
        /**
         * @brief Reads a file as binary buffer
         * @param filename The name of the file to read.
         * @param buffer A reference to a variable to store the data.
         * @return True if the file was successfully read, false otherwise.
         **/
        APPLIB_API bool readBinary(const std::string &filename, DArray<char> &buffer);

        /**
         * @brief Writes a binary buffer to a file
         * @param filename The name of the file to write.
         * @param buffer The data to write.
         * @param size The size of the data.
         * @return True if the file was successfully written, false otherwise.
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
        APPLIB_API void fillLineBuffer(const char *data, size_t size, DArray<std::string_view> &dst);

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
        bool readByBlock(const std::string &filename, std::string &error, T &dstBuffer,
                         void (*callback)(T &, const std::string_view &, int))
        {
#ifdef _WIN32
            HANDLE fileHandle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                           FILE_ATTRIBUTE_NORMAL, NULL);
            if (fileHandle == INVALID_HANDLE_VALUE)
            {
                error = "Failed to open file: " + filename;
                return false;
            }

            LARGE_INTEGER fileSize;
            if (!GetFileSizeEx(fileHandle, &fileSize))
            {
                CloseHandle(fileHandle);
                error = "Failed to get file size: " + filename;
                return false;
            }

            HANDLE mappingHandle = CreateFileMapping(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
            if (mappingHandle == NULL)
            {
                CloseHandle(fileHandle);
                error = "Failed to create file mapping: " + filename;
                return false;
            }

            char *fileData = static_cast<char *>(MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, fileSize.QuadPart));
            if (fileData == NULL)
            {
                CloseHandle(mappingHandle);
                CloseHandle(fileHandle);
                error = "Failed to map file to memory: " + filename;
                return false;
            }

            // Parse the file in parallel
            try
            {
                DArray<std::string_view> lines;
                fillLineBuffer(fileData, fileSize.QuadPart, lines);
                oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<size_t>(0, lines.size(), 512),
                                          [&](const oneapi::tbb::blocked_range<size_t> &range) {
                                              for (size_t i = range.begin(); i != range.end(); ++i)
                                                  callback(dstBuffer, lines[i], i);
                                          });
            }
            catch (const std::exception &e)
            {
                error = "File error: " + std::string(e.what());
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
                DArray<std::string_view> lines;
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
            return error.empty();
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
        APPLIB_API bool writeFileByBlock(const std::string &filename, const char *buffer, size_t blockSize, std::string &error);

        /**
         * @brief std::getline function implementation providing support for Windows &
         * Unix line endings
         * @see
         * https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
         * @param is Input stream
         * @param t Dst std::string buffer
         * @return Current stream
         */
        APPLIB_API std::istream &safeGetline(std::istream &is, std::string &t);

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
    } // namespace file
} // namespace io

#endif