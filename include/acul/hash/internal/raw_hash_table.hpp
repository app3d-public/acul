#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <optional>
#include "../../api.hpp"
#include "../../exception/exception.hpp"
#include "../../memory/alloc.hpp"
#include "../../pair.hpp"

#define AHM_INACTIVE 0xFFFFFFFFu
#ifndef AHM_LOAD_FACTOR
    #define AHM_LOAD_FACTOR 75
#endif
#ifndef AHM_AGRESSIVE_EXPAND_CAP
    #define AHM_AGRESSIVE_EXPAND_CAP 256
#endif

namespace acul
{
    namespace internal
    {
        template <typename Allocator, typename Traits>
        class raw_hashtable
        {
        public:
            using size_type = typename Traits::size_type;
            using value_type = typename Traits::value_type;
            using key_type = typename Traits::key_type;
            using mapped_type = typename Traits::mapped_type;
            using reference = value_type &;
            using const_reference = const value_type &;
            using allocator_type = Allocator;
            using pointer = value_type *;
            using const_pointer = const value_type *;
            using raw_pointer = typename Allocator::pointer;
            using difference_type = std::ptrdiff_t;
            using hasher = typename Traits::hasher;
            using key_equal = typename Traits::key_equal;

            template <typename value_ret_t>
            class basic_iterator
            {
            public:
                using iterator_category = std::forward_iterator_tag;
                using value_type = typename raw_hashtable::value_type;
                using difference_type = std::ptrdiff_t;
                using pointer = value_ret_t *;
                using reference = value_ret_t &;

                basic_iterator() noexcept : _h(nullptr), _idx(0) {}
                basic_iterator(const raw_hashtable *h, size_type idx) noexcept : _h(h), _idx(idx) {}
                template <typename P>
                basic_iterator(const basic_iterator<P> &other) noexcept : _h(other._h), _idx(other._idx)
                {
                }

                reference operator*() const noexcept { return _h->_values[_idx]; }
                pointer operator->() const noexcept { return &_h->_values[_idx]; }

                basic_iterator &operator++() noexcept
                {
                    advance();
                    return *this;
                }

                basic_iterator operator++(int) noexcept
                {
                    basic_iterator tmp = *this;
                    advance();
                    return tmp;
                }

                friend bool operator==(const basic_iterator &a, const basic_iterator &b) noexcept
                {
                    return a._h == b._h && a._idx == b._idx;
                }
                friend bool operator!=(const basic_iterator &a, const basic_iterator &b) noexcept { return !(a == b); }

            private:
                friend class raw_hashtable;
                ACUL_FORCEINLINE void advance() noexcept
                {
                    assert(_h);
                    if (_idx >= _h->_num_buckets) return;
                    size_type i = _idx + 1;
                    const size_type n = _h->_num_buckets;
                    while (i < n && _h->_next[i] == AHM_INACTIVE) ++i;
                    _idx = i;
                }

                const raw_hashtable *_h;
                size_type _idx;
            };

            using iterator = basic_iterator<value_type>;
            using const_iterator = basic_iterator<const value_type>;

            raw_hashtable(size_type bucket_count = 8) noexcept
                : _values(nullptr), _next(nullptr), _mask(0), _num_buckets(0), _num_filled(0), _last(0)
            {
                rehash(bucket_count);
            }

            raw_hashtable(const raw_hashtable &rhs)
            {
                if (!rhs._values)
                {
                    _values = nullptr;
                    _next = nullptr;
                    _num_buckets = _mask = _num_filled = _last = 0;
                    return;
                }
                allocate_blocks(rhs._num_buckets);
                std::memcpy(_next, rhs._next, sizeof(uint32_t) * _num_buckets);
                for (size_type i = 0; i < _num_buckets; ++i)
                    if (_next[i] != AHM_INACTIVE) ::new ((void *)&_values[i]) value_type(rhs._values[i]);
                _num_filled = rhs._num_filled;
                _last = rhs._last;
            }

            raw_hashtable(raw_hashtable &&rhs) noexcept
                : _values(rhs._values),
                  _next(rhs._next),
                  _num_buckets(rhs._num_buckets),
                  _mask(rhs._mask),
                  _num_filled(rhs._num_filled),
                  _last(rhs._last)
            {
                rhs._values = nullptr;
                rhs._next = nullptr;
                rhs._num_buckets = rhs._mask = rhs._num_filled = rhs._last = 0;
            }

