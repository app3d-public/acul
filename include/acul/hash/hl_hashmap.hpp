#pragma once

#include <cassert>
#include <functional>
#include "../api.hpp"
#include "../exception/exception.hpp"
#include "../memory/alloc.hpp"
#include "../pair.hpp"
#if defined(__AVX2__)
    #include "../simd/internal/hl_hashmap_avx2.hpp"
#elif defined(__SSE2__)
    #include "../simd/internal/hl_hashmap_sse2.hpp"
#else
    #include "../simd/internal/hl_hashmap_fallback.hpp"
#endif

#define AHM_HL_CTRL_EMPTY 0x7F
#define AHM_HL_GROUP_SIZE 32
#define AHM_HL_CTRL_PAD   AHM_HL_GROUP_SIZE
#ifndef AHM_HL_LOAD_FACTOR
    #define AHM_HL_LOAD_FACTOR 80
#endif
#define AHM_HL_PHI 11400714819323198485ull
#ifndef AHM_HL_AGRESSIVE_EXPAND_CAP
    #define AHM_HL_AGRESSIVE_EXPAND_CAP 16384
#endif

namespace acul
{
    template <typename K, typename V, typename H = std::hash<K>, typename Eq = std::equal_to<K>,
              typename Allocator = mem_allocator<std::byte>>
    class hl_hashmap
    {
    public:
        using size_type = u32;
        using value_type = pair<K, V>;
        using key_type = K;
        using mapped_type = V;
        using reference = value_type &;
        using const_reference = const value_type &;
        using allocator_type = Allocator;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using raw_pointer = typename Allocator::pointer;
        using difference_type = std::ptrdiff_t;
        using hasher = H;
        using key_equal = Eq;

        template <typename pair_t>
        class basic_iterator
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = pair_t;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type *;
            using reference = value_type &;

            basic_iterator() : _scan{0, 0}, _inited(false) {}

            template <typename P>
            basic_iterator(const basic_iterator<P> &o) noexcept : _scan(o._scan), _inited(o._inited)
            {
            }

            reference operator*() const noexcept { return _map->_pairs[_idx]; }
            pointer operator->() const noexcept { return &_map->_pairs[_idx]; }

            bool operator==(const basic_iterator &r) const noexcept
            {
                return _map->_pairs == r._map->_pairs && _idx == r._idx;
            }
            bool operator!=(const basic_iterator &r) const noexcept { return !(*this == r); }

            basic_iterator &operator++() noexcept
            {
                auto cap = _map->_num_buckets;
                if (_idx >= cap) return *this;
                if (!_inited)
                {
                    internal::ctrl_init_mask_from_current(_map->_ctrl, cap, _idx, &_scan);
                    _inited = true;
                }

                if (_scan.mask)
                {
                    u32 m = _scan.mask;
                    unsigned ofs = (unsigned)__builtin_ctz(m);
                    _scan.mask = m & (m - 1);
                    _idx = _scan.base + (size_type)ofs;
                    return *this;
                }

                size_type base = _scan.base ? (_scan.base + CTRL_SCAN_BLOCK_SIZE)
                                            : (_idx & ~(CTRL_SCAN_BLOCK_SIZE - 1u)) + CTRL_SCAN_BLOCK_SIZE;

                while (base < cap)
                {
                    u32 m = internal::ctrl_block_mask(_map->_ctrl, cap, base);
                    if (m)
                    {
                        unsigned ofs = ctz32(m);
                        _idx = base + (size_type)ofs;
                        _scan.base = base;
                        _scan.mask = m & (m - 1);
                        _inited = true;
                        return *this;
                    }
                    base += CTRL_SCAN_BLOCK_SIZE;
                }

                _idx = cap;
                _inited = false;
                _scan = {0, 0};
                return *this;
            }

            basic_iterator operator++(int) noexcept
            {
                basic_iterator t = *this;
                ++(*this);
                return t;
            }

        private:
            size_type _idx;
            const hl_hashmap *_map;
            internal::ctrl_scan_state_t _scan;
            bool _inited;

            friend class hl_hashmap;

            basic_iterator(const hl_hashmap *map, size_type idx) noexcept
                : _idx(idx), _map(map), _scan{0, 0}, _inited(false)
            {
            }
        };

        using iterator = basic_iterator<value_type>;
        using const_iterator = basic_iterator<const value_type>;

