#include <core/log.hpp>
#include <io/file.hpp>
#include <zstd.h>

namespace io
{
    namespace file
    {
        ReadState readBinary(const std::string &filename, astl::vector<char> &buffer)
        {
            FILE *file = fopen(filename.c_str(), "rb");
            if (!file)
            {
                if (!std::filesystem::exists(filename))
                    logError("File does not exist: %s", filename.c_str());
                else
                    logError("Failed to open file: %s", filename.c_str());
                return ReadState::Error;
            }

            fseek(file, 0, SEEK_END);
            size_t fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            buffer.resize(fileSize);
            fread(buffer.data(), 1, fileSize, file);
            fclose(file);

            return ReadState::Success;
        }

        bool writeBinary(const std::string &filename, const char *buffer, size_t size)
        {
            std::ofstream file(filename, std::ios::binary | std::ios::trunc);
            if (!file) return false;

            file.write(buffer, size);
            file.close();
            return file.good();
        }

        bool writeFileByBlock(const std::string &filename, const char *buffer, size_t blockSize, std::string &error)
        {
#ifdef _WIN32
            HANDLE fileHandle =
                CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fileHandle == INVALID_HANDLE_VALUE)
            {
                error = "Failed to open file: " + filename;
                return false;
            }

            size_t bufferSize = strlen(buffer);
            size_t numBlocks = (bufferSize + blockSize - 1) / blockSize; // Определяем количество блоков

            for (size_t blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
            {
                size_t offset = blockIndex * blockSize;
                size_t size = std::min(bufferSize - offset, blockSize); // Определяем размер блока

                DWORD bytesWritten;
                if (!WriteFile(fileHandle, buffer + offset, size, &bytesWritten, NULL) || bytesWritten < size)
                {
                    CloseHandle(fileHandle);
                    error = "Failed to write block to file: " + filename;
                    return false;
                }
            }

            CloseHandle(fileHandle);
#else
            int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd == -1)
            {
                error = "Failed to open file: " + filename + " " + strerror(errno);
                return false;
            }

            size_t bufferSize = strlen(buffer);
            size_t numBlocks = (bufferSize + blockSize - 1) / blockSize; // Determine the number of blocks

            for (size_t blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
            {
                size_t offset = blockIndex * blockSize;
                size_t size = std::min(bufferSize - offset, blockSize); // Determine the block size

                ssize_t bytesWritten = write(fd, buffer + offset, size);
                if (bytesWritten == -1 || static_cast<size_t>(bytesWritten) < size)
                {
                    close(fd);
                    error = "Failed to write block to file: " + filename + " " + strerror(errno);
                    return false;
                }
            }

            close(fd);
#endif
            return true;
        }

        bool copyFile(const std::filesystem::path &src, const std::filesystem::path &dst,
                      const std::filesystem::copy_options options)
        {
            try
            {
                std::filesystem::path targetDirPath = dst.parent_path();
                if (!std::filesystem::exists(targetDirPath)) std::filesystem::create_directories(targetDirPath);

                std::filesystem::copy(src, dst, options);
                return true;
            }
            catch (std::filesystem::filesystem_error &e)
            {
                logWarn("Failed to copy file: %s", e.what());
                return false;
            }
            catch (const std::exception &e)
            {
                logWarn("Failed to copy file: %s", e.what());
                return false;
            }
        }

        bool compress(const char *data, size_t size, astl::vector<char> &compressed, int quality)
        {
            size_t const maxCompressedSize = ZSTD_compressBound(size);
            compressed.resize(maxCompressedSize);

            size_t const compressedSize = ZSTD_compress(compressed.data(), maxCompressedSize, data, size, quality);

            if (ZSTD_isError(compressedSize))
            {
                compressed.clear();
                logError("Failed to compress: %s", ZSTD_getErrorName(compressedSize));
                return false;
            }

            compressed.resize(compressedSize);
            return true;
        }

        bool decompress(const char *data, size_t size, astl::vector<char> &decompressed)
        {
            size_t decompressedSize = ZSTD_getFrameContentSize(data, size);
            if (decompressedSize == 0 || decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
                decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
            {
                logError("Cannot determine decompressed size.");
                return false;
            }

            decompressed.resize(decompressedSize);

            size_t const actualDecompressedSize = ZSTD_decompress(decompressed.data(), decompressedSize, data, size);

            if (ZSTD_isError(actualDecompressedSize))
            {
                decompressed.clear();
                logError("Failed to decompress: %s", ZSTD_getErrorName(actualDecompressedSize));
                return false;
            }

            return true;
        }
    } // namespace file
} // namespace io