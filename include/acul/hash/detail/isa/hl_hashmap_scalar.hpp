#pragma once

#include <cstdint>
#include <cstring>
#include "../../../scalars.hpp"
#include "hl_hashmap.hpp"

namespace acul::detail
{
    static inline uint32_t byte_eq_mask8(uint64_t x, uint8_t tag)
    {
        const uint64_t T = 0x0101010101010101ull * tag;
        const uint64_t K1 = 0x0101010101010101ull;
        const uint64_t K2 = 0x8080808080808080ull;
        uint64_t y = x ^ T;
        uint64_t z = (y - K1) & ~y & K2;
        uint64_t m = (z >> 7) & 0x0101010101010101ull;
        return (uint32_t)((m * 0x0102040810204080ull) >> 56);
    }

    static inline uint32_t byte_is_7F_mask8(uint64_t x)
    {
        const uint64_t T = 0x7F7F7F7F7F7F7F7Full;
        const uint64_t K1 = 0x0101010101010101ull;
        const uint64_t K2 = 0x8080808080808080ull;
        uint64_t y = x ^ T;
        uint64_t z = (y - K1) & ~y & K2;
        uint64_t m = (z >> 7) & 0x0101010101010101ull;
        return (uint32_t)((m * 0x0102040810204080ull) >> 56);
    }

    static inline uint64_t load_u64(const void *p)
    {
        uint64_t v;
        memcpy(&v, p, sizeof(v));
        return v;
    }

    inline void masks64_tag_empty(uint8_t tag, const uint8_t *ctrl, uint32_t &m0, uint32_t &e0, uint32_t &m1,
                                  uint32_t &e1)
    {
        uint32_t a0 = byte_eq_mask8(load_u64(ctrl + 0), tag);
        uint32_t a1 = byte_eq_mask8(load_u64(ctrl + 8), tag);
        uint32_t a2 = byte_eq_mask8(load_u64(ctrl + 16), tag);
        uint32_t a3 = byte_eq_mask8(load_u64(ctrl + 24), tag);
        m0 = a0 | (a1 << 8) | (a2 << 16) | (a3 << 24);

        uint32_t b0 = byte_is_7F_mask8(load_u64(ctrl + 0));
        uint32_t b1 = byte_is_7F_mask8(load_u64(ctrl + 8));
        uint32_t b2 = byte_is_7F_mask8(load_u64(ctrl + 16));
        uint32_t b3 = byte_is_7F_mask8(load_u64(ctrl + 24));
        e0 = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);

        uint32_t c0 = byte_eq_mask8(load_u64(ctrl + 32), tag);
        uint32_t c1 = byte_eq_mask8(load_u64(ctrl + 40), tag);
        uint32_t c2 = byte_eq_mask8(load_u64(ctrl + 48), tag);
        uint32_t c3 = byte_eq_mask8(load_u64(ctrl + 56), tag);
        m1 = c0 | (c1 << 8) | (c2 << 16) | (c3 << 24);

        uint32_t d0 = byte_is_7F_mask8(load_u64(ctrl + 32));
        uint32_t d1 = byte_is_7F_mask8(load_u64(ctrl + 40));
        uint32_t d2 = byte_is_7F_mask8(load_u64(ctrl + 48));
        uint32_t d3 = byte_is_7F_mask8(load_u64(ctrl + 56));
        e1 = d0 | (d1 << 8) | (d2 << 16) | (d3 << 24);
    }

    inline void masks64_empty(const uint8_t *ctrl, uint32_t &e0, uint32_t &e1)
    {
        uint32_t b0 = byte_is_7F_mask8(load_u64(ctrl + 0));
        uint32_t b1 = byte_is_7F_mask8(load_u64(ctrl + 8));
        uint32_t b2 = byte_is_7F_mask8(load_u64(ctrl + 16));
        uint32_t b3 = byte_is_7F_mask8(load_u64(ctrl + 24));
        e0 = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);

        uint32_t d0 = byte_is_7F_mask8(load_u64(ctrl + 32));
        uint32_t d1 = byte_is_7F_mask8(load_u64(ctrl + 40));
        uint32_t d2 = byte_is_7F_mask8(load_u64(ctrl + 48));
        uint32_t d3 = byte_is_7F_mask8(load_u64(ctrl + 56));
        e1 = d0 | (d1 << 8) | (d2 << 16) | (d3 << 24);
    }