        hl_hashmap(size_type initial = 8) noexcept
            : _allocation(nullptr),
              _pairs(nullptr),
              _ctrl(nullptr),
              _mask(0),
              _num_buckets(0),
              _num_filled(0),
              _log2b(0)
        {
            rehash(initial);
        }

        // --- copy ctor
        hl_hashmap(const hl_hashmap &rhs)
            : _allocation(nullptr),
              _pairs(nullptr),
              _ctrl(nullptr),
              _mask(0),
              _num_buckets(0),
              _num_filled(0),
              _log2b(rhs._log2b)
        {
            if (rhs._num_buckets == 0) return;

            allocate_blocks(rhs._num_buckets);
            _num_filled = rhs._num_filled;

            std::memcpy(_ctrl, rhs._ctrl, size_t(_num_buckets + AHM_HL_CTRL_PAD) * sizeof(u8));

            // pairs
            if constexpr (std::is_trivially_copyable_v<value_type>)
                std::memcpy(_pairs, rhs._pairs, size_t(_num_buckets) * sizeof(value_type));
            else
                for (size_type i = 0; i < _num_buckets; ++i)
                    if (_ctrl[i] < AHM_HL_CTRL_EMPTY) ::new (_pairs + i) value_type(rhs._pairs[i]);
        }

        // --- move ctor
        hl_hashmap(hl_hashmap &&rhs) noexcept
            : _allocation(rhs._allocation),
              _pairs(rhs._pairs),
              _ctrl(rhs._ctrl),
              _mask(rhs._mask),
              _num_buckets(rhs._num_buckets),
              _num_filled(rhs._num_filled),
              _log2b(rhs._log2b)
        {
            rhs._allocation = nullptr;
            rhs._pairs = nullptr;
            rhs._ctrl = nullptr;
            rhs._mask = 0;
            rhs._num_buckets = 0;
            rhs._num_filled = 0;
            rhs._log2b = 0;
        }

        // --- copy assign
        hl_hashmap &operator=(const hl_hashmap &rhs)
        {
            if (this == &rhs) return *this;

            // free current
            if (_allocation)
            {
                Allocator::deallocate(_allocation);
                _allocation = nullptr;
                _pairs = nullptr;
                _ctrl = nullptr;
                _mask = 0;
                _num_buckets = 0;
                _num_filled = 0;
            }

            _log2b = rhs._log2b;
            if (rhs._num_buckets == 0) return *this;

            allocate_blocks(rhs._num_buckets);
            _num_filled = rhs._num_filled;

            std::memcpy(_ctrl, rhs._ctrl, size_t(_num_buckets + AHM_HL_CTRL_PAD) * sizeof(u8));

            if constexpr (std::is_trivially_copyable_v<value_type>)
                std::memcpy(_pairs, rhs._pairs, size_t(_num_buckets) * sizeof(value_type));
            else
                for (size_type i = 0; i < _num_buckets; ++i)
                    if (_ctrl[i] < AHM_HL_CTRL_EMPTY) ::new (_pairs + i) value_type(rhs._pairs[i]);
            return *this;
        }

        // --- move assign
        hl_hashmap &operator=(hl_hashmap &&rhs) noexcept
        {
            if (this == &rhs) return *this;

            if (_allocation) Allocator::deallocate(_allocation);

            _allocation = rhs._allocation;
            _pairs = rhs._pairs;
            _ctrl = rhs._ctrl;
            _mask = rhs._mask;
            _num_buckets = rhs._num_buckets;
            _num_filled = rhs._num_filled;
            _log2b = rhs._log2b;

            rhs._allocation = nullptr;
            rhs._pairs = nullptr;
            rhs._ctrl = nullptr;
            rhs._mask = 0;
            rhs._num_buckets = 0;
            rhs._num_filled = 0;
            rhs._log2b = 0;

            return *this;
        }

        hl_hashmap(std::initializer_list<value_type> ilist)
            : _allocation(nullptr),
              _pairs(nullptr),
              _ctrl(nullptr),
              _mask(0),
              _num_buckets(0),
              _num_filled(0),
              _log2b(0)
        {
            reserve((size_type)ilist.size());
            for (const auto &kv : ilist) emplace(kv);
        }

