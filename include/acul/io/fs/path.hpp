#pragma once

#include "../detail/constants.hpp"
#include "../path.hpp"


namespace acul::fs
{
    inline string get_extension(const string &path)
    {
        const char *p = path.c_str();
        size_t len = path.size();
        size_t dot_pos = find_last_of(p, len, '.');
        size_t slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_UNIX);
        if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_WIN32);
        if (dot_pos == acul::string::npos || (slash_pos != acul::string::npos && dot_pos < slash_pos)) return {};
        return path.substr(dot_pos);
    }

    inline string get_filename(const string &path)
    {
        const char *p = path.c_str();
        size_t len = path.size();
        size_t slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_UNIX);
        if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_WIN32);
        return slash_pos == acul::string::npos ? path : path.substr(slash_pos + 1);
    }

    inline string replace_filename(const string &path, const string &new_filename)
    {
        const char *p = path.c_str();
        size_t len = path.size();
        size_t slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_UNIX);
        if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_WIN32);
        return slash_pos == acul::string::npos ? new_filename : path.substr(0, slash_pos + 1) + new_filename;
    }

    inline string replace_extension(const string &path, const string &new_extension)
    {
        const char *p = path.c_str();
        size_t len = path.size();
        size_t slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_UNIX);
        if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, ACUL_PATH_SEPARATOR_WIN32);
        size_t dot_pos = find_last_of(p, len, '.');
        if (dot_pos != acul::string::npos && (slash_pos == acul::string::npos || dot_pos > slash_pos))
            return path.substr(0, dot_pos) + new_extension;
        return path + new_extension;
    }

    inline bool get_executable_path(char *buffer, size_t buffer_size) noexcept
    {
#ifdef _WIN32
        if (!GetModuleFileNameA(NULL, buffer, static_cast<DWORD>(buffer_size))) return false;
#else
        ssize_t count = readlink("/proc/self/exe", buffer, buffer_size - 1);
        if (count == -1 || count >= static_cast<ssize_t>(buffer_size - 1)) return false;
        buffer[count] = '\0'; // Null-terminate
#endif
        return true;
    }

    inline bool get_current_path(char *buffer, size_t buffer_size) noexcept
    {
        if (!get_executable_path(buffer, buffer_size)) return false;
        size_t len = strnlen(buffer, buffer_size);
        size_t pos = find_last_of(buffer, len, ACUL_PATH_SEPARATOR);
        if (pos != string::npos) buffer[pos] = '\0';
        return true;
    }

    inline path get_current_path() noexcept
    {
        char buffer[PATH_MAX];
        if (!get_current_path(buffer, PATH_MAX)) return {};
        return (const char *)buffer;
    }

    APPLIB_API path get_module_directory() noexcept;
} // namespace acul::fs