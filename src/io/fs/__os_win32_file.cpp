#include <acul/io/fs/file.hpp>
#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <cassert>

#include "directory_cursor.hpp"

namespace acul::fs::detail
{
    static bool read_target(const u16string &path, BY_HANDLE_FILE_INFORMATION &info)
    {
        const auto file = CreateFileW(reinterpret_cast<LPCWSTR>(path.c_str()), FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        const bool success = GetFileInformationByHandle(file, &info) != FALSE;
        const DWORD error = success ? ERROR_SUCCESS : GetLastError();
        CloseHandle(file);
        if (!success) SetLastError(error);
        return success;
    }

    directory_cursor::directory_cursor(directory_cursor &&other) noexcept
        : prefix(std::move(other.prefix)), volume(other.volume), id(other.id),
          handle(std::exchange(other.handle, INVALID_HANDLE_VALUE)), first(other.first), has_first(other.has_first)
    {
    }

    directory_cursor::~directory_cursor()
    {
        if (handle != INVALID_HANDLE_VALUE) FindClose(handle);
    }

    int directory_cursor::open(const string &path, bool identify)
    {
        if (path.empty()) return ERROR_PATH_NOT_FOUND;
        if (identify)
        {
            BY_HANDLE_FILE_INFORMATION info;
            if (!read_target(utf8_to_utf16(path), info)) return GetLastError();
            if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
            volume = info.dwVolumeSerialNumber;
            id = (static_cast<u64>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
        }
        prefix = path;
        const bool drive_relative = prefix.size() == 2 && prefix[1] == ':';
        if (!drive_relative && prefix.back() != '\\' && prefix.back() != '/') prefix += '\\';
        const auto pattern = utf8_to_utf16(prefix + '*');
        handle = FindFirstFileW(reinterpret_cast<LPCWSTR>(pattern.c_str()), &first);
        if (handle != INVALID_HANDLE_VALUE)
        {
            has_first = true;
            return 0;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) return error;
        const auto wide = utf8_to_utf16(path);
        const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(wide.c_str()));
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
        return error;
    }

    bool directory_cursor::next(string &path, directory_entry_type &type, bool follow_links)
    {
        if (handle == INVALID_HANDLE_VALUE) return false;
        WIN32_FIND_DATAW entry;
        while (has_first || FindNextFileW(handle, &entry))
        {
            if (has_first)
            {
                entry = first;
                has_first = false;
            }
            const auto *name = entry.cFileName;
            if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
            path = prefix + utf16_to_utf8(reinterpret_cast<const c16 *>(name));
            DWORD attributes = entry.dwFileAttributes;
            bool link = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            if (link && follow_links)
            {
                BY_HANDLE_FILE_INFORMATION info;
                if (!read_target(utf8_to_utf16(path), info)) continue;
                attributes = info.dwFileAttributes;
                link = false;
            }
            if (link || (attributes & FILE_ATTRIBUTE_DEVICE)) type = directory_entry_type::other;
            else if (attributes & FILE_ATTRIBUTE_DIRECTORY) type = directory_entry_type::directory;
            else type = directory_entry_type::regular;
            // Verbatim paths retain their native separators.
            if (strncmp(path.c_str(), R"(\\?\)", 4) != 0)
                for (char &c : path)
                    if (c == '\\') c = '/';
            return true;
        }
        return false;
    }
}

namespace acul::fs
{
    namespace
    {
        HANDLE open_lock_file(const string &filename)
        {
            if (filename.empty()) return INVALID_HANDLE_VALUE;
            const u16string w_filename = utf8_to_utf16(filename);
            return CreateFileW((LPCWSTR)w_filename.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        }

        bool try_lock(HANDLE handle)
        {
            OVERLAPPED overlapped{};
            return LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD,
                              &overlapped) != FALSE;
        }

        void unlock(HANDLE handle)
        {
            OVERLAPPED overlapped{};
            UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        }
    } // namespace

    bool is_directory(const char *path) noexcept
    {
        if (!path || !*path) return false;
        try
        {
            const u16string wide = utf8_to_utf16(path);
            const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(wide.c_str()));
            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
        }
        catch (...)
        {
            return false;
        }
    }

    fd_lock lock_file(const string &filename)
    {
        HANDLE handle = open_lock_file(filename);
        if (handle == INVALID_HANDLE_VALUE) return invalid_fd_lock;

        if (!try_lock(handle))
        {
            CloseHandle(handle);
            return invalid_fd_lock;
        }

        return static_cast<fd_lock>(reinterpret_cast<uintptr_t>(handle));
    }