            raw_hashtable &operator=(const raw_hashtable &rhs)
            {
                if (this == &rhs) return *this;
                if (!rhs._values)
                {
                    if (_values)
                    {
                        clear();
                        Allocator::deallocate((raw_pointer)_values);
                    }
                    _values = nullptr;
                    _next = nullptr;
                    _num_buckets = _mask = _num_filled = _last = 0;
                    return *this;
                }

                raw_hashtable tmp;
                tmp.allocate_blocks(rhs._num_buckets);
                std::memcpy(tmp._next, rhs._next, sizeof(uint32_t) * rhs._num_buckets);
                for (size_type i = 0; i < rhs._num_buckets; ++i)
                    if (tmp._next[i] != AHM_INACTIVE) ::new ((void *)&tmp._values[i]) value_type(rhs._values[i]);
                tmp._num_filled = rhs._num_filled;
                tmp._last = rhs._last;
                swap(*this, tmp);
                return *this;
            }

            raw_hashtable &operator=(raw_hashtable &&rhs) noexcept
            {
                if (this == &rhs) return *this;
                if (_values)
                {
                    clear();
                    Allocator::deallocate((raw_pointer)_values);
                }
                _values = rhs._values;
                _next = rhs._next;
                _num_buckets = rhs._num_buckets;
                _mask = rhs._mask;
                _num_filled = rhs._num_filled;
                _last = rhs._last;

                rhs._values = nullptr;
                rhs._next = nullptr;
                rhs._num_buckets = rhs._mask = rhs._num_filled = rhs._last = 0;
                return *this;
            }

            raw_hashtable(std::initializer_list<value_type> ilist)
                : _values(nullptr), _next(nullptr), _mask(0), _num_buckets(0), _num_filled(0), _last(0)
            {
                reserve((size_type)ilist.size());
                for (const auto &kv : ilist) emplace(kv);
            }

            template <class InputIt>
            raw_hashtable(InputIt first, InputIt last, size_type bucket_count = 8)
                : _values(nullptr), _next(nullptr), _mask(0), _num_buckets(0), _num_filled(0), _last(0)
            {
                using Cat = typename std::iterator_traits<InputIt>::iterator_category;

                if constexpr (std::is_base_of_v<std::forward_iterator_tag, Cat>)
                {
                    const size_type n = (size_type)std::distance(first, last);
                    reserve(n > bucket_count ? n : bucket_count);
                }
                else
                    reserve(bucket_count);

                for (; first != last; ++first) emplace(*first);
            }

            ~raw_hashtable()
            {
                if (!_values) return;
                if constexpr (!std::is_trivially_destructible_v<value_type>)
                    for (size_type i = 0; i < _num_buckets; ++i)
                        if (_next[i] != AHM_INACTIVE) _values[i].~value_type();

                Allocator::deallocate((raw_pointer)_values);
                _values = nullptr;
                _next = nullptr;
                _num_buckets = _mask = _num_filled = 0;
                _last = 0;
            }

            bool empty() const noexcept { return _num_filled == 0; }
            size_type size() const noexcept { return _num_filled; }
            size_type max_size() const noexcept { return std::numeric_limits<size_type>::max() / sizeof(value_type); }
            size_type bucket_count() const noexcept { return _num_buckets; }
            size_type max_bucket_count() const noexcept
            {
                return std::numeric_limits<size_type>::max() / sizeof(value_type);
            }
            size_type bucket_size(size_type n) const noexcept
            {
                if (n >= _num_buckets) return 0;
                const size_type nb = _next[n];
                if (nb == AHM_INACTIVE) return 0;

                size_type count = 1;
                if (nb == n) return count;
                size_type cur = nb;
                for (size_type guard = 0; guard <= _num_buckets; ++guard)
                {
                    ++count;
                    const size_type nx = _next[cur];
                    if (nx == cur || nx == AHM_INACTIVE) break;
                    cur = nx;
                }
                return count;
            }

            float load_factor() const noexcept
            {
                return _num_buckets ? float(_num_filled) / float(_num_buckets) : 0.0f;
            }

            float max_load_factor() const noexcept { return float(AHM_LOAD_FACTOR) / 100.0f; }

            hasher hash_function() const noexcept { return hasher(); }

            key_equal key_eq() const noexcept { return key_equal(); }

            void reserve(size_type n)
            {
                if (n == 0) return;
                if (uint64_t(n) * 100 <= uint64_t(_num_buckets) * AHM_LOAD_FACTOR) return;
                rehash(n);
            }

