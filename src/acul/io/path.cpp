#include <acul/io/path.hpp>
#ifdef _WIN32
    #include <acul/string/utils.hpp>
#else
    #include <dlfcn.h>
#endif

#define IS_SEPARATOR(c) (c == PATH_CHAR_SEP_WIN32 || c == PATH_CHAR_SEP_UNIX)

namespace acul
{
    namespace io
    {
        inline const char *get_separator(const char *start, const char *end)
        {
            const char *sep_unix = (const char *)memchr(start, PATH_CHAR_SEP_UNIX, end - start);
            const char *sep_win32 = (const char *)memchr(start, PATH_CHAR_SEP_WIN32, end - start);

            if (!sep_unix) return sep_win32;
            if (!sep_win32) return sep_unix;
            return (sep_unix < sep_win32) ? sep_unix : sep_win32;
        }

        path path::operator/(const path &other) const
        {
            if (other._nodes.empty()) return *this;
            if (_nodes.empty()) return other;
            path p = *this;
            p._path.clear();
            size_t start_idx = 0;
            for (; start_idx < other._nodes.size(); ++start_idx)
            {
                if (other._nodes[start_idx] == ".." && !p._nodes.empty())
                    p._nodes.pop_back();
                else
                    break;
            }
            p._nodes.insert(p._nodes.end(), other._nodes.begin() + start_idx, other._nodes.end());
            return p;
        }

        void path::begin_parse(const char *&start, const char *end)
        {
            if (IS_SEPARATOR(start[0]))
            {
                if (start[0] == PATH_CHAR_SEP_WIN32) _flags |= FLAG_WIN32;
                ++start;
                _flags |= FLAG_ABS;
                if (IS_SEPARATOR(start[0])) // UNC
                {
                    _scheme = "unc";
                    ++start;
                }
                else // Absolute Unix
                    _scheme = "file";
            }
            else
            {
                const char *scheme_end = get_separator(start, end);
                if (scheme_end && scheme_end > start + 1)
                {
                    auto *colon_ptr = scheme_end - 1;
                    if (*colon_ptr == ':' && IS_SEPARATOR(scheme_end[0]))
                    {
                        if (scheme_end[1] == PATH_CHAR_SEP_UNIX) // Manual scheme
                        {
                            _scheme = string(start, colon_ptr - start);
                            start = scheme_end + 2;
                            _flags |= FLAG_PROTO_CALC;
                            if (_scheme != "file") _flags |= FLAG_PROTO_EXTERNAL;
                        }
                        else if (IS_SEPARATOR(scheme_end[0]) && !(_flags & FLAG_PROTO_EXTERNAL)) // Windows Path
                        {
                            _nodes.emplace_back(start, 1);
                            _scheme = "file";
                            start = scheme_end + 1;
                            _flags |= FLAG_WIN32;
                        }
                        else // URL width username/password
                        {
                            _nodes.emplace_back(start);
                            start = end;
                        }
                        _flags |= FLAG_ABS;
                    }
                }
                else
                    _scheme = "file";
            }
        }

        void path::parse_relative_part(const char *&start, const char *end)
        {
            bool was_added = false;
            while (const char *sep = get_separator(start, end))
            {
                size_t len = sep - start;
                string name(start, len);
                bool is_sub = name == "..";
                if (is_sub && was_added)
                    _nodes.pop_back();
                else if (name != ".")
                {
                    if (!is_sub) was_added = true;
                    _nodes.emplace_back(name);
                }
                start += len + 1;
            }
        }

        void path::parse_path(const string &p)
        {
            const char *start = p.c_str();
            const char *end = start + p.size();

            if (p.size() < 3)
                _scheme = "file";
            else
            {
                begin_parse(start, end);
                if (_flags & FLAG_PROTO_CALC) begin_parse(start, end);
            }

            // Parse contents
            if (start != end && *start == '.' && start + 1 == end) return;
            parse_relative_part(start, end);
            if (start < end) _nodes.emplace_back(start, end - start);
        }

        size_t path::build_scheme_part(string &result, char sep) const
        {
            size_t start = 0;
            if (_flags & FLAG_ABS)
            {
                if (_flags & FLAG_WIN32)
                {
                    if (_scheme == "unc")
                        result += "\\\\";
                    else
                    {
                        result += _nodes[0] + ":\\";
                        ++start;
                    }
                }
                else
                {
                    if (_scheme == "unc")
                        result += "//";
                    else if (_scheme != "file")
                        result += _scheme + "://";
                    else
                        result += sep;
                }
            }
            return start;
        }

        string path::build_path() const
        {
            if (_path.empty())
            {
                if (_nodes.empty()) return _path;
                char sep = _flags & FLAG_WIN32 ? PATH_CHAR_SEP_WIN32 : PATH_CHAR_SEP_UNIX;
                size_t start = build_scheme_part(_path, sep);

                for (size_t i = start; i < _nodes.size() - 1; ++i)
                {
                    _path += _nodes[i];
                    _path += sep;
                }
                if (start != _nodes.size()) _path += _nodes.back();
            }

            return _path;
        }

        path get_module_directory() noexcept
        {
#ifdef _WIN32
            HMODULE hModule = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&get_module_directory), &hModule);

            wchar_t path[MAX_PATH];
            GetModuleFileNameW(hModule, path, MAX_PATH);

            u16string full_path((c16 *)path);
            io::path p = utf16_to_utf8(full_path);
#else
            Dl_info info;
            dladdr(reinterpret_cast<void *>(&get_module_directory), &info);
            io::path p(info.dli_fname);
#endif
            return p.parent_path();
        }
    } // namespace io
} // namespace acul