        template <class InputIt>
        hl_hashmap(InputIt first, InputIt last, size_type bucket_count = 8)
            : _allocation(nullptr),
              _pairs(nullptr),
              _ctrl(nullptr),
              _mask(0),
              _num_buckets(0),
              _num_filled(0),
              _log2b(0)
        {
            using Cat = typename std::iterator_traits<InputIt>::iterator_category;

            if constexpr (std::is_base_of_v<std::forward_iterator_tag, Cat>)
            {
                size_type n = (size_type)std::distance(first, last);
                reserve(n > bucket_count ? n : bucket_count);
            }
            else
                reserve(bucket_count);

            for (; first != last; ++first) emplace(*first);
        }

        ~hl_hashmap()
        {
            if (_allocation) Allocator::deallocate(_allocation);
        }

        bool empty() const noexcept { return _num_filled == 0; }

        size_type size() const noexcept { return _num_filled; }

        size_type max_size() const noexcept { return static_cast<size_type>(0x80000000ull * AHM_HL_LOAD_FACTOR / 100); }

        size_type bucket_count() const noexcept { return _num_buckets; }

        size_type max_bucket_count() const noexcept { return std::numeric_limits<size_type>::max(); }

        size_type bucket_size(size_type n) const
        {
            if (n >= _num_buckets) return 0;
            return (_ctrl[n] < AHM_HL_CTRL_EMPTY) ? 1 : 0;
        }

        ACUL_HOT size_type bucket(const key_type &key) const noexcept
        {
            const uint64_t hphi = hash_mixed(key);
            const uint8_t h2 = h2_from(hphi);
            const u32 h1 = (u32)(hphi >> 7);
            const u32 pos = h1 & _mask;

            // FAST PATH
            if (ACUL_LIKELY(_ctrl[pos] == h2) && ACUL_LIKELY(_eq(key, _pairs[pos].first))) return pos;

            const u32 base0 = pos & ~(AHM_HL_GROUP_SIZE - 1u);
            const u32 cut = pos & (AHM_HL_GROUP_SIZE - 1u);
            const u32 base1 = (base0 + AHM_HL_GROUP_SIZE) & _mask;

            u32 m0, e0, m1, e1;
            internal::masks64_tag_empty(h2, _ctrl + base0, m0, e0, m1, e1);

            {
                u32 cand0 = drop_lt(m0, cut);
                u32 e0tail = drop_lt(e0, cut);

                if (e0tail)
                {
                    cand0 = before_first_empty(cand0, drop_lt(e0, cut));

                    while (cand0)
                    {
                        const u32 off = pop_lsb(cand0);
                        const u32 i = (base0 + off) & _mask;
                        if (_eq(key, _pairs[i].first)) return i;
                    }
                    return _num_buckets;
                }
                else
                {
                    u32 c = cand0;
                    while (c)
                    {
                        const u32 off = pop_lsb(c);
                        const u32 i = (base0 + off) & _mask;
                        if (_eq(key, _pairs[i].first)) return i;
                    }
                }
            }

            {
                u32 cand1 = m1;
                if (e1)
                {
                    cand1 = before_first_empty(cand1, e1);

                    while (cand1)
                    {
                        const u32 off = pop_lsb(cand1);
                        const u32 i = (base1 + off) & _mask;
                        if (_eq(key, _pairs[i].first)) return i;
                    }
                    return _num_buckets;
                }
                else
                {
                    u32 c = cand1;
                    while (c)
                    {
                        const u32 off = pop_lsb(c);
                        const u32 i = (base1 + off) & _mask;
                        if (_eq(key, _pairs[i].first)) return i;
                    }
                }
            }

            return find_fallback_primary(key, h2, (base1 + AHM_HL_GROUP_SIZE) & _mask);
        }

        float load_factor() const noexcept { return _num_buckets ? float(_num_filled) / float(_num_buckets) : 0.0f; }

        float max_load_factor() const noexcept { return float(AHM_HL_LOAD_FACTOR) / 100.0f; }

        hasher hash_function() const noexcept { return H(); }

        key_equal key_eq() const noexcept { return Eq(); }

        void reserve(size_type n)
        {
            if (n == 0) return;
            u64 need = (u64)n * 100u + (AHM_HL_LOAD_FACTOR - 1);
            need /= AHM_HL_LOAD_FACTOR;
            if (need < 8u) need = 8u;
            rehash(need);
        }

