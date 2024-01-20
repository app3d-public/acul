#include <core/file/file.hpp>
#include <core/log.hpp>
#include <core/std/string.hpp>

namespace io
{
    namespace file
    {
        bool readBinary(const std::string &filename, Array<char> &buffer)
        {
#ifdef _WIN32
            std::u16string u16path = convertUTF8toUTF16(filename);
            std::wstring wpath(u16path.begin(), u16path.end());
            FILE *file = _wfopen(wpath.c_str(), L"rb");
#else
            FILE *file = fopen(filename.c_str(), "rb");
#endif

            if (!file)
            {
                logError("Failed to open file: " + filename);
                return false;
            }

            fseek(file, 0, SEEK_END);
            size_t fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            buffer.resize(fileSize);
            fread(buffer.data(), 1, fileSize, file);
            fclose(file);

            return true;
        }

        bool writeBinary(const std::string &filename, char *buffer, size_t size)
        {
            std::ofstream file(filename, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;

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

        std::istream &safeGetline(std::istream &is, std::string &t)
        {
            t.clear();

            // The characters in the stream are read one-by-one using a std::streambuf.
            // That is faster than reading them one-by-one using the std::istream.
            // Code that uses streambuf this way must be guarded by a sentry object.
            // The sentry object performs various tasks,
            // such as thread synchronization and updating the stream state.

            std::istream::sentry se(is, true);
            std::streambuf *sb = is.rdbuf();

            for (;;)
            {
                int c = sb->sbumpc();
                switch (c)
                {
                    case '\n':
                        return is;
                    case '\r':
                        if (sb->sgetc() == '\n')
                            sb->sbumpc();
                        return is;
                    case std::streambuf::traits_type::eof():
                        // Also handle the case when the last line has no line ending
                        if (t.empty())
                            is.setstate(std::ios::eofbit);
                        return is;
                    default:
                        t += (char)c;
                }
            }
        }

        bool copyFile(const std::filesystem::path &src, const std::filesystem::path &dst,
                      const std::filesystem::copy_options options)
        {
            try
            {
                std::filesystem::path targetDirPath = dst.parent_path();
                if (!std::filesystem::exists(targetDirPath))
                    std::filesystem::create_directories(targetDirPath);

                std::filesystem::copy(src, dst, options);
                return true;
            }
            catch (std::filesystem::filesystem_error &e)
            {
                logWarn("Failed to copy file: %s", std::string(e.what()));
                return false;
            }
            catch (const std::exception &e)
            {
                logWarn("Failed to copy file: %s", std::string(e.what()));
                return false;
            }
        }
    } // namespace file
} // namespace io