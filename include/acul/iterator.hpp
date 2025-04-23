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
} // namespace acul