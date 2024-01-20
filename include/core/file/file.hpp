#ifndef APP_CORE_FILE_H
#define APP_CORE_FILE_H

#include <filesystem>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_group.h>
#include <string>
#include <thread>
#include "../std/array.hpp"

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
        bool readBinary(const std::string &filename, Array<char> &buffer);

        /**
         * @brief Writes a binary buffer to a file
         * @param filename The name of the file to write.
         * @param buffer The data to write.
         * @param size The size of the data.
         * @return True if the file was successfully written, false otherwise.
         **/
        bool writeBinary(const std::string &filename, char *buffer, size_t size);

        /**
         * Reads a file using memory mapping, which maps the content of a file into
         * the memory space of the calling process. This method is efficient for
         * reading large files as it minimizes disk I/O operations. The function reads
         * the file line by line and processes each line using a callback function.
         *
         * @param filename The name of the file to read.
         * @param error A reference to a string for storing any error messages that
         * may occur during the file reading process.
         * @param dstBuffer A reference to a variable where the processed data will
         * be stored. This buffer is manipulated directly by the callback function.
         * @param callback A function pointer to the callback function that will be
         * invoked for each line of the file. This function is responsible for
         * processing the line and storing the result in dstBuffer.
         *
         * @return True if the file was successfully read and processed using memory
         * mapping, false otherwise. If false, the error parameter will contain
         * a description of the error.
         *
         * Note: This function is designed to work in a single-threaded context but
         * takes advantage of memory mapping for efficient file processing, making
         * it suitable for large files.
         */
        template <typename T>
        bool readWithMapping(const std::string &filename, std::string &error, T &dstBuffer,
                             void (*callback)(T &, const std::string_view &))
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

            try
            {
                const char *fileStart = fileData;
                const char *fileEnd = fileData + fileSize.QuadPart;
                const char *lineStart = fileStart;

                for (const char *p = fileData; p < fileEnd; ++p)
                {
                    if (*p == '\n' || *p == '\0')
                    {
                        callback(dstBuffer, std::string_view(lineStart, p - lineStart));
                        lineStart = p + 1;
                    }
                }

                // Обработка последней строки, если она не заканчивается новой строкой
                if (lineStart != fileEnd)
                    callback(dstBuffer, std::string_view(lineStart, fileEnd - lineStart));
            }
            catch (const std::exception &e)
            {
                error = "Failed to process file with error: " + std::string(e.what());
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
                const char *fileStart = fileData;
                const char *fileEnd = fileData + fileSize;
                const char *lineStart = fileStart;

                for (const char *p = fileStart; p < fileEnd; ++p)
                {
                    if (*p == '\n' || *p == '\0')
                    {
                        callback(dstBuffer, std::string_view(lineStart, p - lineStart));
                        lineStart = p + 1;
                    }
                }

                if (lineStart != fileEnd)
                    callback(dstBuffer, std::string_view(lineStart, fileEnd - lineStart));
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

            // Find the line boundaries and split the file into chunks
            const size_t threadCount = std::thread::hardware_concurrency();
            Array<size_t> blockIndices(threadCount + 1);
            Array<size_t> lineCounts(threadCount, 0);

            for (size_t i = 0; i < threadCount; ++i)
                blockIndices[i] = i * (fileSize.QuadPart / threadCount);
            blockIndices[threadCount] = fileSize.QuadPart;

            for (size_t i = 1; i < threadCount; ++i)
            {
                while (blockIndices[i] < fileSize.QuadPart && fileData[blockIndices[i]] != '\n')
                    ++blockIndices[i];
                ++blockIndices[i];
            }

            // Calculate the number of lines in each block
            for (size_t i = 0; i < threadCount; ++i)
            {
                const char *blockStart = fileData + blockIndices[i];
                const char *blockEnd = fileData + blockIndices[i + 1];

                for (const char *p = blockStart; p < blockEnd; ++p)
                    if (*p == '\n' || *p == '\0')
                        ++lineCounts[i];
            }

            // Calculate the starting line number for each block
            for (size_t i = 1; i < threadCount; ++i)
                lineCounts[i] += lineCounts[i - 1];

            // Parse the file in parallel
            oneapi::tbb::task_group_context tgc(oneapi::tbb::task_group_context::isolated);
            try
            {
                oneapi::tbb::parallel_for(
                    oneapi::tbb::blocked_range<size_t>(0, threadCount),
                    [&](const oneapi::tbb::blocked_range<size_t> &r) {
                        for (size_t i = r.begin(); i != r.end(); ++i)
                        {
                            size_t lineIndex = (i == 0 ? 0 : lineCounts[i - 1]);
                            const char *blockStart = fileData + blockIndices[i];
                            const char *blockEnd = fileData + blockIndices[i + 1];

                            const char *lineStart = blockStart;
                            const char *lineEnd = lineStart;

                            for (; lineEnd < blockEnd; ++lineEnd)
                            {
                                if (*lineEnd == '\n' || *lineEnd == '\0')
                                {
                                    callback(dstBuffer, std::string_view(lineStart, lineEnd - lineStart), lineIndex++);
                                    lineStart = lineEnd + 1;
                                }
                            }

                            if (lineStart != lineEnd)
                                callback(dstBuffer, std::string_view(lineStart, lineEnd - lineStart), lineIndex++);
                        }
                    },
                    tgc);
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

            // The remaining logic is largely the same, with adjustments made for differences between Windows and Linux
            const size_t threadCount = std::thread::hardware_concurrency();
            Array<size_t> blockIndices(threadCount + 1);
            Array<size_t> lineCounts(threadCount, 0);

            for (size_t i = 0; i < threadCount; ++i)
                blockIndices[i] = i * (fileSize / threadCount);
            blockIndices[threadCount] = fileSize;

            for (size_t i = 1; i < threadCount; ++i)
            {
                while (blockIndices[i] < fileSize && fileData[blockIndices[i]] != '\n')
                    ++blockIndices[i];
                ++blockIndices[i];
            }

            for (size_t i = 0; i < threadCount; ++i)
            {
                const char *blockStart = fileData + blockIndices[i];
                const char *blockEnd = fileData + blockIndices[i + 1];
                for (const char *p = blockStart; p < blockEnd; ++p)
                    if (*p == '\n' || *p == '\0')
                        ++lineCounts[i];
            }

            for (size_t i = 1; i < threadCount; ++i)
                lineCounts[i] += lineCounts[i - 1];

            oneapi::tbb::task_group_context tgc(oneapi::tbb::task_group_context::isolated);
            try
            {
                oneapi::tbb::parallel_for(
                    oneapi::tbb::blocked_range<size_t>(0, threadCount),
                    [&](const oneapi::tbb::blocked_range<size_t> &r) {
                        for (size_t i = r.begin(); i != r.end(); ++i)
                        {
                            size_t lineIndex = (i == 0 ? 0 : lineCounts[i - 1]);
                            const char *blockStart = fileData + blockIndices[i];
                            const char *blockEnd = fileData + blockIndices[i + 1];
                            const char *lineStart = blockStart;
                            const char *lineEnd = lineStart;

                            for (; lineEnd < blockEnd; ++lineEnd)
                            {
                                if (*lineEnd == '\n' || *lineEnd == '\0')
                                {
                                    callback(dstBuffer, std::string_view(lineStart, lineEnd - lineStart), lineIndex++);
                                    lineStart = lineEnd + 1;
                                }
                            }
                            if (lineStart != lineEnd)
                                callback(dstBuffer, std::string_view(lineStart, lineEnd - lineStart), lineIndex++);
                        }
                    },
                    tgc);
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
        bool writeFileByBlock(const std::string &filename, const char *buffer, size_t blockSize, std::string &error);

        /**
         * @brief std::getline function implementation providing support for Windows &
         * Unix line endings
         * @see
         * https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
         * @param is Input stream
         * @param t Dst std::string buffer
         * @return Current stream
         */
        std::istream &safeGetline(std::istream &is, std::string &t);

        /**
         * @brief Copy a file from source path to destination path.
         *
         * @param src The source path of the file to be copied.
         * @param dst The destination path where the file will be copied.
         * @param options The copy options to be applied during the copy operation.
         * @return true if the file is successfully copied, false otherwise.
         */
        bool copyFile(const std::filesystem::path &src, const std::filesystem::path &dst,
                      const std::filesystem::copy_options options);
    } // namespace file
} // namespace io

#endif