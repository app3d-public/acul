#include <acul/api.hpp>
#include <acul/string/string_pool.hpp>
#include <immintrin.h>

namespace acul
{
    namespace io
    {
        namespace file
        {
            APPLIB_API void fill_line_buffer(const char *data, size_t size, acul::string_pool<char> &dst)
            {
                const char *dataEnd = data + size;

                __m256i newline = _mm256_set1_epi8('\n');
                __m256i return_carriage = _mm256_set1_epi8('\r');
                const char *lineStart = data;

                const char *p = data;
                while (p < dataEnd)
                {
                    const char *next_p = (p + 32 <= dataEnd) ? p + 32 : dataEnd;

                    // Ensure aligned memory access (if necessary)
                    __m256i chunk = _mm256_loadu_si256((const __m256i *)p);

                    int mask_nl = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, newline));
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
                        const char *newlinePos = p + index;

                        if (newlinePos > lineStart)
                        {
                            size_t lineLen = newlinePos - lineStart;
                            if (lineLen > 0 && lineStart[lineLen - 1] == '\r') --lineLen;
                            dst.push(lineStart, lineLen);
                        }

                        lineStart = newlinePos + 1; // Skip newline or carriage return
                        p = newlinePos + 1;         // Advance pointer
                        mask >>= (index + 1);
                        ++i;
                    }

                    if (i < 32) p = next_p;
                }

                if (lineStart < dataEnd)
                {
                    size_t lineLen = dataEnd - lineStart;
                    if (lineLen > 0 && lineStart[lineLen - 1] == '\r') --lineLen;
                    dst.push(lineStart, lineLen);
                }
            }

        } // namespace file
    } // namespace io
} // namespace acul