#include <acul/api.hpp>
#include <acul/string/string_view_pool.hpp>

namespace acul::detail::scalar
{
    void fill_line_buffer(const char *data, size_t size, string_view_pool<char> &dst)
    {
        const char *p = data;
        const char *end = data + size;
        const char *line_start = p;

        while (p < end)
        {
            if (*p == '\n')
            {
                size_t line_len = p - line_start;
                if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
                dst.push(line_start, line_len);
                ++p;
                line_start = p;
            }
            else
            {
                ++p;
            }
        }

        if (line_start < end)
        {
            size_t line_len = end - line_start;
            if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
            dst.push(line_start, line_len);
        }
    }

} // namespace acul::detail::scalar