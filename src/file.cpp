#include <core/file/file.hpp>
#include <core/log.hpp>
#include <core/std/string.hpp>
#include <oneapi/tbb/enumerable_thread_specific.h>

namespace io
{
    namespace file
    {
        bool readBinary(const std::string &filename, DArray<char> &buffer)
        {
            FILE *file = fopen(filename.c_str(), "rb");
            if (!file)
            {
                if (!std::filesystem::exists(filename))
                    logError("File does not exist: " + filename);
                else
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

        void fillLineBuffer(const char *data, size_t size, DArray<std::string_view> &dst)
        {
            size_t threadCount = tbb::this_task_arena::max_concurrency();
            DArray<size_t> blockIndices(threadCount + 1);
            DArray<size_t> lineCounts(threadCount, 0);

            for (size_t i = 0; i < threadCount; ++i) { blockIndices[i] = i * (size / threadCount); }
            blockIndices[threadCount] = size;

            // Adjust each block to start at the beginning of a line
            for (size_t i = 1; i < threadCount; ++i)
            {
                while (blockIndices[i] < size && data[blockIndices[i]] != '\n' && data[blockIndices[i]] != '\0')
                    ++blockIndices[i];
                if (blockIndices[i] < size) ++blockIndices[i]; // move past the newline
            }

            oneapi::tbb::enumerable_thread_specific<DArray<std::string_view>> local_results;
            oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<size_t>(0, threadCount),
                                      [&](const tbb::blocked_range<size_t> &r) {
                                          auto &local_dst = local_results.local();
                                          for (size_t i = r.begin(); i != r.end(); ++i)
                                          {
                                              const char *blockStart = data + blockIndices[i];
                                              const char *blockEnd = data + blockIndices[i + 1];
                                              const char *lineStart = blockStart;
                                              for (const char *p = blockStart; p < blockEnd; ++p)
                                              {
                                                  if (*p == '\n' || *p == '\0')
                                                  {
                                                      local_dst.emplace_back(lineStart, p - lineStart);
                                                      lineStart = p + 1;
                                                  }
                                              }
                                              // Handle the last line in the block
                                              if (lineStart < blockEnd)
                                                  local_dst.emplace_back(lineStart, blockEnd - lineStart);
                                          }
                                      });

            // Combine all local results into the main vector
            for (auto &local_vec : local_results) dst.insert(dst.end(), local_vec.begin(), local_vec.end());
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
                        if (sb->sgetc() == '\n') sb->sbumpc();
                        return is;
                    case std::streambuf::traits_type::eof():
                        // Also handle the case when the last line has no line ending
                        if (t.empty()) is.setstate(std::ios::eofbit);
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
                if (!std::filesystem::exists(targetDirPath)) std::filesystem::create_directories(targetDirPath);

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