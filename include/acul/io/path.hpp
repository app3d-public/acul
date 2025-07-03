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

            bool is_scheme_external() const { return _flags & FLAG_PROTO_EXTERNAL; }

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
                result._path.clear();
                return result;
            }

            string filename() const { return _nodes.back(); }

            string scheme() const { return _scheme; }

            bool operator==(const path &other) const { return str() == other.str(); }
            bool operator!=(const path &other) const { return str() != other.str(); }

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

        inline size_t find_last_of(const char *str, size_t len, char ch) noexcept
        {
#if defined(__linux__)
            const char *pos = static_cast<const char *>(memrchr(str, ch, len));
#else
            const char *pos = strrchr(str, ch);
#endif
            return pos ? static_cast<size_t>(pos - str) : acul::string::npos;
        }

        inline string get_extension(const string &path)
        {
            const char *p = path.c_str();
            size_t len = path.size();
            size_t dot_pos = find_last_of(p, len, '.');
            size_t slash_pos = find_last_of(p, len, PATH_CHAR_SEP_UNIX);
            if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, PATH_CHAR_SEP_WIN32);
            if (dot_pos == acul::string::npos || (slash_pos != acul::string::npos && dot_pos < slash_pos)) return {};
            return path.substr(dot_pos);
        }

        inline string get_filename(const string &path)
        {
            const char *p = path.c_str();
            size_t len = path.size();
            size_t slash_pos = find_last_of(p, len, PATH_CHAR_SEP_UNIX);
            if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, PATH_CHAR_SEP_WIN32);
            return slash_pos == acul::string::npos ? path : path.substr(slash_pos + 1);
        }

        inline string replace_filename(const string &path, const string &new_filename)
        {
            const char *p = path.c_str();
            size_t len = path.size();
            size_t slash_pos = find_last_of(p, len, PATH_CHAR_SEP_UNIX);
            if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, PATH_CHAR_SEP_WIN32);
            return slash_pos == acul::string::npos ? new_filename : path.substr(0, slash_pos + 1) + new_filename;
        }

        inline string replace_extension(const string &path, const string &new_extension)
        {
            const char *p = path.c_str();
            size_t len = path.size();
            size_t slash_pos = find_last_of(p, len, PATH_CHAR_SEP_UNIX);
            if (slash_pos == acul::string::npos) slash_pos = find_last_of(p, len, PATH_CHAR_SEP_WIN32);
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
            size_t pos = find_last_of(buffer, len, PATH_CHAR_SEP);
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
    } // namespace io
} // namespace acul