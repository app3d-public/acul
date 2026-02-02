#pragma once
#include "../string/string.hpp"
#include "../vector.hpp"

namespace acul
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

        bool has_filename() const { return !_nodes.empty(); }

        void remove_filename()
        {
            if (!_nodes.empty())
            {
                _nodes.pop_back();
                _path.clear();
            }
        }

        string scheme() const { return _scheme; }

        string extension() const
        {
            if (_nodes.empty()) return {};
            const string &fname = _nodes.back();
            if (fname.empty()) return {};

            size_t pos = fname.rfind('.');
            if (pos == string::npos || pos == 0) return {};
            return fname.substr(pos);
        }

        bool has_extension() const { return !extension().empty(); }

        inline path replace_extension(const string &new_extension) const
        {
            path result = *this;
            result._path.clear();
            result._nodes.back() = stem() + new_extension;
            return result;
        }

        string stem() const
        {
            if (_nodes.empty()) return {};

            const string &fname = _nodes.back();
            if (fname.empty()) return {};

            size_t pos = fname.rfind('.');
            if (pos == string::npos || pos == 0) return fname;
            return fname.substr(0, pos);
        }

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
} // namespace acul