            void rehash(size_type required)
            {
                if (required < _num_filled) required = _num_filled;
                size_type new_b = get_growth_size_aligned(required);

                const size_t bytes_values = size_t(new_b) * sizeof(value_type);
                const size_t bytes_next = size_t(new_b) * sizeof(uint32_t);
                const size_t total = bytes_values + bytes_next;

                auto *base = Allocator::allocate(total);
                auto *npairs = reinterpret_cast<value_type *>(base);
                auto *nnext = reinterpret_cast<uint32_t *>((raw_pointer)(base) + bytes_values);
                std::memset(nnext, 0xFF, bytes_next); // AHM_INACTIVE

                const size_type nmask = new_b - 1;

                if (_values)
                {
                    rehash_view v{npairs, nnext, alloc_aux(new_b), nmask};
                    rehash_reserve_heads(_values, _next, _num_buckets, v);
                    rehash_move_pass(_values, _next, _num_buckets, v);

                    Allocator::deallocate((raw_pointer)_values);
                    Allocator::deallocate((raw_pointer)v.aux.reserved);
                }

                _values = npairs;
                _next = nnext;
                _num_buckets = new_b;
                _mask = nmask;
                _last = 0;
            }

            template <class... Args>
            ACUL_FORCEINLINE pair<iterator, bool> emplace(Args &&...args)
            {
                if (uint64_t(_num_filled + 1) * 100 >= uint64_t(_num_buckets) * AHM_LOAD_FACTOR)
                    rehash(get_next_capacity());
                return insert_kv(std::forward<Args>(args)...);
            }

            size_type bucket(const key_type &key) const noexcept
            {
                const size_type b0 = key_to_bucket(key);
                const size_type nb = _next[b0];
                if (is_empty(nb)) return _num_buckets;

                if (_eq(Traits::get_key(_values[b0]), key)) return b0;
                if (nb == b0) return _num_buckets;

                size_type cur = nb;
                for (;;)
                {
                    if (_eq(Traits::get_key(_values[cur]), key)) return cur;
                    const size_type nx = _next[cur];
                    if (nx == cur) return _num_buckets;
                    cur = nx;
                }
            }

            inline iterator find(const key_type &key) noexcept { return iterator(this, bucket(key)); }
            inline const_iterator find(const key_type &key) const noexcept { return const_iterator(this, bucket(key)); }
            inline bool contains(const key_type &key) const noexcept { return bucket(key) != _num_buckets; }

            size_type erase(const key_type &key) noexcept
            {
                if (_num_buckets == 0) return 0;

                const size_type b0 = key_to_bucket(key);
                const uint32_t nb = _next[b0];
                if (is_empty(nb)) return 0;

                if (_eq(Traits::get_key(_values[b0]), key))
                {
                    if (nb == b0)
                    {
                        clear_bucket(b0);
                        --_num_filled;
                        return 1;
                    }

                    const size_type nx = _next[nb];
                    if constexpr (!std::is_trivially_destructible_v<value_type>) _values[b0].~value_type();
                    ::new ((void *)&_values[b0]) value_type(std::move(_values[nb]));
                    _next[b0] = (nx == nb) ? b0 : nx;
                    clear_bucket(nb);
                    --_num_filled;
                    return 1;
                }

                size_type prev = b0;
                size_type cur = nb;
                for (;;)
                {
                    if (_eq(Traits::get_key(_values[cur]), key))
                    {
                        const size_type nx = _next[cur];
                        _next[prev] = (nx == cur) ? prev : nx;
                        clear_bucket(cur);
                        --_num_filled;
                        return 1;
                    }
                    const size_type nx = _next[cur];
                    if (nx == cur) break;
                    prev = cur;
                    cur = nx;
                }
                return 0;
            }

            iterator erase(const_iterator pos) noexcept
            {
                size_type i = pos._idx;
                if (i >= _num_buckets) return end();
                if (_next[i] == AHM_INACTIVE) return iterator(this, find_next_bucket(i));

                const size_type kmain = key_to_bucket(_values[i].first);

                if (i == kmain)
                {
                    const uint32_t nb = _next[i];
                    if (nb == i)
                    {
                        clear_bucket(i);
                        --_num_filled;
                        return iterator(this, find_next_bucket(i));
                    }

                    const size_type nx = _next[nb];
                    if constexpr (!std::is_trivially_destructible_v<value_type>) _values[i].~value_type();
                    ::new ((void *)&_values[i]) value_type(std::move(_values[nb]));
                    _next[i] = (nx == nb) ? i : nx;
                    clear_bucket(nb);
                    --_num_filled;
                    return iterator(this, i);
                }

                const size_type prev = find_prev_bucket(kmain, i);
                const size_type nx = _next[i];

                if (prev != AHM_INACTIVE)
                {
                    _next[prev] = (nx == i) ? prev : nx;
                    clear_bucket(i);
                    --_num_filled;
                    return iterator(this, find_next_bucket(i));
                }
                {
                    const size_type first = _next[kmain];

                    if (nx != i && nx != AHM_INACTIVE)
                    {
                        if (first != AHM_INACTIVE && first != kmain)
                        {
                            size_type t = nx;
                            for (size_type guard = 0; guard <= _num_buckets; ++guard)
                            {
                                const size_type tn = _next[t];
                                if (tn == t)
                                {
                                    _next[t] = first;
                                    break;
                                }
                                if (tn == AHM_INACTIVE) break;
                                t = tn;
                            }
                        }
                        _next[kmain] = nx;
                    }
                    clear_bucket(i);
                    --_num_filled;
                    return iterator(this, find_next_bucket(i));
                }
            }

