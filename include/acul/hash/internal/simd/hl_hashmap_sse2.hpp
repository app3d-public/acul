#pragma once

#include <cstdint>
#include <emmintrin.h>
#include "../../../scalars.hpp"
#include "hl_hashmap.hpp"

namespace acul
{
    namespace internal
    {
        inline void masks64_tag_empty(uint8_t tag, const uint8_t *ctrl, uint32_t &m0, uint32_t &e0, uint32_t &m1,
                                      uint32_t &e1)
        {
            const __m128i v0a = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 0));
            const __m128i v0b = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 16));
            const __m128i v1a = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 32));
            const __m128i v1b = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 48));

            const __m128i tv = _mm_set1_epi8(static_cast<char>(tag));
            const __m128i ev = _mm_set1_epi8(static_cast<char>(0x7F));

            const __m128i mt0a = _mm_cmpeq_epi8(v0a, tv);
            const __m128i mt0b = _mm_cmpeq_epi8(v0b, tv);
            const __m128i me0a = _mm_cmpeq_epi8(v0a, ev);
            const __m128i me0b = _mm_cmpeq_epi8(v0b, ev);

            const __m128i mt1a = _mm_cmpeq_epi8(v1a, tv);
            const __m128i mt1b = _mm_cmpeq_epi8(v1b, tv);
            const __m128i me1a = _mm_cmpeq_epi8(v1a, ev);
            const __m128i me1b = _mm_cmpeq_epi8(v1b, ev);

            m0 =
                static_cast<uint32_t>(_mm_movemask_epi8(mt0a)) | (static_cast<uint32_t>(_mm_movemask_epi8(mt0b)) << 16);
            e0 =
                static_cast<uint32_t>(_mm_movemask_epi8(me0a)) | (static_cast<uint32_t>(_mm_movemask_epi8(me0b)) << 16);

            m1 =
                static_cast<uint32_t>(_mm_movemask_epi8(mt1a)) | (static_cast<uint32_t>(_mm_movemask_epi8(mt1b)) << 16);
            e1 =
                static_cast<uint32_t>(_mm_movemask_epi8(me1a)) | (static_cast<uint32_t>(_mm_movemask_epi8(me1b)) << 16);
        }

        inline void masks64_empty(const uint8_t *ctrl, uint32_t &e0, uint32_t &e1)
        {
            const __m128i v0a = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 0));
            const __m128i v0b = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 16));
            const __m128i v1a = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 32));
            const __m128i v1b = _mm_load_si128(reinterpret_cast<const __m128i *>(ctrl + 48));
            const __m128i ev = _mm_set1_epi8(static_cast<char>(0x7F));

            const __m128i me0a = _mm_cmpeq_epi8(v0a, ev);
            const __m128i me0b = _mm_cmpeq_epi8(v0b, ev);
            const __m128i me1a = _mm_cmpeq_epi8(v1a, ev);
            const __m128i me1b = _mm_cmpeq_epi8(v1b, ev);

            e0 =
                static_cast<uint32_t>(_mm_movemask_epi8(me0a)) | (static_cast<uint32_t>(_mm_movemask_epi8(me0b)) << 16);
            e1 =
                static_cast<uint32_t>(_mm_movemask_epi8(me1a)) | (static_cast<uint32_t>(_mm_movemask_epi8(me1b)) << 16);
        }

#define CTRL_SCAN_BLOCK_SIZE 16

        inline size_t ctrl_skip_to_valid(const uint8_t *ctrl, size_t idx, size_t cap)
        {
            const uint32_t BLK = 16;

            // дойти до границы блока
            while (idx < cap && (idx & (BLK - 1u)))
            {
                if (ctrl[idx] < 0x7F) return idx;
                ++idx;
            }

            const __m128i e = _mm_set1_epi8(0x7F);
            while (idx + BLK <= cap)
            {
                const __m128i v = _mm_loadu_si128((const __m128i *)(ctrl + idx));
                const __m128i eq = _mm_cmpeq_epi8(v, e);
                const unsigned em = (unsigned)_mm_movemask_epi8(eq); // 1=EMPTY
                const unsigned occ = (~em) & 0xFFFFu;                // 1=OCCUPIED
                if (occ) return idx + (size_t)ctz32(occ);
                idx += BLK;
            }

            while (idx < cap)
            {
                if (ctrl[idx] < 0x7F) return idx;
                ++idx;
            }
            return idx;
        }

        inline void ctrl_init_mask_from_current(const uint8_t *ctrl, size_t cap, size_t idx, ctrl_scan_state_t *st)
        {
            const uint32_t BLK = 16;

            const size_t base = idx & ~(size_t)(BLK - 1u);
            st->base = base;

            const uint32_t limit = (uint32_t)((base + BLK <= cap) ? BLK : (cap - base));
            uint32_t m = 0;

            if (limit == BLK)
            {
                const __m128i v = _mm_loadu_si128((const __m128i *)(ctrl + base));
                const __m128i e = _mm_set1_epi8(0x7F);
                const __m128i eq = _mm_cmpeq_epi8(v, e);
                const unsigned em = (unsigned)_mm_movemask_epi8(eq); // 1=EMPTY
                m = (~em) & 0xFFFFu;                                 // 1=OCCUPIED
            }
            else
            {
                for (uint32_t k = 0; k < limit; ++k)
                    if (ctrl[base + k] < 0x7F) m |= (1u << k);
            }

            const uint32_t shift = (uint32_t)(idx - base);
            if (shift + 1u >= limit) { m = 0; }
            else { m &= ~((1u << (shift + 1u)) - 1u); }
            st->mask = m;
        }

        inline uint32_t ctrl_block_mask(const uint8_t *ctrl, size_t cap, size_t base)
        {
            const uint32_t BLK = 16;

            const uint32_t limit = (uint32_t)((base + BLK <= cap) ? BLK : (cap - base));
            if (limit == BLK)
            {
                const __m128i v = _mm_loadu_si128((const __m128i *)(ctrl + base));
                const __m128i e = _mm_set1_epi8(0x7F);
                const __m128i eq = _mm_cmpeq_epi8(v, e);
                const unsigned em = (unsigned)_mm_movemask_epi8(eq); // 1=EMPTY
                return (~em) & 0xFFFFu;                              // 1=OCCUPIED
            }
            else
            {
                uint32_t m = 0;
                for (uint32_t k = 0; k < limit; ++k)
                    if (ctrl[base + k] < 0x7F) m |= (1u << k);
                return m;
            }
        }
    } // namespace internal
} // namespace acul