        void rehash(u64 required)
        {
            if (required < (u64)_num_filled) required = (u64)_num_filled;

            const size_type new_b = (size_type)get_growth_size_aligned((u32)required);
            const size_type new_mask = new_b - 1;

            const size_t bytes_pairs = size_t(new_b) * sizeof(value_type);
            const size_t bytes_ctrl = size_t(new_b) + AHM_HL_CTRL_PAD;
            const size_t total_bytes = bytes_pairs + bytes_ctrl + 127;

            raw_pointer new_raw = Allocator::allocate(total_bytes);
            u8 *raw = align_up_ptr(reinterpret_cast<u8 *>(new_raw), 64);
            value_type *new_pairs = reinterpret_cast<value_type *>(raw);
            u8 *ctrl_base = raw + bytes_pairs;
            u8 *new_ctrl = align_up_ptr(ctrl_base, 64);
            std::memset(new_ctrl, AHM_HL_CTRL_EMPTY, new_b + AHM_HL_CTRL_PAD);

            value_type *old_pairs = _pairs;
            u8 *old_ctrl = _ctrl;
            size_type old_b = _num_buckets;

            _pairs = new_pairs;
            _ctrl = new_ctrl;
            _num_buckets = new_b;
            _mask = new_mask;
            _log2b = log2_u32(new_b);

            size_type inserted = 0;

            constexpr u32 GSHIFT = 5;
            const u32 G = (u32)((_num_buckets + (1u << GSHIFT) - 1u) >> GSHIFT);

            u32 *free_masks = alloc_n<u32>(G);
            std::memset(free_masks, 0xFF, sizeof(u32) * G);
            const u32 rem = _num_buckets & 31;
            if (rem) free_masks[G - 1] = (rem == 32) ? 0xFFFFFFFFu : ((1u << rem) - 1u);

            if (old_pairs)
            {
                for (size_type i = 0; i < old_b; ++i)
                {
                    const u8 c = old_ctrl[i];
                    if (c == AHM_HL_CTRL_EMPTY) continue;

                    value_type kv;
                    if constexpr (std::is_trivially_copyable_v<value_type>)
                        kv = old_pairs[i];
                    else
                        kv = std::move(old_pairs[i]);

                    const u64 hphi = hash_mixed(kv.first);
                    const u8 h2 = h2_from(hphi);
                    const u32 h1 = (u32)(hphi >> 7);

                    const u32 start = h1 & _mask;
                    u32 g = start >> GSHIFT;
                    const u32 cut = start & 31;

                    u32 m = free_masks[g] & (~0u << cut);
                    if (ACUL_LIKELY(m))
                    {
                        const u32 off = ctz32(m);
                        free_masks[g] &= ~(1u << off);
                        const u32 j = (g << GSHIFT) | off;
                        if constexpr (std::is_trivially_copyable_v<value_type>)
                            _pairs[j] = kv;
                        else
                            ::new ((void *)&_pairs[j]) value_type(std::move(kv));
                        set_ctrl(j, h2);
                        ++inserted;
                        continue;
                    }

                    u32 gg = (g + 1u == G) ? 0u : (g + 1u);
                    for (;; gg = (gg + 1u == G) ? 0u : (gg + 1u))
                    {
                        u32 mm = free_masks[gg];
                        if (ACUL_LIKELY(mm))
                        {
                            const u32 off = ctz32(mm);
                            free_masks[gg] = (mm & (mm - 1u));
                            const u32 j = (gg << GSHIFT) | off;
                            if constexpr (std::is_trivially_copyable_v<value_type>)
                                _pairs[j] = kv;
                            else
                                ::new ((void *)&_pairs[j]) value_type(std::move(kv));
                            set_ctrl(j, h2);
                            ++inserted;
                            break;
                        }
                    }
                }

                if (_allocation) Allocator::deallocate(_allocation);
            }
            _allocation = new_raw;
            _num_filled = inserted;
            release(free_masks);
        }

        ACUL_FORCEINLINE pair<iterator, bool> emplace(const value_type &kv) { return emplace(kv.first, kv.second); }
        ACUL_FORCEINLINE pair<iterator, bool> emplace(value_type &&kv)
        {
            return emplace(std::move(kv.first), std::move(kv.second));
        }

