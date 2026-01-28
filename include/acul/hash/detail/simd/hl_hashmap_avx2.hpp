#pragma once

#include <immintrin.h>
#include "../../../scalars.hpp"
#include "hl_hashmap.hpp"

namespace acul
{
    namespace internal
    {
        inline void masks64_tag_empty(uint8_t tag, const uint8_t *ctrl, uint32_t &m0, uint32_t &e0, uint32_t &m1,
                                      uint32_t &e1)
        {
            const __m256i v0 = _mm256_load_si256(reinterpret_cast<const __m256i *>(ctrl));
            const __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i *>(ctrl + 32));
            const __m256i tv = _mm256_set1_epi8(static_cast<char>(tag));
            const __m256i ev = _mm256_set1_epi8(static_cast<char>(0x7F));
            const __m256i mt0 = _mm256_cmpeq_epi8(v0, tv);
            const __m256i me0 = _mm256_cmpeq_epi8(v0, ev);
            const __m256i mt1 = _mm256_cmpeq_epi8(v1, tv);
            const __m256i me1 = _mm256_cmpeq_epi8(v1, ev);
            m0 = static_cast<uint32_t>(_mm256_movemask_epi8(mt0));
            e0 = static_cast<uint32_t>(_mm256_movemask_epi8(me0));
            m1 = static_cast<uint32_t>(_mm256_movemask_epi8(mt1));
            e1 = static_cast<uint32_t>(_mm256_movemask_epi8(me1));
        }

        inline void masks64_empty(const uint8_t *ctrl, uint32_t &e0, uint32_t &e1)
        {
            const __m256i v0 = _mm256_load_si256(reinterpret_cast<const __m256i *>(ctrl));
            const __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i *>(ctrl + 32));
            const __m256i ev = _mm256_set1_epi8(static_cast<char>(0x7F));
            const __m256i me0 = _mm256_cmpeq_epi8(v0, ev);
            const __m256i me1 = _mm256_cmpeq_epi8(v1, ev);
            e0 = static_cast<uint32_t>(_mm256_movemask_epi8(me0));
            e1 = static_cast<uint32_t>(_mm256_movemask_epi8(me1));
        }

#define CTRL_SCAN_BLOCK_SIZE 32

        inline size_t ctrl_skip_to_valid(const uint8_t *ctrl, size_t idx, size_t cap)
        {
            while (idx < cap && (idx & 31u))
            {
                if (ctrl[idx] < 0x7F) return idx;
                ++idx;
            }
            const __m256i e = _mm256_set1_epi8(0x7F);
            while (idx + 32 <= cap)
            {
                __m256i v = _mm256_loadu_si256((const __m256i *)(ctrl + idx));
                __m256i eq = _mm256_cmpeq_epi8(v, e);
                unsigned em = (unsigned)_mm256_movemask_epi8(eq);
                unsigned occ = ~em;
                if (!occ)
                {
                    idx += 32;
                    continue;
                }
                return idx + (size_t)ctz32(occ);
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
            const uint32_t blk = 32u;
            size_t base = idx & ~(size_t)(blk - 1u);
            st->base = base;
            uint32_t limit = (uint32_t)((base + blk <= cap) ? blk : (cap - base));
            unsigned m = 0;
            if (limit == 32u)
            {
                __m256i v = _mm256_loadu_si256((const __m256i *)(ctrl + base));
                __m256i e = _mm256_set1_epi8((char)0x7F);
                __m256i eq = _mm256_cmpeq_epi8(v, e);
                unsigned em = (unsigned)_mm256_movemask_epi8(eq);
                m = ~em;
            }
            else
            {
                for (uint32_t k = 0; k < limit; ++k)
                    if (ctrl[base + k] < 0x7F) m |= (1u << k);
            }
            uint32_t shift = (uint32_t)(idx - base);
            if (shift + 1u >= limit)
                m = 0;
            else
                m &= ~((1u << (shift + 1u)) - 1u);
            st->mask = (uint32_t)m;
        }

        inline uint32_t ctrl_block_mask(const uint8_t *ctrl, size_t cap, size_t base)
        {
            const uint32_t blk = 32u;
            uint32_t limit = (uint32_t)((base + blk <= cap) ? blk : (cap - base));
            if (limit == 32u)
            {
                __m256i v = _mm256_loadu_si256((const __m256i *)(ctrl + base));
                __m256i e = _mm256_set1_epi8((char)0x7F);
                __m256i eq = _mm256_cmpeq_epi8(v, e);
                unsigned em = (unsigned)_mm256_movemask_epi8(eq);
                return ~em;
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