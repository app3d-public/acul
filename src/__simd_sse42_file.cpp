#include <core/std/darray.hpp>
#include <cstddef>
#include <string_view>
#include <core/api.hpp>

namespace io
{
    namespace file
    {
        APPLIB_API void fillLineBuffer(const char *data, size_t size, DArray<std::string_view> &dst)
        {
            const char *dataEnd = data + size;
            dst.reserve(size / 50);
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
                    p = next_p;
                else
                {
                    while (mask != 0)
                    {
                        int index = __builtin_ctz(mask); // Get index of first set bit
                        const char *newlinePos = p + index;
                        if (newlinePos >= dataEnd) break; // Stop if we reached the end of data
                        if (newlinePos > lineStart) dst.emplace_back(lineStart, newlinePos - lineStart);
                        lineStart = newlinePos + 1;
                        p = newlinePos + 1;
                        mask >>= (index + 1);
                    }
                    continue;
                }

                if (next_p == dataEnd && lineStart < dataEnd) dst.emplace_back(lineStart, dataEnd - lineStart);
            }
        }
    } // namespace file
} // namespace io