#include <acul/io/file.hpp>
#include <acul/log.hpp>
#include <cassert>

namespace acul
{
    namespace io
    {
        namespace file
        {
            bool write_by_block(const string &filename, const char *buffer, size_t blockSize, acul::string &error)
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
                if (hSrc == INVALID_HANDLE_VALUE) return op_state::error;

                DWORD creation = overwrite ? CREATE_ALWAYS : CREATE_NEW;
                HANDLE hDst = CreateFileA(dst, GENERIC_WRITE, 0, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hDst == INVALID_HANDLE_VALUE)
                {
                    DWORD err = GetLastError();
                    CloseHandle(hSrc);

                    if (!overwrite && err == ERROR_FILE_EXISTS) return op_state::skipped_existing;

                    return op_state::error;
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

                return copySuccess ? op_state::success : op_state::error;
            }

            op_state create_directory(const char *path)
            {
                if (CreateDirectoryA(path, NULL))
                    return op_state::success;
                else
                {
                    DWORD error = GetLastError();
                    if (error == ERROR_ALREADY_EXISTS) return op_state::skipped_existing;
                    logError("Failed to create directory %s. Error code: %lu", path, error);
                    return op_state::error;
                }
            }

            op_state list_files(const acul::string &base_path, acul::vector<acul::string> &dst, bool recursive)
            {
                assert(!base_path.empty() && "base_path is null");
                acul::u16string search_path = acul::utf8_to_utf16(base_path + "\\*");
                WIN32_FIND_DATAW findData;
                HANDLE handle = FindFirstFileW((LPCWSTR)search_path.c_str(), &findData);
                if (handle == INVALID_HANDLE_VALUE)
                {
                    logError("Failed to open directory: %s. Error code: %lu", base_path.c_str(), GetLastError());
                    return op_state::error;
                }
                do {
                    const wchar_t *name = findData.cFileName;
                    if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
                    acul::string fullPath = base_path + '/' + acul::utf16_to_utf8((const c16 *)name);
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        if (recursive) list_files(fullPath, dst, true);
                    }
                    else
                        dst.push_back(fullPath);

                } while (FindNextFileW(handle, &findData) != 0);
                FindClose(handle);
                return op_state::success;
            }
        } // namespace file
    } // namespace io
} // namespace acul