        template <class KK, class... Args>
        pair<iterator, bool> emplace(KK &&k, Args &&...args)
        {
            if (u64(_num_filled + 1) * 100 >= u64(_num_buckets) * AHM_HL_LOAD_FACTOR) rehash(get_next_capacity());

            const key_type &key = k;
            const u64 hphi = hash_mixed(key);
            const u8 h2 = h2_from(hphi);
            const u32 h1 = (u32)(hphi >> 7);

            u32 pos = h1 & _mask;
            u32 base = pos & ~(AHM_HL_GROUP_SIZE - 1u);
            u32 cut = pos & (AHM_HL_GROUP_SIZE - 1u);

            for (;;)
            {
                const u32 base0 = base;
                const u32 base1 = (base + AHM_HL_GROUP_SIZE) & _mask;

                u32 m0, e0, m1, e1;
                internal::masks64_tag_empty(h2, _ctrl + base0, m0, e0, m1, e1);

                u32 cand0 = drop_lt(m0, cut);
                cand0 = before_first_empty(cand0, drop_lt(e0, cut));
                while (cand0)
                {
                    const u32 off = pop_lsb(cand0);
                    const u32 i = (base0 + off) & _mask;
                    if (_eq(key, _pairs[i].first)) return {iterator(this, i), false};
                }

                if (!drop_lt(e0, cut))
                {
                    u32 cand1 = before_first_empty(m1, e1);
                    while (cand1)
                    {
                        const u32 off = pop_lsb(cand1);
                        const u32 j = (base1 + off) & _mask;
                        if (_eq(_pairs[j].first, key)) return {iterator(this, j), false};
                    }
                }

                if (u32 e = drop_lt(e0, cut); e)
                {
                    const u32 off = ctz32(e);
                    const u32 i = (base0 + off) & _mask;
                    ::new ((void *)&_pairs[i]) value_type(std::forward<KK>(k), std::forward<Args>(args)...);
                    set_ctrl(i, h2);
                    ++_num_filled;
                    return {iterator(this, i), true};
                }
                if (e1)
                {
                    const u32 off = ctz32(e1);
                    const u32 i = (base1 + off) & _mask;
                    ::new ((void *)&_pairs[i]) value_type(std::forward<KK>(k), std::forward<Args>(args)...);
                    set_ctrl(i, h2);
                    ++_num_filled;
                    return {iterator(this, i), true};
                }

                base = (base + 2 * AHM_HL_GROUP_SIZE) & _mask;
                cut = 0;

                if (base == ((h1 & _mask) & ~(AHM_HL_GROUP_SIZE - 1u)))
                {
                    rehash(get_next_capacity());
                    const u32 npos = ((u32)(hash_mixed(key) >> 7)) & _mask;
                    base = npos & ~(AHM_HL_GROUP_SIZE - 1u);
                    cut = npos & (AHM_HL_GROUP_SIZE - 1u);
                }
            }
        }

        size_type erase(const key_type &key) noexcept
        {
            const size_type i0 = bucket(key);
            if (i0 >= _num_buckets) return 0;

            if constexpr (!std::is_trivially_destructible_v<value_type>) _pairs[i0].~value_type();

            _ctrl[i0] = AHM_HL_CTRL_EMPTY;
            if (i0 < AHM_HL_GROUP_SIZE) _ctrl[_num_buckets + i0] = AHM_HL_CTRL_EMPTY;

            size_type hole = i0;
            size_type i = (i0 + 1) & _mask;

            for (;; i = (i + 1) & _mask)
            {
                const u8 c = _ctrl[i];
                if (c == AHM_HL_CTRL_EMPTY) break;

                const u64 hphi = hash_mixed(_pairs[i].first);
                const size_type home = ((size_type)(hphi >> 7)) & _mask;

                const u32 dist = (u32)((i - home) & _mask);
                const u32 gap = (u32)((i - hole) & _mask);

                if (dist >= gap)
                {
                    const u8 tag = h2_from(hphi);
                    ::new ((void *)&_pairs[hole]) value_type(std::move(_pairs[i]));
                    if constexpr (!std::is_trivially_destructible_v<value_type>) _pairs[i].~value_type();

                    set_ctrl(hole, tag);
                    _ctrl[i] = AHM_HL_CTRL_EMPTY;
                    if (i < AHM_HL_GROUP_SIZE) _ctrl[_num_buckets + i] = AHM_HL_CTRL_EMPTY;
                    hole = i;
                }
            }

            --_num_filled;
            return 1;
        }