            iterator erase(iterator first, iterator last) noexcept
            {
                size_type cur = first._idx;
                const size_type stop = last._idx;
                while (cur != stop)
                {
                    iterator it_next = erase(iterator(this, cur));
                    cur = it_next._idx;
                }
                return iterator(this, stop);
            }

            inline iterator begin() noexcept
            {
                if (_num_filled == 0) return end();
                return iterator(this, find_first_bucket());
            }
            inline iterator end() noexcept { return iterator(this, _num_buckets); }
            inline const_iterator begin() const noexcept
            {
                if (_num_filled == 0) return end();
                return cbegin();
            }
            inline const_iterator end() const noexcept { return cend(); }
            inline const_iterator cbegin() const noexcept
            {
                if (_num_filled == 0) return cend();
                return const_iterator(this, find_first_bucket());
            }
            inline const_iterator cend() const noexcept { return const_iterator(this, _num_buckets); }

            void clear() noexcept
            {
                if (!_values) return;
                if constexpr (!std::is_trivially_destructible_v<value_type>)
                {
                    for (size_type i = 0; i < _num_buckets; ++i)
                        if (_next[i] != AHM_INACTIVE) _values[i].~value_type();
                }
                std::memset(_next, 0xFF, sizeof(uint32_t) * _num_buckets);
                _num_filled = 0;
                _last = 0;
            }

            void swap(raw_hashtable &a, raw_hashtable &b) noexcept
            {
                using std::swap;
                swap(a._values, b._values);
                swap(a._next, b._next);
                swap(a._num_buckets, b._num_buckets);
                swap(a._mask, b._mask);
                swap(a._num_filled, b._num_filled);
                swap(a._last, b._last);
            }

            size_type count(const key_type &key) const noexcept { return bucket(key) != _num_buckets ? 1 : 0; }

            template <typename U>
            size_type count(const U &key) const noexcept
            {
                return bucket(key) != _num_buckets ? 1 : 0;
            }

            pair<iterator, iterator> equal_range(const key_type &key) noexcept
            {
                iterator it = find(key);
                if (it == end()) return {end(), end()};
                iterator next = it;
                ++next;
                return {it, next};
            }

            pair<const_iterator, const_iterator> equal_range(const key_type &key) const noexcept
            {
                const_iterator it = find(key);
                if (it == end()) return {end(), end()};
                const_iterator next = it;
                ++next;
                return {it, next};
            }

            template <class K = key_type,
                      typename = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            mapped_type &at(const K &key)
            {
                size_type idx = bucket(key);
                if (idx == _num_buckets) throw out_of_range(_num_buckets, static_cast<size_t>(-1));
                return _values[idx].second;
            }

            template <class K, class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            const mapped_type &at(const K &key) const
            {
                size_type idx = bucket(key);
                if (idx == _num_buckets) throw out_of_range(_num_buckets, static_cast<size_t>(-1));
                return _values[idx].second;
            }

            template <class K = key_type,
                      class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            mapped_type &operator[](const K &key)
            {
                auto res = emplace(key, mapped_type{});
                return res.first->second;
            }

            template <class K = key_type,
                      class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            mapped_type &operator[](K &&key)
            {
                auto res = emplace(std::move(key), mapped_type{});
                return res.first->second;
            }

            pair<iterator, bool> insert(const value_type &x) { return emplace(x); }

            pair<iterator, bool> insert(value_type &&x) { return emplace(std::move(x)); }

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
                using Cat = typename std::iterator_traits<InputIt>::iterator_category;
                if constexpr (std::is_base_of_v<std::forward_iterator_tag, Cat>)
                {
                    if (uint64_t(_num_filled + std::distance(first, last)) * 100 >=
                        uint64_t(_num_buckets) * AHM_LOAD_FACTOR)
                        rehash(get_next_capacity());
                    for (; first != last; ++first) insert_kv(*first);
                }
                else
                    for (; first != last; ++first) emplace(*first);
            }

