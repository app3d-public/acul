#include <acul/io/fs/file.hpp>
#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <cassert>

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
        u16string w_path = utf8_to_utf16(path);
        DWORD file_attr = GetFileAttributesW((LPCWSTR)w_path.c_str());
        return file_attr != INVALID_FILE_ATTRIBUTES && (file_attr & FILE_ATTRIBUTE_DIRECTORY);
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
        HANDLE file_handle =
            CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
    {
        if (CopyFileA(src, dst, !overwrite)) return make_op_success();
        DWORD err = GetLastError();
        if (!overwrite && err == ERROR_FILE_EXISTS)
            return op_result(ACUL_OP_SUCCESS, ACUL_OP_DOMAIN, ACUL_OP_CODE_SKIPPED);
        return make_op_error(ACUL_OP_WRITE_ERROR, err);
    }

    op_result create_directory(const char *path)
    {
        if (CreateDirectoryA(path, NULL)) return make_op_success();
        else
        {
            DWORD error = GetLastError();
            if (error == ERROR_ALREADY_EXISTS)
            {
                DWORD dw_attrib = GetFileAttributesA(path);
                if (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY))
                    return op_result(ACUL_OP_SUCCESS, ACUL_OP_DOMAIN, ACUL_OP_CODE_SKIPPED);
            }
            return make_op_error(ACUL_OP_WRITE_ERROR, error);
        }
    }

    op_result remove_file(const char *path)
    {
        return DeleteFileA(path) ? make_op_success() : make_op_error(ACUL_OP_DELETE_ERROR, GetLastError());
    }

    op_result remove_directory(const char *path)
    {
        return RemoveDirectoryA(path) ? make_op_success() : make_op_error(ACUL_OP_DELETE_ERROR, GetLastError());
    }

    op_result list_files(const string &base_path, vector<string> &dst, bool recursive)
    {
        assert(!base_path.empty() && "base_path is null");
        u16string search_path = utf8_to_utf16(base_path + "\\*");
        WIN32_FIND_DATAW find_data;
        HANDLE handle = FindFirstFileW((LPCWSTR)search_path.c_str(), &find_data);
        if (handle == INVALID_HANDLE_VALUE) return make_op_error(ACUL_OP_READ_ERROR, GetLastError());
        do
        {
            const wchar_t *name = find_data.cFileName;
            if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
            string full_path = base_path + '/' + utf16_to_utf8((const c16 *)name);
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (recursive) list_files(full_path, dst, true);
            }
            else dst.push_back(full_path);

        } while (FindNextFileW(handle, &find_data) != 0);
        FindClose(handle);
        return make_op_success();
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
        const auto *bytes = static_cast<const char *>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0,
                                                                   static_cast<size_t>(length.QuadPart)));
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
