#include <acul/api.hpp>
#include <acul/string/string_pool.hpp>

extern "C" APPLIB_API void acul_fill_line_buffer(const char *data, size_t size, acul::string_pool<char> &dst)
{
    const char *const data_end = data + size;

    const __m128i ch_nl = _mm_set1_epi8('\n');
    const __m128i ch_cr = _mm_set1_epi8('\r');

    const char *line_start = data;
    const char *p = data;
    bool prev_chunk_ended_with_cr = false;

    while ((p + 16) <= data_end)
    {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        int mask_nl = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, ch_nl));
        int mask_cr = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, ch_cr));

        if (prev_chunk_ended_with_cr && p[0] == '\n')
        {
            mask_nl &= ~1;
            if (line_start == p) line_start = p + 1;
        }

        int mask_cr_solo = mask_cr & ~(mask_nl >> 1);

        unsigned mask = static_cast<unsigned>(mask_nl | mask_cr_solo);

        while (mask != 0)
        {
            int index = __builtin_ctz(mask); // 0..15
            const char *sep_pos = p + index;

            size_t line_len = static_cast<size_t>(sep_pos - line_start);
            if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;

            dst.push(line_start, line_len);

            line_start = sep_pos + 1;
            mask &= (mask - 1);
        }

        prev_chunk_ended_with_cr = (p[15] == '\r');
        p += 16;
    }

    if (p < data_end)
    {
        if (prev_chunk_ended_with_cr && *p == '\n') ++p;

        const char *s = p;
        while (s < data_end)
        {
            char c = *s;
            if (c == '\n' || c == '\r')
            {
                bool is_crlf = (c == '\r' && (s + 1) < data_end && s[1] == '\n');

                size_t line_len = static_cast<size_t>(s - line_start);
                if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;

                dst.push(line_start, line_len);

                if (is_crlf) ++s;
                line_start = s + 1;
            }
            ++s;
        }
    }

    if (line_start < data_end)
    {
        size_t line_len = static_cast<size_t>(data_end - line_start);
        if (line_len > 0 && line_start[line_len - 1] == '\r') --line_len;
        dst.push(line_start, line_len);
    }
}