            void insert(std::initializer_list<value_type> ilist)
            {
                if (uint64_t(_num_filled + ilist.size()) * 100 >= uint64_t(_num_buckets) * AHM_LOAD_FACTOR)
                    rehash(get_next_capacity());
                for (const auto &kv : ilist) insert_kv(kv);
            }

            template <class M, class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE pair<iterator, bool> insert_or_assign(const key_type &key, M &&obj)
            {
                auto [it, inserted] = emplace(key, std::forward<M>(obj));
                if (!inserted) it->second = std::forward<M>(obj);
                return {it, inserted};
            }

            template <class M, class = std::enable_if<!std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE pair<iterator, bool> insert_or_assign(const key_type &key)
            {
                return emplace(key);
            }

            template <class M, class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE pair<iterator, bool> insert_or_assign(key_type &&key, M &&obj)
            {
                auto [it, inserted] = emplace(std::move(key), std::forward<M>(obj));
                if (!inserted) it->second = std::forward<M>(obj);
                return {it, inserted};
            }

            template <class M, class = std::enable_if<!std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE pair<iterator, bool> insert_or_assign(key_type &&key)
            {
                return emplace(std::move(key));
            }

            template <class M, class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE iterator insert_or_assign(const_iterator /*hint*/, const key_type &key, M &&obj)
            {
                return insert_or_assign(key, std::forward<M>(obj)).first;
            }

            template <class M, class = std::enable_if<!std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE iterator insert_or_assign(const_iterator /*hint*/, const key_type &key)
            {
                return insert_or_assign(key).first;
            }

            template <class M, class = std::enable_if<std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE iterator insert_or_assign(const_iterator /*hint*/, key_type &&key, M &&obj)
            {
                return insert_or_assign(std::move(key), std::forward<M>(obj)).first;
            }

            template <class M, class = std::enable_if<!std::is_same_v<pair<key_type, mapped_type>, value_type>>>
            ACUL_FORCEINLINE iterator insert_or_assign(const_iterator /*hint*/, key_type &&key)
            {
                return insert_or_assign(std::move(key)).first;
            }

            std::optional<value_type> extract(const key_type &key) noexcept
            {
                if (_num_buckets == 0) return std::nullopt;

                const size_type b0 = key_to_bucket(key);
                const uint32_t nb = _next[b0];
                if (nb == AHM_INACTIVE) return std::nullopt;

                // --- head
                if (_eq(Traits::get_key(_values[b0]), key))
                {
                    std::optional<value_type> out(std::move(_values[b0]));

                    if (nb == b0)
                    {
                        clear_bucket(b0);
                        --_num_filled;
                        return out;
                    }

                    const size_type nx = _next[nb];
                    if constexpr (!std::is_trivially_destructible_v<value_type>) _values[b0].~value_type();
                    ::new ((void *)&_values[b0]) value_type(std::move(_values[nb]));
                    _next[b0] = (nx == nb) ? b0 : nx;
                    clear_bucket(nb);
                    --_num_filled;
                    return out;
                }

                size_type prev = b0;
                size_type cur = nb;
                for (;;)
                {
                    if (_eq(Traits::get_key(_values[cur]), key))
                    {
                        std::optional<value_type> out(std::move(_values[cur]));
                        const size_type nx = _next[cur];
                        _next[prev] = (nx == cur) ? prev : nx;
                        clear_bucket(cur);
                        --_num_filled;
                        return out;
                    }
                    const size_type nx = _next[cur];
                    if (nx == cur) break;
                    prev = cur;
                    cur = nx;
                }
                return std::nullopt;
            }

            void merge(raw_hashtable &other)
            {
                if (&other == this) return;
                reserve(_num_filled + other._num_filled);

                for (auto it = other.begin(); it != other.end();)
                {
                    auto [self_it, inserted] = emplace(it->first, it->second);
                    if (inserted)
                        it = other.erase(it);
                    else
                        ++it;
                }
            }

        private:
            value_type *_values;
            size_type *_next;
            size_type _mask, _num_buckets, _num_filled;
            size_type _last;

            static constexpr hasher _hasher = hasher{};
            static constexpr key_equal _eq = key_equal{};

            void allocate_blocks(size_type bucket_count)
            {
                if (bucket_count < 8) bucket_count = 8;
                const size_t bytes_values = size_t(bucket_count) * sizeof(value_type);
                const size_t bytes_next = size_t(bucket_count) * sizeof(uint32_t);
                const size_t total = bytes_values + bytes_next;

                void *base = Allocator::allocate(total);
                _values = reinterpret_cast<value_type *>(base);
                _next = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(base) + bytes_values);
                std::memset(_next, 0xFF, bytes_next); // AHM_INACTIVE

