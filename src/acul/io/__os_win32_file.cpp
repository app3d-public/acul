#include <acul/io/file.hpp>
#include <acul/log.hpp>
#include <cassert>

namespace acul
{
    namespace io
    {
        namespace file
        {
            bool write_by_block(const string &filename, const char *buffer, size_t blockSize, string &error)
            {
                HANDLE fileHandle =
                    CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (fileHandle == INVALID_HANDLE_VALUE)
                {
                    error = "Failed to open file: " + filename;
                    return false;
                }

                size_t bufferSize = strlen(buffer);
                size_t numBlocks = (bufferSize + blockSize - 1) / blockSize;

                for (size_t blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
                {
                    size_t offset = blockIndex * blockSize;
                    size_t size = std::min(bufferSize - offset, blockSize);

                    DWORD bytesWritten;
                    if (!WriteFile(fileHandle, buffer + offset, size, &bytesWritten, NULL) || bytesWritten < size)
                    {
                        CloseHandle(fileHandle);
                        error = "Failed to write block to file: " + filename;
                        return false;
                    }
                }

                CloseHandle(fileHandle);
                return true;
            }

            op_state copy(const char *src, const char *dst, bool overwrite) noexcept
            {
                HANDLE hSrc = CreateFileA(src, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL, NULL);
                if (hSrc == INVALID_HANDLE_VALUE) return op_state::Error;

                DWORD creation = overwrite ? CREATE_ALWAYS : CREATE_NEW;
                HANDLE hDst = CreateFileA(dst, GENERIC_WRITE, 0, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hDst == INVALID_HANDLE_VALUE)
                {
                    DWORD err = GetLastError();
                    CloseHandle(hSrc);

                    if (!overwrite && err == ERROR_FILE_EXISTS) return op_state::SkippedExisting;

                    return op_state::Error;
                }

                char buffer[4096];
                DWORD bytesRead, bytesWritten;
                bool copySuccess = true;

                while (ReadFile(hSrc, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
                {
                    if (!WriteFile(hDst, buffer, bytesRead, &bytesWritten, NULL) || bytesRead != bytesWritten)
                    {
                        copySuccess = false;
                        break;
                    }
                }

                CloseHandle(hSrc);
                CloseHandle(hDst);

                return copySuccess ? op_state::Success : op_state::Error;
            }

            op_state create_directory(const char *path)
            {
                if (CreateDirectoryA(path, NULL))
                    return op_state::Success;
                else
                {
                    DWORD error = GetLastError();
                    if (error == ERROR_ALREADY_EXISTS) return op_state::SkippedExisting;
                    LOG_ERROR("Failed to create directory %s. Error code: %lu", path, error);
                    return op_state::Error;
                }
            }

            op_state remove_file(const char *path)
            {
                if (DeleteFileA(path))
                    return op_state::Success;
                else
                {
                    DWORD error = GetLastError();
                    LOG_ERROR("Failed to remove file %s. Error code: %lu", path, error);
                    return op_state::Error;
                }
            }

            op_state list_files(const string &base_path, vector<string> &dst, bool recursive)
            {
                assert(!base_path.empty() && "base_path is null");
                u16string search_path = utf8_to_utf16(base_path + "\\*");
                WIN32_FIND_DATAW find_data;
                HANDLE handle = FindFirstFileW((LPCWSTR)search_path.c_str(), &find_data);
                if (handle == INVALID_HANDLE_VALUE)
                {
                    LOG_ERROR("Failed to open directory: %s. Error code: %lu", base_path.c_str(), GetLastError());
                    return op_state::Error;
                }
                do {
                    const wchar_t *name = find_data.cFileName;
                    if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
                    string full_path = base_path + '/' + utf16_to_utf8((const c16 *)name);
                    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        if (recursive) list_files(full_path, dst, true);
                    }
                    else
                        dst.push_back(full_path);

                } while (FindNextFileW(handle, &find_data) != 0);
                FindClose(handle);
                return op_state::Success;
            }

            op_state read_by_block(const string &filename, const std::function<void(char *, size_t)> &callback)
            {
                u16string w_filename = utf8_to_utf16(filename);
                HANDLE file_handle = CreateFileW((LPCWSTR)w_filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (file_handle == INVALID_HANDLE_VALUE) return op_state::Error;

                LARGE_INTEGER file_size;
                if (!GetFileSizeEx(file_handle, &file_size))
                {
                    CloseHandle(file_handle);
                    return op_state::Error;
                }

                HANDLE mapping_handle = CreateFileMapping(file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
                if (mapping_handle == NULL)
                {
                    CloseHandle(file_handle);
                    return op_state::Error;
                }

                char *file_data =
                    static_cast<char *>(MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, file_size.QuadPart));
                if (file_data == NULL)
                {
                    CloseHandle(mapping_handle);
                    CloseHandle(file_handle);
                    return op_state::Error;
                }

                // Parse the file in parallel
                op_state res = op_state::Success;
                try
                {
                    callback(file_data, file_size.QuadPart);
                }
                catch (...)
                {
                    res = op_state::Error;
                }

                // Unmap the file from memory
                UnmapViewOfFile(file_data);
                CloseHandle(mapping_handle);
                CloseHandle(file_handle);
                return res;
            }
        } // namespace file
    } // namespace io
} // namespace acul