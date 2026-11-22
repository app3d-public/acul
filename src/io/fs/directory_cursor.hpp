#pragma once

#include <acul/io/fs/file.hpp>
#ifndef _WIN32
    #include <dirent.h>
#endif

namespace acul::fs::detail
{
    enum class directory_entry_type
    {
        directory,
        regular,
        other
    };

    struct directory_cursor
    {
        string prefix;
        u64 volume = 0;
        u64 id = 0;
#ifdef _WIN32
        HANDLE handle = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATAW first{};
        bool has_first = false;
#else
        DIR *handle = nullptr;
#endif

        directory_cursor() = default;
        directory_cursor(directory_cursor &&other) noexcept;
        ~directory_cursor();

        int open(const string &path, bool identify);
        bool next(string &path, directory_entry_type &type, bool follow_links);
    };
} // namespace acul::fs::detail
