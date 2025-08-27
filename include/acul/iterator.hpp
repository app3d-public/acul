#pragma once

#include <iterator>

namespace acul
{

    template <typename C>
    class pair_second_iterator
    {
    public:
        using value_type = typename C::mapped_type;
        using reference = const value_type &;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using base_iterator = typename C::const_iterator;

        explicit pair_second_iterator(base_iterator it) : _it(it) {}

        reference operator*() const { return _it->second; }
        pair_second_iterator &operator++()
        {
            ++_it;
            return *this;
        }
        bool operator!=(const pair_second_iterator &other) const { return _it != other._it; }

    private:
        base_iterator _it;
    };

    template <typename P>
    class pointer_iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = std::remove_cv_t<std::remove_pointer_t<P>>;
        using difference_type = std::ptrdiff_t;
        using pointer = P;
        using reference = std::remove_pointer_t<P> &;
        using const_reference = const value_type &;

        pointer_iterator(pointer ptr = nullptr) : _ptr(ptr) {}

        template <typename P2, typename = std::enable_if_t<std::is_convertible_v<P2, P>>>
        pointer_iterator(const pointer_iterator<P2> &other) : _ptr(other._ptr)
        {
        }

        inline reference operator*() noexcept { return *_ptr; }
        inline reference operator*() const noexcept { return *_ptr; }
        inline pointer operator->() noexcept { return _ptr; }
        inline pointer operator->() const noexcept { return _ptr; }

        pointer_iterator &operator++()
        {
            ++_ptr;
            return *this;
        }
        pointer_iterator operator++(int)
        {
            pointer_iterator temp = *this;
            ++(*this);
            return temp;
        }
        pointer_iterator &operator--()
        {
            --_ptr;
            return *this;
        }
        pointer_iterator operator--(int)
        {
            pointer_iterator temp = *this;
            --(*this);
            return temp;
        }

        friend bool operator==(const pointer_iterator &a, const pointer_iterator &b) { return a._ptr == b._ptr; }
        friend bool operator!=(const pointer_iterator &a, const pointer_iterator &b) { return a._ptr != b._ptr; }

        pointer_iterator &operator+=(difference_type movement)
        {
            _ptr += movement;
            return *this;
        }
        pointer_iterator &operator-=(difference_type movement)
        {
            _ptr -= movement;
            return *this;
        }
        pointer_iterator operator+(difference_type movement) const { return pointer_iterator(_ptr + movement); }
        pointer_iterator operator-(difference_type movement) const { return pointer_iterator(_ptr - movement); }

        difference_type operator-(const pointer_iterator &other) const { return _ptr - other._ptr; }

        friend bool operator<(const pointer_iterator &a, const pointer_iterator &b) { return a._ptr < b._ptr; }
        friend bool operator>(const pointer_iterator &a, const pointer_iterator &b) { return a._ptr > b._ptr; }
        friend bool operator<=(const pointer_iterator &a, const pointer_iterator &b) { return a._ptr <= b._ptr; }
        friend bool operator>=(const pointer_iterator &a, const pointer_iterator &b) { return a._ptr >= b._ptr; }

        reference operator[](difference_type offset) const { return *(*this + offset); }

        pointer base() const noexcept { return _ptr; }

    private:
        pointer _ptr;

        template <class>
        friend class pointer_iterator;
    };

    template <typename P1, typename P2>
    constexpr bool is_same_pointee =
        std::is_same_v<std::remove_cv_t<std::remove_pointer_t<P1>>, std::remove_cv_t<std::remove_pointer_t<P2>>>;

    template <typename P1, typename P2, typename = std::enable_if_t<is_same_pointee<P1, P2>>>
    inline bool operator==(const pointer_iterator<P1> &a, const pointer_iterator<P2> &b)
    {
        return a.base() == b.base();
    }
    template <typename P1, typename P2, typename = std::enable_if_t<is_same_pointee<P1, P2>>>
    inline bool operator!=(const pointer_iterator<P1> &a, const pointer_iterator<P2> &b)
    {
        return !(a == b);
    }

    template <typename P1, typename P2, typename = std::enable_if_t<is_same_pointee<P1, P2>>>
    inline auto operator-(const pointer_iterator<P1> &a, const pointer_iterator<P2> &b) ->
        typename pointer_iterator<P1>::difference_type
    {
        return a.base() - b.base();
    }

    template <typename P1, typename P2, typename = std::enable_if_t<is_same_pointee<P1, P2>>>
    inline bool operator<(const pointer_iterator<P1> &a, const pointer_iterator<P2> &b)
    {
        return a.base() < b.base();
    }
} // namespace acul