                _num_buckets = bucket_count;
                _mask = bucket_count - 1;
                _num_filled = 0;
                _last = 0;
            }

            ACUL_FORCEINLINE void clear_bucket(size_type i) noexcept
            {
                if constexpr (!std::is_trivially_destructible_v<value_type>) _values[i].~value_type();
                _next[i] = AHM_INACTIVE;
            }

            ACUL_FORCEINLINE void place_move(value_type &src, value_type *dst)
            {
                if constexpr (std::is_trivially_copyable_v<value_type>)
                    std::memcpy((void *)dst, (void *)&src, sizeof(value_type));
                else
                    ::new ((void *)dst) value_type(std::move(src));
                if constexpr (!std::is_trivially_destructible_v<value_type>) src.~value_type();
            }

            struct aux_mem
            {
                uint8_t *reserved; // [new_b] bytes, 0/1
                uint32_t *tails;   // [new_b] uint32_t
            };

            ACUL_FORCEINLINE aux_mem alloc_aux(size_type new_b)
            {
                const size_t off_tails = align_up(size_t(new_b), alignof(uint32_t));
                const size_t bytes = off_tails + sizeof(uint32_t) * size_t(new_b);

                void *raw = Allocator::allocate(bytes);
                auto *base8 = static_cast<uint8_t *>(raw);
                auto *reserved = base8;
                auto *tails = reinterpret_cast<uint32_t *>(base8 + off_tails);

                std::memset(reserved, 0x00, new_b);                 // not reserved
                std::memset(tails, 0xFF, sizeof(uint32_t) * new_b); // AHM_INACTIVE

                return {reserved, tails};
            }

            struct rehash_view
            {
                value_type *npairs;
                uint32_t *nnext;
                aux_mem aux;
                size_type nmask;
                size_type global_free = 0;

                ACUL_FORCEINLINE size_type find_empty(size_type near_hint) noexcept
                {
                    size_type s1 = (near_hint + 1) & nmask;
                    if (nnext[s1] == AHM_INACTIVE && !aux.reserved[s1]) return s1;

                    size_type s2 = (near_hint + 2) & nmask;
                    if (nnext[s2] == AHM_INACTIVE && !aux.reserved[s2]) return s2;

                    size_type slot = (near_hint + 2) & nmask;
                    for (size_type step = 2; step < 6; ++step)
                    {
                        size_type c1 = slot & nmask;
                        if (nnext[c1] == AHM_INACTIVE && !aux.reserved[c1]) return c1;
                        size_type c2 = (slot + 1 + (step & 1)) & nmask;
                        if (nnext[c2] == AHM_INACTIVE && !aux.reserved[c2]) return c2;
                        slot += step;
                    }

                    size_type s = global_free & nmask;
                    for (;;)
                    {
                        if (nnext[s] == AHM_INACTIVE && !aux.reserved[s])
                        {
                            global_free = (s + 1) & nmask;
                            return s;
                        }
                        s = (s + 1) & nmask;
                    }
                }
            };

            ACUL_FORCEINLINE void rehash_reserve_heads(value_type *opairs, const uint32_t *onext, size_type old_b,
                                                       rehash_view &v) noexcept
            {
                for (size_type i = 0; i < old_b; ++i)
                {
                    if (onext[i] == AHM_INACTIVE) continue;
                    const size_type b0 = size_type(_hasher(Traits::get_key(opairs[i])) & v.nmask);
                    v.aux.reserved[b0] = 1;
                }
            }

            ACUL_FORCEINLINE void rehash_move_pass(value_type *opairs, const uint32_t *onext, size_type old_b,
                                                   rehash_view &v) noexcept
            {
                for (size_type i = 0; i < old_b; ++i)
                {
                    if (onext[i] == AHM_INACTIVE) continue;

                    value_type &e = opairs[i];
                    const size_type b0 = size_type(_hasher(Traits::get_key(e)) & v.nmask);

                    if (v.nnext[b0] == AHM_INACTIVE)
                    {
                        // head
                        place_move(e, &v.npairs[b0]);
                        v.nnext[b0] = b0; // self-loop
                        v.aux.tails[b0] = b0;
                    }
                    else
                    {
                        // tail
                        const size_type tail = v.aux.tails[b0];
                        const size_type nb = v.find_empty(tail);
                        v.nnext[tail] = nb;
                        v.nnext[nb] = nb; // self-loop
                        place_move(e, &v.npairs[nb]);
                        v.aux.tails[b0] = nb;
                    }
                }
            }

