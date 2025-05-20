#include <acul/api.hpp>
#include <acul/string/string_pool.hpp>

namespace acul
{
    namespace io
    {
        namespace file
        {
            APPLIB_API void fill_line_buffer(const char *data, size_t size, string_pool<char> &pool)
            {
                const char *data_end = data + size;

                __m128i newline = _mm_set1_epi8('\n');
                __m128i return_carriage = _mm_set1_epi8('\r');
                const char *line_start = data;

                const char *p = data;
                while (p < data_end)
                {
                    const char *next_p = (p + 16 <= data_end) ? p + 16 : data_end;
                    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));

                    int mask_nl = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, newline));
                    int mask_cr = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, return_carriage));
                    int mask = mask_nl | mask_cr;

                    if (mask == 0)
                    {
                        p = next_p;
                        continue;
                    }

                    int i = 0;
                    while (mask != 0 && p < next_p)
                    {
                        int index = __builtin_ctz(mask);
                        const char *new_line_pos = p + index;

                        if (new_line_pos > line_start)
                        {
                            size_t line_len = new_line_pos - line_start;
                            if (line_len > 0 && line_start[line_len - 1] == '\r') line_len--;
                            pool.push(line_start, line_len);
                        }

                        line_start = new_line_pos + 1;
                        p = new_line_pos + 1;
                        mask >>= (index + 1);
                        i++;
                    }

                    if (i < 16) p = next_p;
                }

                if (line_start < data_end)
                {
                    size_t line_len = data_end - line_start;
                    if (line_len > 0 && line_start[line_len - 1] == '\r') line_len--;
                    pool.push(line_start, line_len);
                }
            }
        } // namespace file
    } // namespace io
} // namespace acul