#define CTRL_SCAN_BLOCK_SIZE 32

    inline size_t ctrl_skip_to_valid(const uint8_t *ctrl, size_t idx, size_t cap)
    {
        const uint32_t BLK = 32;

        while (idx < cap && (idx & 7u))
        {
            if (ctrl[idx] < 0x7F) return idx;
            ++idx;
        }

        while (idx + BLK <= cap)
        {
            const uint32_t em0 = byte_is_7F_mask8(load_u64(ctrl + idx + 0));
            const uint32_t em1 = byte_is_7F_mask8(load_u64(ctrl + idx + 8));
            const uint32_t em2 = byte_is_7F_mask8(load_u64(ctrl + idx + 16));
            const uint32_t em3 = byte_is_7F_mask8(load_u64(ctrl + idx + 24));
            const uint32_t em = em0 | (em1 << 8) | (em2 << 16) | (em3 << 24); // 1=EMPTY
            const uint32_t occ = ~em;                                         // 1=OCCUPIED
            if (occ) return idx + (size_t)ctz32(occ);
            idx += BLK;
        }

        while (idx + 8 <= cap)
        {
            const uint32_t em = byte_is_7F_mask8(load_u64(ctrl + idx));
            const uint32_t occ = (~em) & 0xFFu;
            if (occ) return idx + (size_t)ctz32(occ);
            idx += 8;
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
        const uint32_t BLK = 32;

        const size_t base = idx & ~(size_t)(BLK - 1u);
        st->base = base;

        const uint32_t limit = (uint32_t)((base + BLK <= cap) ? BLK : (cap - base));
        uint32_t m = 0;

        if (limit == BLK)
        {
            const uint32_t em0 = byte_is_7F_mask8(load_u64(ctrl + base + 0));
            const uint32_t em1 = byte_is_7F_mask8(load_u64(ctrl + base + 8));
            const uint32_t em2 = byte_is_7F_mask8(load_u64(ctrl + base + 16));
            const uint32_t em3 = byte_is_7F_mask8(load_u64(ctrl + base + 24));
            const uint32_t em = em0 | (em1 << 8) | (em2 << 16) | (em3 << 24); // 1=EMPTY
            m = ~em;                                                          // 1=OCCUPIED
        }
        else
        {
            for (uint32_t k = 0; k < limit; ++k)
                if (ctrl[base + k] < 0x7F) m |= (1u << k);
        }

        const uint32_t shift = (uint32_t)(idx - base);
        if (shift + 1u >= limit) m = 0;
        else m &= ~((1u << (shift + 1u)) - 1u);

        st->mask = m;
    }

    inline uint32_t ctrl_block_mask(const uint8_t *ctrl, size_t cap, size_t base)
    {
        const uint32_t BLK = 32;
        const uint32_t limit = (uint32_t)((base + BLK <= cap) ? BLK : (cap - base));
        if (limit == BLK)
        {
            const uint32_t em0 = byte_is_7F_mask8(load_u64(ctrl + base + 0));
            const uint32_t em1 = byte_is_7F_mask8(load_u64(ctrl + base + 8));
            const uint32_t em2 = byte_is_7F_mask8(load_u64(ctrl + base + 16));
            const uint32_t em3 = byte_is_7F_mask8(load_u64(ctrl + base + 24));
            const uint32_t em = em0 | (em1 << 8) | (em2 << 16) | (em3 << 24); // 1=EMPTY
            return ~em;                                                       // 1=OCCUPIED
        }
        else
        {
            uint32_t m = 0;
            for (uint32_t k = 0; k < limit; ++k)
                if (ctrl[base + k] < 0x7F) m |= (1u << k);
            return m;
        }
    }
} // namespace acul::detail
