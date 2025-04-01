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
                const char *dataEnd = data + size;

                __m128i newline = _mm_set1_epi8('\n');
                __m128i return_carriage = _mm_set1_epi8('\r');
                const char *lineStart = data;

                const char *p = data;
                while (p < dataEnd)
                {
                    const char *next_p = (p + 16 <= dataEnd) ? p + 16 : dataEnd;
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
                        const char *newlinePos = p + index;

                        if (newlinePos > lineStart)
                        {
                            size_t lineLen = newlinePos - lineStart;
                            if (lineLen > 0 && lineStart[lineLen - 1] == '\r') lineLen--;
                            pool.push(lineStart, lineLen);
                        }

                        lineStart = newlinePos + 1;
                        p = newlinePos + 1;
                        mask >>= (index + 1);
                        i++;
                    }

                    if (i < 16) p = next_p;
                }

                if (lineStart < dataEnd)
                {
                    size_t lineLen = dataEnd - lineStart;
                    if (lineLen > 0 && lineStart[lineLen - 1] == '\r') lineLen--;
                    pool.push(lineStart, lineLen);
                }
            }
        } // namespace file
    } // namespace io
} // namespace acul