    bool unlock_file(fd_lock id) noexcept
    {
        if (id == invalid_fd_lock || id == 0) return false;

        HANDLE handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(id));
        OVERLAPPED overlapped{};
        const bool unlocked = UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped) != FALSE;
        const bool closed = CloseHandle(handle) != FALSE;
        return unlocked && closed;
    }

    bool is_file_locked(const string &filename)
    {
        HANDLE handle = open_lock_file(filename);
        if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_SHARING_VIOLATION;

        if (try_lock(handle))
        {
            unlock(handle);
            CloseHandle(handle);
            return false;
        }

        const DWORD error = GetLastError();
        CloseHandle(handle);
        return error == ERROR_LOCK_VIOLATION || error == ERROR_SHARING_VIOLATION;
    }

    op_result write_by_block(const string &filename, const char *buffer, size_t block_size)
    {
        const u16string wide = utf8_to_utf16(filename);
        HANDLE file_handle = CreateFileW(reinterpret_cast<LPCWSTR>(wide.c_str()), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                         FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE) return make_op_error(ACUL_OP_WRITE_ERROR, GetLastError());

        size_t buffer_size = null_terminated_length(buffer);
        size_t num_blocks = (buffer_size + block_size - 1) / block_size;

        for (size_t block_index = 0; block_index < num_blocks; ++block_index)
        {
            size_t offset = block_index * block_size;
            size_t size = std::min(buffer_size - offset, block_size);

            DWORD bytes_written;
            if (!WriteFile(file_handle, buffer + offset, size, &bytes_written, NULL) || bytes_written < size)
            {
                DWORD err = GetLastError();
                CloseHandle(file_handle);
                return make_op_error(ACUL_OP_WRITE_ERROR, err);
            }
        }

        CloseHandle(file_handle);
        return make_op_success();
    }

    op_result copy_file(const char *src, const char *dst, bool overwrite) noexcept
    try
    {
        const u16string from = utf8_to_utf16(src), to = utf8_to_utf16(dst);
        if (CopyFileW(reinterpret_cast<LPCWSTR>(from.c_str()), reinterpret_cast<LPCWSTR>(to.c_str()), !overwrite))
            return make_op_success();
        DWORD err = GetLastError();
        if (!overwrite && err == ERROR_FILE_EXISTS)
            return op_result(ACUL_OP_SUCCESS, ACUL_OP_DOMAIN, ACUL_OP_CODE_SKIPPED);
        return make_op_error(ACUL_OP_WRITE_ERROR, err);
    }
    catch (...)
    {
        return make_op_error(ACUL_OP_INVALID_SIZE);
    }

    op_result create_directory(const char *path)
    {
        const u16string wide = utf8_to_utf16(path);
        if (CreateDirectoryW(reinterpret_cast<LPCWSTR>(wide.c_str()), NULL)) return make_op_success();
        else
        {
            DWORD error = GetLastError();
            if (error == ERROR_ALREADY_EXISTS)
            {
                DWORD dw_attrib = GetFileAttributesW(reinterpret_cast<LPCWSTR>(wide.c_str()));
                if (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY))
                    return op_result(ACUL_OP_SUCCESS, ACUL_OP_DOMAIN, ACUL_OP_CODE_SKIPPED);
            }
            return make_op_error(ACUL_OP_WRITE_ERROR, error);
        }
    }

    op_result remove_file(const char *path)
    {
        const u16string wide = utf8_to_utf16(path);
        return DeleteFileW(reinterpret_cast<LPCWSTR>(wide.c_str()))
                   ? make_op_success()
                   : make_op_error(ACUL_OP_DELETE_ERROR, GetLastError());
    }

    op_result replace_file(const string &source, const string &destination)
    {
        const u16string from = utf8_to_utf16(source), to = utf8_to_utf16(destination);
        return MoveFileExW(reinterpret_cast<LPCWSTR>(from.c_str()), reinterpret_cast<LPCWSTR>(to.c_str()),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
                   ? make_op_success()
                   : make_op_error(ACUL_OP_WRITE_ERROR, GetLastError());
    }

    op_result remove_directory(const char *path)
    {
        const u16string wide = utf8_to_utf16(path);
        return RemoveDirectoryW(reinterpret_cast<LPCWSTR>(wide.c_str()))
                   ? make_op_success()
                   : make_op_error(ACUL_OP_DELETE_ERROR, GetLastError());
    }

    op_result mapped_file::open(const string &filename)
    {
        close();
        const u16string name = utf8_to_utf16(filename);
        HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(name.c_str()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return make_op_error(ACUL_OP_READ_ERROR, GetLastError());
        LARGE_INTEGER length;
        if (!GetFileSizeEx(file, &length))
        {
            const DWORD error = GetLastError();
            CloseHandle(file);
            return make_op_error(ACUL_OP_INVALID_SIZE, error);
        }
        if (length.QuadPart < 0)
        {
            CloseHandle(file);
            return make_op_error(ACUL_OP_INVALID_SIZE);
        }
        if (length.QuadPart == 0)
        {
            CloseHandle(file);
            return make_op_success();
        }
        HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping)
        {
            const DWORD error = GetLastError();
            CloseHandle(file);
            return make_op_error(ACUL_OP_MAP_ERROR, error);
        }
        const auto *bytes = static_cast<const char *>(
            MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, static_cast<size_t>(length.QuadPart)));
        const DWORD error = bytes ? ERROR_SUCCESS : GetLastError();
        // The view retains the mapping until UnmapViewOfFile; these handles are no longer needed.
        CloseHandle(mapping);
        CloseHandle(file);
        if (!bytes) return make_op_error(ACUL_OP_MAP_ERROR, error);
        _data = bytes;
        _size = static_cast<size_t>(length.QuadPart);
        return make_op_success();
    }

    void mapped_file::close() noexcept
    {
        if (_data) UnmapViewOfFile(_data);
        _data = nullptr;
        _size = 0u;
    }

    op_result read_by_block(const string &filename, unique_function<void(char *, size_t)> callback)
    {
        mapped_file mapping;
        ACUL_TRY(mapping.open(filename));
        if (mapping.size() == 0u) return make_op_success();
        try
        {
            callback(const_cast<char *>(mapping.data()), mapping.size());
        }
        catch (...)
        {
            return make_op_error(ACUL_OP_ERROR_GENERIC);
        }
        return make_op_success();
    }
} // namespace acul::fs
