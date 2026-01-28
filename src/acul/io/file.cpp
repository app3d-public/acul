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
            bool read_binary(const string &filename, vector<char> &buffer)
            {
                FILE *file = fopen(filename.c_str(), "rb");
                if (!file) return false;

                fseek(file, 0, SEEK_END);
                size_t file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                buffer.resize(file_size);
                fread(buffer.data(), 1, file_size, file);
                fclose(file);

                return true;
            }

            bool read_virtual(const string &filename, vector<char> &buffer)
            {
                FILE *file = fopen(filename.c_str(), "rb");
                if (!file) return false;

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
                            return false;
                        }
                    }
                }

                fclose(file);
                return true;
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
            op_result compress(const char *data, size_t size, vector<char> &compressed, int quality)
            {
                size_t const max_compressed_size = ZSTD_compressBound(size);
                compressed.resize(max_compressed_size);

                size_t const compressed_size =
                    ZSTD_compress(compressed.data(), max_compressed_size, data, size, quality);

                if (ZSTD_isError(compressed_size))
                {
                    compressed.clear();
                    return make_op_error(ACUL_OP_COMPRESS_ERROR, ZSTD_getErrorCode(compressed_size));
                }

                compressed.resize(compressed_size);
                return make_op_success();
            }

            op_result decompress(const char *data, size_t size, vector<char> &decompressed)
            {
                size_t decompressed_size = ZSTD_getFrameContentSize(data, size);
                if (decompressed_size == 0) return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_CODE_SIZE_ZERO);
                if (decompressed_size == ZSTD_CONTENTSIZE_ERROR)
                    return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_CODE_SIZE_ERROR);
                if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN)
                    return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_CODE_SIZE_UNKNOWN);

                decompressed.resize(decompressed_size);

                size_t const actual_decompressed_size =
                    ZSTD_decompress(decompressed.data(), decompressed_size, data, size);

                if (ZSTD_isError(actual_decompressed_size))
                {
                    decompressed.clear();
                    return make_op_error(ACUL_OP_DECOMPRESS_ERROR, ZSTD_getErrorCode(actual_decompressed_size));
                }

                return make_op_success();
            }
#endif

            namespace internal
            {
                namespace nosimd
                {
                    APPLIB_API void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst)
                    {
                        const char *p = data;
                        const char *end = data + size;
                        const char *line_start = p;

                        while (p < end)
                        {
                            if (*p == '\n')
                            {
                                size_t line_len = p - line_start;
                                if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
                                dst.push(line_start, line_len);
                                ++p;
                                line_start = p;
                            }
                            else
                            {
                                ++p;
                            }
                        }

                        if (line_start < end)
                        {
                            size_t line_len = end - line_start;
                            if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
                            dst.push(line_start, line_len);
                        }
                    }
                } // namespace nosimd
            } // namespace internal
        } // namespace file
    } // namespace io
} // namespace acul
