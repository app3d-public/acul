#include <acul/io/file.hpp>
#include <acul/log.hpp>
#include <zstd.h>

namespace acul
{
    namespace io
    {
        namespace file
        {
            op_state read_binary(const acul::string &filename, acul::vector<char> &buffer)
            {
                FILE *file = fopen(filename.c_str(), "rb");
                if (!file)
                {
                    if (!exists(filename.c_str()))
                        logError("File does not exist: %s", filename.c_str());
                    else
                        logError("Failed to open file: %s", filename.c_str());
                    return op_state::error;
                }

                fseek(file, 0, SEEK_END);
                size_t fileSize = ftell(file);
                fseek(file, 0, SEEK_SET);

                buffer.resize(fileSize);
                fread(buffer.data(), 1, fileSize, file);
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

            bool compress(const char *data, size_t size, acul::vector<char> &compressed, int quality)
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

            bool decompress(const char *data, size_t size, acul::vector<char> &decompressed)
            {
                size_t decompressedSize = ZSTD_getFrameContentSize(data, size);
                if (decompressedSize == 0 || decompressedSize == ZSTD_CONTENTSIZE_ERROR ||
                    decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
                {
                    logError("Cannot determine decompressed size.");
                    return false;
                }

                decompressed.resize(decompressedSize);

                size_t const actualDecompressedSize =
                    ZSTD_decompress(decompressed.data(), decompressedSize, data, size);

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
} // namespace acul
