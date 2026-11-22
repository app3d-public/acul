#include <acul/io/fs/file.hpp>
#include <acul/io/path.hpp>
#include <acul/log.hpp>
#include <acul/memory/smart_ptr.hpp>
#include <zstd.h>
#include <zstd_errors.h>
#include "directory_cursor.hpp"

#define FILE_READ_STREAM_CHUNK_SIZE 4096

namespace acul::fs
{
    struct file_iterator::traversal
    {
        file_search_flags options;
        vector<detail::directory_cursor> stack;

        int open(const string &path)
        {
            const bool identify =
                (options & file_search_flag_bits::recursive) && (options & file_search_flag_bits::follow_symlinks);
            detail::directory_cursor directory;
            if (const int error = directory.open(path, identify)) return error;
            if (identify)
                for (const auto &parent : stack)
                    if (parent.volume == directory.volume && parent.id == directory.id) return 0;
            stack.push_back(std::move(directory));
            return 0;
        }
    };

    file_iterator list_files(const string &base_path, file_search_flags flags)
    {
        return file_iterator(base_path, flags);
    }

    file_iterator::file_iterator(const string &base_path, file_search_flags options)
    {
        auto walk = acul::make_unique<traversal>();
        walk->options = options;
        if (const int error = walk->open(base_path))
        {
            _result = make_op_error(ACUL_OP_READ_ERROR, error);
            return;
        }
        _walk = walk.get();
        ++*this;
        walk.release();
    }

    file_iterator::~file_iterator()
    {
        if (_walk) acul::release(_walk);
    }

    file_iterator &file_iterator::operator++()
    {
        _current.clear();
        if (!_walk) return *this;
        const auto options = _walk->options;
        const bool follow_links = bool(options & file_search_flag_bits::follow_symlinks);
        string path;
        while (!_walk->stack.empty())
        {
            detail::directory_entry_type type;
            if (!_walk->stack.back().next(path, type, follow_links))
            {
                _walk->stack.pop_back();
                continue;
            }
            if (type == detail::directory_entry_type::directory)
            {
                if (options & file_search_flag_bits::recursive) _walk->open(path);
                if (!(options & file_search_flag_bits::include_directories)) continue;
            }
            else if ((options & file_search_flag_bits::regular_files_only) &&
                     type != detail::directory_entry_type::regular)
                continue;
            _current = std::move(path);
            break;
        }
        return *this;
    }

    size_t read_binary_fd(const string &filename, FILE *&fd)
    {
#ifdef _WIN32
        const u16string wide = utf8_to_utf16(filename);
        fd = _wfopen(reinterpret_cast<LPCWSTR>(wide.c_str()), L"rb");
#else
        fd = fopen(filename.c_str(), "rb");
#endif
        if (!fd) return 0;
        if (fs::fseek(fd, 0, SEEK_END) != 0)
        {
            fclose(fd);
            fd = nullptr;
            return 0;
        }
        const i64 file_size = fs::ftell(fd);
        if (file_size < 0 || fs::fseek(fd, 0, SEEK_SET) != 0)
        {
            fclose(fd);
            fd = nullptr;
            return 0;
        }
        return static_cast<size_t>(file_size);
    }

    bool read_binary(const string &filename, vector<char> &buffer)
    {
        FILE *fd;
        size_t file_size = read_binary_fd(filename, fd);
        if (file_size == 0)
        {
            if (fd) fclose(fd);
            return false;
        }

        buffer.resize(file_size);
        if (fread(buffer.data(), 1, file_size, fd) != file_size)
        {
            fclose(fd);
            return false;
        }
        fclose(fd);
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
#ifdef _WIN32
        const u16string wide = utf8_to_utf16(filename);
        FILE *file = _wfopen(reinterpret_cast<LPCWSTR>(wide.c_str()), L"wb");
#else
        FILE *file = fopen(filename.c_str(), "wb");
#endif
        if (!file) return false;
        const bool written = fwrite(buffer, 1, size, file) == size && fflush(file) == 0;
        return fclose(file) == 0 && written;
    }

    op_result create_directories(const string &name)
    {
        if (name.empty()) return make_op_error(ACUL_OP_INVALID_SIZE);
        if (is_directory(name.c_str())) return make_op_success();
        const path parent = path(name).parent_path();
        if (!parent.empty() && parent.str() != name) ACUL_TRY(create_directories(parent.str()));
        return create_directory(name.c_str());
    }

#ifdef ACUL_ZSTD_ENABLE
    op_result compress(const char *data, size_t size, vector<char> &compressed, int quality)
    {
        size_t const max_compressed_size = ZSTD_compressBound(size);
        compressed.resize(max_compressed_size);

        size_t const compressed_size = ZSTD_compress(compressed.data(), max_compressed_size, data, size, quality);

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
        if (decompressed_size == 0) return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_OP_CODE_SIZE_ZERO);
        if (decompressed_size == ZSTD_CONTENTSIZE_ERROR)
            return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_OP_CODE_SIZE_ERROR);
        if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN)
            return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_OP_CODE_SIZE_UNKNOWN);

        decompressed.resize(decompressed_size);

        size_t const actual_decompressed_size = ZSTD_decompress(decompressed.data(), decompressed_size, data, size);

        if (ZSTD_isError(actual_decompressed_size))
        {
            decompressed.clear();
            return make_op_error(ACUL_OP_DECOMPRESS_ERROR, ZSTD_getErrorCode(actual_decompressed_size));
        }

        return make_op_success();
    }
#endif

} // namespace acul::fs