            inline pair<iterator, bool> place_kv_at(size_type pos, size_type link_from, value_type &&v)
            {
                ::new ((void *)&_values[pos]) value_type(std::forward<value_type>(v));

                if (link_from == AHM_INACTIVE)
                {
                    const size_type cur_first = _next[pos];
                    _next[pos] = (cur_first == AHM_INACTIVE) ? pos : cur_first;
                }
                else
                {
                    _next[pos] = pos;
                    _next[link_from] = pos;
                }

                ++_num_filled;
                return {iterator(this, pos), true};
            }

            inline pair<iterator, bool> place_kv_at(size_type pos, size_type link_from, const value_type &v)
            {
                ::new ((void *)&_values[pos]) value_type(v);

                if (link_from == AHM_INACTIVE)
                {
                    const size_type cur_first = _next[pos];
                    _next[pos] = (cur_first == AHM_INACTIVE) ? pos : cur_first;
                }
                else
                {
                    _next[pos] = pos;
                    _next[link_from] = pos;
                }

                ++_num_filled;
                return {iterator(this, pos), true};
            }

            template <class Kx, class Vx, class M = mapped_type,
                      std::enable_if_t<!std::is_same_v<M, std::false_type>, int> = 0>
            inline pair<iterator, bool> place_kv_at(size_type pos, size_type link_from, Kx &&k, Vx &&v)
            {
                ::new ((void *)&_values[pos]) value_type{std::forward<Kx>(k), std::forward<Vx>(v)};

                if (link_from == AHM_INACTIVE)
                {
                    const size_type cur_first = _next[pos];
                    _next[pos] = (cur_first == AHM_INACTIVE) ? pos : cur_first;
                }
                else
                {
                    _next[pos] = pos;
                    _next[link_from] = pos;
                }

                ++_num_filled;
                return {iterator(this, pos), true};
            }

            template <class Kx, class Vx, class M = mapped_type,
                      std::enable_if_t<!std::is_same_v<M, std::false_type>, int> = 0>
            ACUL_FORCEINLINE pair<iterator, bool> insert_kv(Kx &&key, Vx &&val)
            {
                const auto sel = find_slot(key);
                if (sel.existed) return {iterator(this, sel.pos), false};
                return place_kv_at(sel.pos, sel.link_from, std::forward<Kx>(key), std::forward<Vx>(val));
            }

            ACUL_FORCEINLINE pair<iterator, bool> insert_kv(const value_type &kv)
            {
                const auto sel = find_slot(Traits::get_key(kv));
                if (sel.existed) return {iterator(this, sel.pos), false};
                return place_kv_at(sel.pos, sel.link_from, kv);
            }

            ACUL_FORCEINLINE pair<iterator, bool> insert_kv(value_type &&kv)
            {
                const auto sel = find_slot(Traits::get_key(kv));
                if (sel.existed) return {iterator(this, sel.pos), false};
                return place_kv_at(sel.pos, sel.link_from, std::move(kv));
            }

            template <class Kx, class... Args>
            ACUL_FORCEINLINE pair<iterator, bool> insert_kv(Kx &&key, Args &&...margs)
            {
                if constexpr (std::is_same_v<mapped_type, std::false_type>)
                {
                    value_type val(std::forward<Kx>(key), std::forward<Args>(margs)...);
                    const auto sel = find_slot(val);
                    if (sel.existed) return {iterator(this, sel.pos), false};
                    return place_kv_at(sel.pos, sel.link_from, std::move(val));
                }
                else
                {
                    const auto sel = find_slot(key);
                    if (sel.existed) return {iterator(this, sel.pos), false};
                    mapped_type val(std::forward<Args>(margs)...);
                    return place_kv_at(sel.pos, sel.link_from, std::forward<Kx>(key), std::move(val));
                }
            }

            static ACUL_FORCEINLINE bool is_empty(size_type w) noexcept { return w == AHM_INACTIVE; }

            inline size_type find_empty_bucket(size_type from, size_type chain_len_hint = 1) noexcept
            {
                size_type b = from;
                if (is_empty(_next[(b + 1) & _mask])) return (b + 1) & _mask;
                if (is_empty(_next[(b + 2) & _mask])) return (b + 2) & _mask;

                size_type slot = (from + 1u + (chain_len_hint >> 1)) & _mask;
                for (size_type step = 2; step < 6; ++step)
                {
                    size_type c1 = (slot)&_mask;
                    if (is_empty(_next[c1])) return c1;
                    size_type c2 = (slot + 1 + (step & 1)) & _mask;
                    if (is_empty(_next[c2])) return c2;
                    slot += step;
                }

                for (;;)
                {
                    size_type cand = (++_last) & _mask;
                    if (is_empty(_next[cand])) return cand;

                    size_type mid = (_num_buckets / 2 + _last) & _mask;
                    if (is_empty(_next[mid]))
                    {
                        _last = mid;
                        return mid;
                    }
                }
            }

