#include <acul/api.hpp>
#include <acul/string/string_pool.hpp>
#include <immintrin.h>

extern "C" APPLIB_API void acul_io_fill_line_buffer(const char *data, size_t size, acul::string_pool<char> &dst)
{
    const char *data_end = data + size;

    __m256i new_line = _mm256_set1_epi8('\n');
    __m256i return_carriage = _mm256_set1_epi8('\r');
    const char *line_start = data;

    const char *p = data;
    while (p < data_end)
    {
        const char *next_p = (p + 32 <= data_end) ? p + 32 : data_end;

        // Ensure aligned memory access (if necessary)
        __m256i chunk = _mm256_loadu_si256((const __m256i *)p);

        int mask_nl = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, new_line));
        int mask_cr = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, return_carriage));
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
            const char *new_linePos = p + index;

            if (new_linePos > line_start)
            {
                size_t line_len = new_linePos - line_start;
                if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
                dst.push(line_start, line_len);
            }

            line_start = new_linePos + 1; // Skip new_line or carriage return
            p = new_linePos + 1;          // Advance pointer
            mask >>= (index + 1);
            ++i;
        }

        if (i < 32) p = next_p;
    }

    if (line_start < data_end)
    {
        size_t line_len = data_end - line_start;
        if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
        dst.push(line_start, line_len);
    }
}