        iterator erase(const_iterator pos) noexcept
        {
            const size_type i0 = pos._idx;
            if (i0 >= _num_buckets) return end();

            if constexpr (!std::is_trivially_destructible_v<value_type>) _pairs[i0].~value_type();

            _ctrl[i0] = AHM_HL_CTRL_EMPTY;
            if (i0 < AHM_HL_GROUP_SIZE) _ctrl[_num_buckets + i0] = AHM_HL_CTRL_EMPTY;

            size_type hole = i0;
            size_type i = (i0 + 1) & _mask;

            for (;; i = (i + 1) & _mask)
            {
                const u8 c = _ctrl[i];
                if (c == AHM_HL_CTRL_EMPTY) break;

                const u64 hphi = hash_mixed(_pairs[i].first);
                const size_type home = ((size_type)(hphi >> 7)) & _mask;

                const u32 dist = (u32)((i - home) & _mask);
                const u32 gap = (u32)((i - hole) & _mask);

                if (dist >= gap)
                {
                    const u8 tag = h2_from(hphi);
                    ::new ((void *)&_pairs[hole]) value_type(std::move(_pairs[i]));
                    if constexpr (!std::is_trivially_destructible_v<value_type>) _pairs[i].~value_type();

                    set_ctrl(hole, tag);
                    _ctrl[i] = AHM_HL_CTRL_EMPTY;
                    if (i < AHM_HL_GROUP_SIZE) _ctrl[_num_buckets + i] = AHM_HL_CTRL_EMPTY;
                    hole = i;
                }
            }

            --_num_filled;
            return iterator(this, internal::ctrl_skip_to_valid(_ctrl, i0, _num_buckets));
        }

        iterator erase(const_iterator first, const_iterator last) noexcept
        {
            if (first == last) return iterator(this, first._idx);

            iterator ret = end();
            while (first != last)
            {
                const_iterator nxt = first;
                ++nxt;

                ret = erase(first);
                first = nxt;
            }
            return ret;
        }

        template <typename U>
        ACUL_FORCEINLINE iterator find(const U &key) noexcept
        {
            return iterator(this, bucket(key));
        }

        template <typename U>
        ACUL_FORCEINLINE const_iterator find(const U &key) const noexcept
        {
            return const_iterator(this, bucket(key));
        }

        size_type count(const key_type &key) const noexcept { return bucket(key) != _num_buckets ? 1 : 0; }

        template <typename U>
        size_type count(const U &key) const noexcept
        {
            return bucket(key) != _num_buckets ? 1 : 0;
        }

        template <typename It>
        pair<It, It> equal_range(const key_type &key) noexcept
        {
            It it = find(key);
            if (it == end()) return {it, it};
            It next = it;
            ++next;
            return {it, next};
        }

        iterator begin() noexcept
        {
            if (!_pairs || _num_buckets == 0) return iterator(this, 0);
            size_type i = internal::ctrl_skip_to_valid(_ctrl, 0, _num_buckets);
            if (i >= _num_buckets) return end();
            return iterator(this, i);
        }
        const_iterator begin() const noexcept { return cbegin(); }
        const_iterator cbegin() const noexcept
        {
            if (!_pairs || _num_buckets == 0) return const_iterator(this, 0);
            size_type i = internal::ctrl_skip_to_valid(_ctrl, 0, _num_buckets);
            if (i >= _num_buckets) return cend();
            return const_iterator(this, i);
        }

        iterator end() noexcept { return iterator(this, _num_buckets); }
        const_iterator end() const noexcept { return cend(); }
        const_iterator cend() const noexcept { return const_iterator(this, _num_buckets); }

        mapped_type &at(const key_type &key)
        {
            size_type idx = bucket(key);
            if (idx == _num_buckets) throw out_of_range(_num_buckets, static_cast<size_t>(-1));
            return _pairs[idx].second;
        }

        const mapped_type &at(const key_type &key) const
        {
            size_type idx = bucket(key);
            if (idx == _num_buckets) throw out_of_range(_num_buckets, static_cast<size_t>(-1));
            return _pairs[idx].second;
        }

        mapped_type &operator[](const key_type &key)
        {
            auto res = emplace(key, V{});
            return res.first->second;
        }

        mapped_type &operator[](key_type &&key)
        {
            auto res = emplace(std::move(key), V{});
            return res.first->second;
        }

