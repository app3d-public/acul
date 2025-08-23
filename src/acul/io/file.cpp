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
                if (!file) return op_state::error;

                fseek(file, 0, SEEK_END);
                size_t file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                buffer.resize(file_size);
                fread(buffer.data(), 1, file_size, file);
                fclose(file);

                return op_state::success;
            }

            op_state read_virtual(const string &filename, vector<char> &buffer)
            {
                FILE *file = fopen(filename.c_str(), "rb");
                if (!file) return op_state::error;

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
                            return op_state::error;
                        }
                    }
                }

                fclose(file);
                return op_state::success;
            }

            bool write_binary(const string &filename, const char *buffer, size_t size)
            {
                std::ofstream file(filename.c_str(), std::ios::binary | std::ios::trunc);
                if (!file) return false;

                file.write(buffer, size);
                file.close();
                return file.good();
            }

#ifdef ACUL_ZSTD_ENABLE
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
                size_t decompressed_size = ZSTD_getFrameContentSize(data, size);
                if (decompressed_size == 0 || decompressed_size == ZSTD_CONTENTSIZE_ERROR ||
                    decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN)
                {
                    LOG_ERROR("Cannot determine decompressed size.");
                    return false;
                }

                decompressed.resize(decompressed_size);

                size_t const actual_decompressed_size =
                    ZSTD_decompress(decompressed.data(), decompressed_size, data, size);

                if (ZSTD_isError(actual_decompressed_size))
                {
                    decompressed.clear();
                    LOG_ERROR("Failed to decompress: %s", ZSTD_getErrorName(actual_decompressed_size));
                    return false;
                }

                return true;
            }
#endif
        } // namespace file
    } // namespace io
} // namespace acul
