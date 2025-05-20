#include <acul/io/file.hpp>
#include <acul/log.hpp>
#include <zstd.h>

#define FILE_READ_STREAM_CHUNK_SIZE 4096

namespace acul
{
    namespace io
    {
        namespace file
        {
            op_state read_binary(const string &filename, vector<char> &buffer)
            {
                FILE *file = fopen(filename.c_str(), "rb");
                if (!file)
                {
                    if (!exists(filename.c_str()))
                        LOG_ERROR("File does not exist: %s", filename.c_str());
                    else
                        LOG_ERROR("Failed to open file: %s", filename.c_str());
                    return op_state::Error;
                }

                fseek(file, 0, SEEK_END);
                size_t file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                buffer.resize(file_size);
                fread(buffer.data(), 1, file_size, file);
                fclose(file);

                return op_state::Success;
            }

            op_state read_virtual(const string &filename, vector<char> &buffer)
            {
                FILE *file = fopen(filename.c_str(), "rb");
                if (!file)
                {
                    if (!exists(filename.c_str()))
                        LOG_ERROR("File does not exist: %s\n", filename.c_str());
                    else
                        LOG_ERROR("Failed to open file: %s\n", filename.c_str());
                    return op_state::Error;
                }

                char chunk[FILE_READ_STREAM_CHUNK_SIZE];

                while (true)
                {
                    size_t n = fread(chunk, 1, FILE_READ_STREAM_CHUNK_SIZE, file);
                    if (n > 0) buffer.insert(buffer.end(), chunk, chunk + n);

                    if (n < FILE_READ_STREAM_CHUNK_SIZE)
                    {
                        if (feof(file)) break;
                        if (ferror(file))
                        {
                            fclose(file);
                            return op_state::Error;
                        }
                    }
                }

                fclose(file);
                return op_state::Success;
            }

            bool write_binary(const string &filename, const char *buffer, size_t size)
            {
                std::ofstream file(filename.c_str(), std::ios::binary | std::ios::trunc);
                if (!file) return false;

                file.write(buffer, size);
                file.close();
                return file.good();
            }

            bool compress(const char *data, size_t size, vector<char> &compressed, int quality)
            {
                size_t const maxCompressedSize = ZSTD_compressBound(size);
                compressed.resize(maxCompressedSize);

                size_t const compressedSize = ZSTD_compress(compressed.data(), maxCompressedSize, data, size, quality);

                if (ZSTD_isError(compressedSize))
                {
                    compressed.clear();
                    LOG_ERROR("Failed to compress: %s", ZSTD_getErrorName(compressedSize));
                    return false;
                }

                compressed.resize(compressedSize);
                return true;
            }

            bool decompress(const char *data, size_t size, vector<char> &decompressed)
            {
                size_t decompressedSize = ZSTD_getFrameContentSize(data, size);
                if (decompressedSize == 0 || decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
                    decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
                {
                    LOG_ERROR("Cannot determine decompressed size.");
                    return false;
                }

                decompressed.resize(decompressedSize);

                size_t const actualDecompressedSize =
                    ZSTD_decompress(decompressed.data(), decompressedSize, data, size);

                if (ZSTD_isError(actualDecompressedSize))
                {
                    decompressed.clear();
                    LOG_ERROR("Failed to decompress: %s", ZSTD_getErrorName(actualDecompressedSize));
                    return false;
                }

                return true;
            }
        } // namespace file
    } // namespace io
} // namespace acul