            inline size_type find_prev_bucket(size_type kmain, size_type kbucket) const noexcept
            {
                if (kbucket == kmain) return AHM_INACTIVE;

                size_type cur = kmain;
                for (size_t guard = 0; guard <= _num_buckets; ++guard)
                {
                    const size_type nx = _next[cur];
                    if (nx == kbucket) return cur;
                    if (nx == cur || nx == AHM_INACTIVE) break;
                    cur = nx;
                }
                return AHM_INACTIVE;
            }

            ACUL_FORCEINLINE size_type find_next_bucket(size_type i) const noexcept
            {
                size_type j = i + 1;
                while (j < _num_buckets && _next[j] == AHM_INACTIVE) ++j;
                return j;
            }

            inline size_type kickout_bucket(size_type kmain, size_type kbucket) noexcept
            {
                const size_type old_next = _next[kbucket];
                const size_type from = (old_next == AHM_INACTIVE ? kbucket : old_next);
                const size_type new_bucket = find_empty_bucket(from, /*hint*/ 2);
                ::new (_values + new_bucket) value_type(std::move(_values[kbucket]));

                size_type prev = find_prev_bucket(kmain, kbucket);
                if (prev != AHM_INACTIVE)
                {
                    _next[prev] = new_bucket;
                    _next[new_bucket] = (old_next == kbucket) ? new_bucket : old_next;
                }
                else
                {
                    const size_type first = _next[kmain];
                    _next[kmain] = new_bucket;
                    if (old_next == kbucket)
                        _next[new_bucket] = (first == AHM_INACTIVE || first == kmain) ? new_bucket : first;
                    else
                    {
                        _next[new_bucket] = old_next;
                        if (first != AHM_INACTIVE && first != kmain)
                        {
                            size_type t = old_next;
                            for (size_t guard = 0; guard <= _num_buckets; ++guard)
                            {
                                const size_type nx = _next[t];
                                if (nx == t)
                                {
                                    _next[t] = first;
                                    break;
                                }
                                if (nx == AHM_INACTIVE) break;
                                t = nx;
                            }
                        }
                    }
                }

                clear_bucket(kbucket);
                return kbucket;
            }

            struct alloc_res
            {
                size_type pos;
                size_type link_from;
                bool existed;
            };

            alloc_res find_slot(const key_type &key) noexcept
            {
                const size_type b0 = key_to_bucket(key);
                const size_type n0 = _next[b0];

                if (n0 == AHM_INACTIVE) return {b0, AHM_INACTIVE, false};
                if (_eq(Traits::get_key(_values[b0]), key)) return {b0, AHM_INACTIVE, true};

                const size_type kmain = key_to_bucket(Traits::get_key(_values[b0]));
                if (kmain != b0)
                {
                    if (_next[kmain] == AHM_INACTIVE)
                    {
                        const size_type old_next = _next[b0];
                        ::new ((void *)&_values[kmain]) value_type(std::move(_values[b0]));
                        _next[kmain] = (old_next == b0) ? kmain : old_next; // self-loop or first node
                        clear_bucket(b0);
                        return {b0, AHM_INACTIVE, false};
                    }
                    else
                    {
                        (void)kickout_bucket(kmain, b0);
                        return {b0, AHM_INACTIVE, false};
                    }
                }

                size_type cur = n0, clen = 1;
                for (;;)
                {
                    if (_eq(Traits::get_key(_values[cur]), key)) return {cur, AHM_INACTIVE, true};
                    const size_type nx = _next[cur];
                    if (nx == cur)
                    {
                        const size_type nb = find_empty_bucket(cur, clen);
                        return {nb, cur, false};
                    }
                    cur = nx;
                    ++clen;
                }
            }

            inline size_type find_first_bucket() const noexcept
            {
                for (size_type i = 0, n = _num_buckets; i < n; ++i)
                    if (_next[i] != AHM_INACTIVE) return i;
                return _num_buckets;
            }

            ACUL_FORCEINLINE size_type key_to_bucket(const key_type &k) const noexcept
            {
                return size_type(_hasher(k) & _mask);
            }

            inline uint64_t get_next_capacity()
            {
                if (_num_buckets < AHM_AGRESSIVE_EXPAND_CAP) return _num_buckets * 4;
                return _num_buckets + 1;
            }
        };
    } // namespace internal
} // namespace acul