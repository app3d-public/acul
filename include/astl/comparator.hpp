#ifndef APP_CORE_STD_COMPARATOR_H
#define APP_CORE_STD_COMPARATOR_H

#include <string>
#include "map.hpp"
#include "vector.hpp"

namespace astl
{
    /**
     * @brief case_insensitive_comparator uses for sorting strings caseless.
     *
     * @note For example, I use this one to sort fonts no matter font name starts with letter starts with low case or
     * high.
     */
    struct case_insensitive_comparator
    {
        bool operator()(const std::string &a, const std::string &b) const noexcept
        {
            return strcasecmp(a.c_str(), b.c_str()) < 0;
        }
    };

    // Template class, designed to store key-value pairs where key comparison is case-insensitive.
    template <typename F, typename S>
    class case_insensitive_map
    {
    public:
        using value_type = map<F, vector<S>, case_insensitive_comparator>;
        using reference = value_type &;
        using const_reference = const value_type &;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using iterator = typename map<F, vector<S>, case_insensitive_comparator>::iterator;
        using const_iterator = typename map<F, vector<S>, case_insensitive_comparator>::const_iterator;

        iterator begin() { return iterator(_data.begin()); }
        iterator end() { return iterator(_data.end()); }
        const_iterator cbegin() const { return const_iterator(_data.begin()); }
        const_iterator cend() const { return const_iterator(_data.end()); }

        iterator operator[](const F &key)
        {
            auto it = _data.find(key);
            return it != _data.end() ? it : end();
        }

        const_iterator operator[](const F &key) const
        {
            auto it = _data.find(key);
            return it != _data.cend() ? it : cend();
        }

        // Method to insert a key-value pair into the map.
        void insert(const F &key, const vector<S> &value) { _data[key] = value; }

        // Method to erase a key-value pair from the map by its key.
        void erase(const F &key) { _data.erase(key); }

        // Clears all key-value pairs from the map.
        void clear() { _data.clear(); }

        // Check if the map is empty.
        bool empty() const { return _data.empty(); }

        // Returns the number of key-value pairs in the map.
        size_t size() const { return _data.size(); }

        // Method to insert a value into the array associated with a key, or create a new array if the key doesn't
        // exist.
        void emplace(const F &key, const S &value)
        {
            auto it = _data.find(key);
            if (it != _data.end())
                it->second.push_back(value);
            else
                _data[key] = vector<S>{value};
        }

    private:
        value_type _data;
    };
} // namespace astl
#endif