        void clear() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<value_type>)
            {
                for (size_type i = 0; i < _num_buckets; ++i)
                    if (_ctrl[i] < AHM_HL_CTRL_EMPTY) _pairs[i].~value_type();
            }
            std::memset(_ctrl, AHM_HL_CTRL_EMPTY, size_t(_num_buckets + AHM_HL_GROUP_SIZE) * sizeof(u8));
            _num_filled = 0;
        }

        pair<iterator, bool> insert(const value_type &x) { return emplace(x.first, x.second); }

        pair<iterator, bool> insert(value_type &&x) { return emplace(x.first, std::move(x.second)); }

        template <class P, class = std::enable_if_t<std::is_constructible<value_type, P &&>::value>>
        pair<iterator, bool> insert(P &&p)
        {
            value_type v(std::forward<P>(p));
            return insert(std::move(v));
        }

        iterator insert(const_iterator /*hint*/, const value_type &x) { return insert(x).first; }

        iterator insert(const_iterator /*hint*/, value_type &&x) { return insert(std::move(x)).first; }

        template <class InputIt>
        void insert(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
            {
                const auto &kv = *first;
                emplace(kv.first, kv.second);
            }
        }

        void insert(std::initializer_list<value_type> ilist)
        {
            for (const auto &kv : ilist) emplace(kv.first, kv.second);
        }

        template <class M>
        pair<iterator, bool> insert_or_assign(const key_type &key, M &&obj)
        {
            auto it = find(key);
            if (it != end())
            {
                it->second = std::forward<M>(obj);
                return {it, false};
            }
            return emplace(key, std::forward<M>(obj));
        }

        template <class M>
        pair<iterator, bool> insert_or_assign(key_type &&key, M &&obj)
        {
            auto it = find(key);
            if (it != end())
            {
                it->second = std::forward<M>(obj);
                return {it, false};
            }

            return emplace(std::move(key), std::forward<M>(obj));
        }

        template <class M>
        iterator insert_or_assign(const_iterator /*hint*/, const key_type &key, M &&obj)
        {
            return insert_or_assign(key, std::forward<M>(obj)).first;
        }

        template <class M>
        iterator insert_or_assign(const_iterator /*hint*/, key_type &&key, M &&obj)
        {
            return insert_or_assign(std::move(key), std::forward<M>(obj)).first;
        }

        void swap(hl_hashmap &other) noexcept
        {
            using std::swap;
            swap(_allocation, other._allocation);
            swap(_pairs, other._pairs);
            swap(_ctrl, other._ctrl);
            swap(_mask, other._mask);
            swap(_num_buckets, other._num_buckets);
            swap(_num_filled, other._num_filled);
            swap(_log2b, other._log2b);
        }

        std::optional<value_type> extract(const key_type &key)
        {
            const size_type i0 = bucket(key);
            if (i0 >= _num_buckets) return std::nullopt;

            std::optional<value_type> out;
            if constexpr (std::is_trivially_copyable_v<value_type>)
                out.emplace(_pairs[i0]);
            else
                out.emplace(std::move(_pairs[i0]));

            if constexpr (!std::is_trivially_destructible_v<value_type>) _pairs[i0].~value_type();

            _ctrl[i0] = AHM_HL_CTRL_EMPTY;
            if (i0 < AHM_HL_GROUP_SIZE) _ctrl[_num_buckets + i0] = AHM_HL_CTRL_EMPTY;

            size_type hole = i0;
            size_type i = (i0 + 1) & _mask;

            for (;; i = (i + 1) & _mask)
            {
                const u8 c = _ctrl[i];
                if (c == AHM_HL_CTRL_EMPTY) break;

                const u64 hphi = hash_mixed(_pairs[i].first);
                const size_type home = ((size_type)(hphi >> 7)) & _mask;

                const u32 dist = (u32)((i - home) & _mask);
                const u32 gap = (u32)((i - hole) & _mask);

                if (dist >= gap)
                {
                    const u8 tag = h2_from(hphi);
                    ::new ((void *)&_pairs[hole]) value_type(std::move(_pairs[i]));
                    if constexpr (!std::is_trivially_destructible_v<value_type>) _pairs[i].~value_type();

                    set_ctrl(hole, tag);
                    _ctrl[i] = AHM_HL_CTRL_EMPTY;
                    if (i < AHM_HL_GROUP_SIZE) _ctrl[_num_buckets + i] = AHM_HL_CTRL_EMPTY;
                    hole = i;
                }
            }

            --_num_filled;
            return out;
        }

        void merge(hl_hashmap &other)
        {
            if (&other == this) return;
            for (size_type i = 0; i < other._num_buckets; ++i)
            {
                if (other._ctrl[i] >= AHM_HL_CTRL_EMPTY) continue;

                const key_type &k = other._pairs[i].first;
                if (bucket(k) < _num_buckets) continue;

                auto node = other.extract(k);
                if (!node) continue;
                emplace(std::move(*node));
            }
        }

    private:
        raw_pointer _allocation;
        value_type *_pairs;
        u8 *_ctrl;
        size_type _mask, _num_buckets, _num_filled, _log2b;
        static constexpr H _hasher{};
        static constexpr Eq _eq{};

        ACUL_FORCEINLINE u64 hash_mixed(const key_type &k) const noexcept { return _hasher(k) * AHM_HL_PHI; }

        ACUL_FORCEINLINE u8 h2_from(u64 h) const noexcept
        {
            u8 t = u8(h >> 56);
            return (t == AHM_HL_CTRL_EMPTY) ? (AHM_HL_CTRL_EMPTY - 1) : t;
        }

        ACUL_FORCEINLINE u64 get_next_capacity()
        {
            if (_num_buckets < AHM_HL_AGRESSIVE_EXPAND_CAP) return _num_buckets * 4;
            return _num_buckets + 1;
        }

        ACUL_FORCEINLINE static u32 before_first_empty(u32 match, u32 empties) noexcept
        {
            if (!empties) return match;
            const u32 lsb = empties & (~empties + 1u);
            return match & (lsb - 1u);
        }

        static ACUL_FORCEINLINE u32 log2_u32(u32 x) noexcept { return 31u - ctz32(x); }

        ACUL_FORCEINLINE void set_ctrl(size_type i, u8 v) noexcept
        {
            _ctrl[i] = v;
            if (ACUL_LIKELY(i < AHM_HL_GROUP_SIZE)) _ctrl[_num_buckets + i] = v;
        }

        ACUL_FORCEINLINE static u32 drop_lt(u32 mask, u32 cutoff) noexcept
        {
            return cutoff ? (mask & (~0u << cutoff)) : mask;
        }

        ACUL_FORCEINLINE static u32 mask_before_first_empty(u32 match, u32 emp) noexcept
        {
            if (!emp) return match;
            u32 lsb = emp & (~emp + 1u);
            return match & (lsb - 1u);
        }

        struct probe_seq
        {
            size_type mask;
            size_type offset_;
            size_type index_;

            probe_seq(size_type h1, size_type m) : mask(m), offset_(h1 & m), index_(0) {}

            ACUL_FORCEINLINE size_type offset() const { return offset_; }
            ACUL_FORCEINLINE size_type offset(size_type i) const { return (offset_ + i) & mask; }

            ACUL_FORCEINLINE void next()
            {
                offset_ = (offset_ + AHM_HL_GROUP_SIZE) & mask;
                index_ += AHM_HL_GROUP_SIZE;
            }
            ACUL_FORCEINLINE size_type index() const { return index_; }
        };

        inline size_type find_fallback_primary(const key_type &key, u8 h2, u32 start_base) const noexcept
        {
            u32 b = start_base;
            size_type walked = 0;

            for (;;)
            {
                u32 m0, e0, m1, e1;
                internal::masks64_tag_empty(h2, _ctrl + b, m0, e0, m1, e1);

                u32 c = m0;
                while (c)
                {
                    const u32 off = pop_lsb(c);
                    const u32 i = (b + off) & _mask;
                    if (_eq(key, _pairs[i].first)) return i;
                }
                if (e0) return _num_buckets;

                c = m1;
                while (c)
                {
                    const u32 off = pop_lsb(c);
                    const u32 i = (b + AHM_HL_GROUP_SIZE + off) & _mask;
                    if (_eq(key, _pairs[i].first)) return i;
                }
                if (e1) return _num_buckets;

                b = (b + 2 * AHM_HL_GROUP_SIZE) & _mask;
                walked += 2 * AHM_HL_GROUP_SIZE;
                if (walked > _num_buckets) return _num_buckets;
            }
        }

        inline void allocate_blocks(size_type buckets) noexcept
        {
            const size_t bytes_pairs = size_t(buckets) * sizeof(value_type);
            const size_t bytes_ctrl = size_t(buckets + AHM_HL_CTRL_PAD) * sizeof(u8);

            _allocation = Allocator::allocate(bytes_pairs + bytes_ctrl);
            _pairs = reinterpret_cast<value_type *>(_allocation);
            _ctrl = reinterpret_cast<u8 *>(_allocation + bytes_pairs);

            _num_buckets = buckets;
            _mask = buckets ? (buckets - 1u) : 0u;
        }
    };
} // namespace acul