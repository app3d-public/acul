#include <acul/api.hpp>
#include <acul/string/string_pool.hpp>

namespace acul
{
    namespace io
    {
        namespace file
        {
            APPLIB_API void fill_line_buffer(const char *data, size_t size, string_pool<char> &dst)
            {
                const char *p = data;
                const char *end = data + size;
                const char *line_start = p;

                while (p < end)
                {
                    if (*p == '\n' || *p == '\r')
                    {
                        size_t line_len = p - line_start;
                        if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
                        dst.push(line_start, line_len);
                        ++p;
                        line_start = p;
                    }
                    else
                        ++p;
                }

                // trailing line without newline
                if (line_start < end)
                {
                    size_t line_len = end - line_start;
                    if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
                    dst.push(line_start, line_len);
                }
            }

        } // namespace file
    } // namespace io
} // namespace acul
