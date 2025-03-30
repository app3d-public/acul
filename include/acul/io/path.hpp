#pragma once
#include "../string/string.hpp"
#include "../vector.hpp"

#define PATH_CHAR_SEP_WIN32 '\\'
#define PATH_CHAR_SEP_UNIX  '/'

#ifdef _WIN32
    #define PATH_CHAR_SEP PATH_CHAR_SEP_WIN32
#else
    #define PATH_CHAR_SEP PATH_CHAR_SEP_UNIX
#endif

namespace acul
{
    namespace io
    {
        class APPLIB_API path
        {

            enum
            {
                FLAG_NONE = 0x0,
                FLAG_ABS = 0x1,
                FLAG_WIN32 = 0x2,
                FLAG_PROTO_CALC = 0x4,
                FLAG_PROTO_EXTERNAL = 0x8
            };

        public:
            using value_type = string;
            using size_type = size_t;
            using iterator = vector<string>::iterator;
            using const_iterator = vector<string>::const_iterator;

            path() = default;
            path(const value_type &p) { parse_path(p); }
            path(const char *p) { parse_path(p); }

            const string str() const { return build_path(); }

            path operator/(const path &other) const;

            operator string() const { return str(); }

            [[nodiscard]] size_type size() const { return _nodes.size(); }

            [[nodiscard]] bool empty() const { return _nodes.empty(); }

            bool is_absolute() const { return _flags & FLAG_ABS; }

            bool is_unix_like() const { return !(_flags & FLAG_WIN32); }

            string &front() { return _nodes.front(); }
            const string &front() const { return _nodes.front(); }

            string &back() { return _nodes.back(); }
            const string &back() const { return _nodes.back(); }

            iterator begin() { return _nodes.begin(); }
            const_iterator begin() const { return _nodes.cbegin(); }
            const_iterator cbegin() const { return _nodes.cbegin(); }

            iterator end() { return _nodes.end(); }
            const_iterator end() const { return _nodes.cend(); }
            const_iterator cend() const { return _nodes.cend(); }
            path parent_path() const
            {
                if (_nodes.empty()) return {};
                path result = *this;
                result._nodes.pop_back();
                return result;
            }

            string filename() const { return _nodes.back(); }

            string scheme() const { return _scheme; }

        private:
            vector<string> _nodes;
            mutable string _scheme, _path;
            mutable int _flags = 0x0;

            void begin_parse(const char *&start, const char *end);
            void parse_relative_part(const char *&start, const char *end);
            void parse_path(const string &p);
            size_type build_scheme_part(string &result, char sep) const;

            string build_path() const;
        };

        inline size_t find_last_of(const string &str, char ch) noexcept
        {
#if defined(__linux__)
            const char *pos = static_cast<const char *>(memrchr(str.c_str(), ch, str.size()));
#else
            const char *pos = strrchr(str.c_str(), ch);
#endif
            return pos ? static_cast<size_t>(pos - str.c_str()) : SIZE_MAX;
        }

        inline string get_extension(const string &path)
        {
            size_t dot_pos = find_last_of(path, '.');
            size_t slash_pos = find_last_of(path, PATH_CHAR_SEP_UNIX);
            if (slash_pos == SIZE_MAX) slash_pos = find_last_of(path, PATH_CHAR_SEP_WIN32);
            if (dot_pos == SIZE_MAX || (slash_pos != SIZE_MAX && dot_pos < slash_pos)) return {};
            return path.substr(dot_pos);
        }

        inline string get_filename(const string &path)
        {
            size_t slash_pos = find_last_of(path, PATH_CHAR_SEP_UNIX);
            if (slash_pos == SIZE_MAX) slash_pos = find_last_of(path, PATH_CHAR_SEP_WIN32);
            return slash_pos == SIZE_MAX ? path : path.substr(slash_pos + 1);
        }

        inline string replace_filename(const string &path, const string &new_filename)
        {
            size_t slash_pos = find_last_of(path, PATH_CHAR_SEP_UNIX);
            if (slash_pos == SIZE_MAX) slash_pos = find_last_of(path, PATH_CHAR_SEP_WIN32);
            return slash_pos == SIZE_MAX ? new_filename : path.substr(0, slash_pos + 1) + new_filename;
        }

        inline string replace_extension(const string &path, const string &new_extension)
        {
            size_t slash_pos = find_last_of(path, PATH_CHAR_SEP_UNIX);
            if (slash_pos == SIZE_MAX) slash_pos = find_last_of(path, PATH_CHAR_SEP_WIN32);
            size_t dot_pos = find_last_of(path, '.');
            if (dot_pos != SIZE_MAX && (slash_pos == SIZE_MAX || dot_pos > slash_pos))
                return path.substr(0, dot_pos) + new_extension;
            return path + new_extension;
        }

    } // namespace io
} // namespace acul