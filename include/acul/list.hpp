#pragma once

#include "memory.hpp"
#include "type_traits.hpp"

namespace acul
{

    template <typename T, typename Allocator = mem_allocator<T>>
    class list
    {
        struct Node
        {
            using pointer = Node *;
            T data;
            pointer next;
            pointer prev;

            Node(const T &value) : data(value), next(nullptr), prev(nullptr) {}

            template <typename... Args>
            Node(Args &&...args) : data(std::forward<Args>(args)...), next(nullptr), prev(nullptr)
            {
            }
        };

    public:
        using node_allocator = typename Allocator::template rebind<Node>::other;
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = Node *;
        using const_pointer = const Node *;
        using size_type = size_t;

        class Iterator;
        class ReverseIterator;

        using iterator = Iterator;
        using const_iterator = const Iterator;
        using reverse_iterator = ReverseIterator;
        using const_reverse_iterator = const ReverseIterator;

        list() = default;

        list(const list &other)
        {
            for (pointer node = other._head; node != nullptr; node = node->next) push_back(node->data);
        }

        list(list &&other) noexcept : _head(other._head), _last(other._last)
        {
            other._head = nullptr;
            other._last = nullptr;
        }

        list &operator=(const list &other)
        {
            if (this != &other)
            {
                clear();
                for (pointer node = other._head; node != nullptr; node = node->next) push_back(node->data);
            }
            return *this;
        }

        list &operator=(list &&other) noexcept
        {
            if (this != &other)
            {
                clear();
                _head = other._head;
                _last = other._last;
                other._head = nullptr;
                other._last = nullptr;
            }
            return *this;
        }

        explicit list(size_type count, const_reference value = T()) noexcept : _head(nullptr), _last(nullptr)
        {
            for (size_type i = 0; i < count; ++i) push_back(value);
        }

        list(std::initializer_list<T> ilist) noexcept : _head(nullptr), _last(nullptr)
        {
            for (const_reference value : ilist) push_back(value);
        }

        list &operator=(std::initializer_list<T> ilist)
        {
            clear();
            for (const_reference value : ilist) push_back(value);
            return *this;
        }

        template <typename InputIt, typename = std::enable_if_t<is_input_iterator_based<InputIt>::value>>
        list(InputIt first, InputIt last) : list()
        {
            for (; first != last; ++first) push_back(*first);
        }

        ~list() noexcept
        {
            while (_head != nullptr)
            {
                Node *p = _head;
                _head = _head->next;
                node_allocator::destroy(p);
                node_allocator::deallocate(p, 1);
            }
        }

        bool operator==(const list &other) const
        {
            pointer node = this->_head;
            pointer otherNode = other._head;
            while (node != nullptr && otherNode != nullptr)
            {
                if (node->data != otherNode->data) return false;
                node = node->next;
                otherNode = otherNode->next;
            }
            return node == nullptr && otherNode == nullptr;
        }

        void clear() noexcept
        {
            pointer current = _head;
            while (current != nullptr)
            {
                pointer next = current->next;
                node_allocator::destroy(current);
                node_allocator::deallocate(current, 1);
                current = next;
            }
            _head = _last = nullptr;
        }

        reference front() noexcept { return _head->data; }
        const_reference front() const noexcept { return _head->data; }

        reference back() noexcept { return _last->data; }
        const_reference back() const noexcept { return _last->data; }

        void push_front(const_reference value) noexcept
        {
            pointer node = node_allocator::allocate(1);
            if constexpr (std::is_trivially_constructible_v<T>)
                node->data = value;
            else
                node_allocator::construct(node, value);
            node->next = _head;
            node->prev = nullptr;
            if (_head)
                _head->prev = node;
            else
                _last = node;
            _head = node;
        }

        void push_front(T &&value) noexcept
        {
            pointer node = node_allocator::allocate(1);
            if constexpr (std::is_trivially_constructible_v<T>)
                node->data = std::forward<T>(value);
            else
                node_allocator::construct(node, std::forward<T>(value));
            node->next = _head;
            node->prev = nullptr;
            if (_head)
                _head->prev = node;
            else
                _last = node;
            _head = node;
        }

        void push_back(const_reference value) noexcept
        {
            pointer p = node_allocator::allocate(1);
            if constexpr (std::is_trivially_constructible_v<T>)
                p->data = value;
            else
                node_allocator::construct(p, value);
            p->prev = _last;
            p->next = nullptr;
            if (_last)
                _last->next = p;
            else
                _head = p;
            _last = p;
        }

        void push_back(T &&value) noexcept
        {
            pointer p = node_allocator::allocate(1);
            if constexpr (std::is_trivially_constructible_v<T>)
                p->data = std::forward<T>(value);
            else
                node_allocator::construct(p, std::forward<T>(value));
            p->prev = _last;
            p->next = nullptr;
            if (_last)
                _last->next = p;
            else
                _head = p;
            _last = p;
        }

        void pop_front() noexcept
        {
            if (!_head) return;
            pointer temp = _head;
            _head = _head->next;
            if (_head)
                _head->prev = nullptr;
            else
                _last = nullptr;
            node_allocator::destroy(temp);
            node_allocator::deallocate(temp, 1);
        }

        void pop_back() noexcept
        {
            if (!_last) return;
            pointer temp = _last;
            _last = _last->prev;
            if (_last)
                _last->next = nullptr;
            else
                _head = nullptr;
            node_allocator::destroy(temp);
            node_allocator::deallocate(temp, 1);
        }

        template <typename... Args>
        void emplace_front(Args &&...args)
        {
            pointer node = node_allocator::allocate(1);
            node_allocator::construct(node, std::forward<Args>(args)...);
            node->next = _head;
            node->prev = nullptr;
            if (_head)
                _head->prev = node;
            else
                _last = node;
            _head = node;
        }

        template <typename... Args>
        void emplace_back(Args &&...args)
        {
            pointer node = node_allocator::allocate(1);
            node_allocator::construct(node, std::forward<Args>(args)...);
            node->next = nullptr;
            node->prev = _last;
            if (_last)
                _last->next = node;
            else
                _head = node;
            _last = node;
        }

        iterator begin() { return iterator(_head); }
        const_iterator begin() const { return const_iterator(_head); }
        const_iterator cbegin() const { return const_iterator(_head); }

        iterator end() { return iterator(nullptr); }
        const_iterator end() const { return const_iterator(nullptr); }
        const_iterator cend() const { return const_iterator(nullptr); }

        reverse_iterator rbegin() { return reverse_iterator(_last); }
        const_reverse_iterator rbegin() const { return const_reverse_iterator(_last); }
        const_reverse_iterator crbegin() const { return const_reverse_iterator(_last); }

        reverse_iterator rend() { return reverse_iterator(nullptr); }
        const_reverse_iterator rend() const { return const_reverse_iterator(nullptr); }
        const_reverse_iterator crend() const { return const_reverse_iterator(nullptr); }

        iterator insert(const iterator &pos, const_reference value)
        {
            if (pos == begin())
            {
                push_front(value);
                return begin();
            }
            else if (pos == end())
            {
                push_back(value);
                return --end();
            }
            pointer node = node_allocator::allocate(1);
            if constexpr (std::is_trivially_constructible_v<T>)
                node->data = value;
            else
                node_allocator::construct(node, value);
            auto prev = pos._ptr->prev;
            node->next = pos._ptr;
            node->prev = prev;
            prev->next = node;
            pos._ptr->prev = node;
            return iterator(node);
        }

        template <typename InputIt>
        void insert(iterator pos, InputIt first, InputIt last);

        template <typename InputIt>
        void assign(InputIt first, InputIt last)
        {
            clear();
            for (; first != last; ++first) push_back(*first);
        }

        void assign(std::initializer_list<T> ilist) { assign(ilist.begin(), ilist.end()); }

        void assign(size_type count, const_reference value)
        {
            clear();
            for (size_type i = 0; i < count; ++i) push_back(value);
        }

        iterator erase(const_iterator &pos)
        {
            if (!pos._ptr) return pos;
            iterator nextIter(pos._ptr->next);
            if (pos._ptr == _head)
                pop_front();
            else if (pos._ptr == _last)
                pop_back();
            else
            {
                pos._ptr->prev->next = pos._ptr->next;
                pos._ptr->next->prev = pos._ptr->prev;
                node_allocator::destroy(pos._ptr);
                node_allocator::deallocate(pos._ptr, 1);
            }
            return nextIter;
        }

        iterator erase(iterator first, iterator last);

        size_type size() const noexcept
        {
            size_type count = 0;
            pointer current = _head;
            while (current)
            {
                ++count;
                current = current->next;
            }
            return count;
        }

        size_type max_size() const noexcept { return node_allocator::max_size(); }

        bool empty() const { return _head == nullptr; }

        void swap(list &other) noexcept
        {
            std::swap(_head, other._head);
            std::swap(_last, other._last);
        }

        void reverse() noexcept
        {
            pointer current = _head;
            pointer temp = nullptr;
            _last = _head;

            while (current != nullptr)
            {
                temp = current->prev;
                current->prev = current->next;
                current->next = temp;
                current = current->prev;
            }
            if (temp != nullptr) _head = temp->prev;
        }

        void sort()
        {
            if (!_head || !_head->next) return;

            int length = 0;
            for (pointer p = _head; p != nullptr; p = p->next) ++length;
            for (int size = 1; size < length; size *= 2)
            {
                pointer cur = _head;
                pointer tail = nullptr;
                pointer next = nullptr;

                while (cur)
                {
                    pointer left = cur;
                    pointer right = split(left, size);
                    cur = split(right, size);
                    next = merge(left, right);
                    if (!tail)
                        _head = next;
                    else
                        tail->next = next;
                    while (tail && tail->next) tail = tail->next;
                }
            }
        }

        pointer merge(pointer left, pointer right)
        {
            if (!left) return right;
            if (!right) return left;

            pointer head = nullptr;
            pointer tail = nullptr;

            if (left->data < right->data)
            {
                head = tail = left;
                left = left->next;
            }
            else
            {
                head = tail = right;
                right = right->next;
            }

            while (left && right)
            {
                if (left->data < right->data)
                {
                    tail->next = left;
                    left->prev = tail;
                    tail = left;
                    left = left->next;
                }
                else
                {
                    tail->next = right;
                    right->prev = tail;
                    tail = right;
                    right = right->next;
                }
            }

            if (left)
            {
                tail->next = left;
                left->prev = tail;
            }
            else if (right)
            {
                tail->next = right;
                right->prev = tail;
            }

            return head;
        }

        void merge(list &other)
        {
            if (this == &other || other._head == nullptr) return;
            _head = merge(_head, other._head);
            pointer temp = _head;
            while (temp && temp->next) temp = temp->next;
            _last = temp;
            other._head = other._last = nullptr;
        }

        void unique()
        {
            if (_head == nullptr || _head->next == nullptr) return;
            for (auto it = _head; it != nullptr && it->next != nullptr;)
            {
                if (it->data == it->next->data)
                {
                    auto to_delete = it->next;
                    it->next = to_delete->next;
                    if (to_delete->next != nullptr)
                        to_delete->next->prev = it;
                    else
                        _last = it;
                    node_allocator::destroy(to_delete);
                    node_allocator::deallocate(to_delete, 1);
                }
                else
                    it = it->next;
            }
        }

        void splice_after(iterator pos, list &other);
        void splice_after(iterator pos, list &other, iterator it);
        void splice_after(iterator pos, list &&other, iterator it);
        void splice_after(iterator pos, list &&other);
        void splice_after(iterator pos, list &other, iterator first, iterator last);
        void splice_after(iterator pos, list &&other, iterator first, iterator last);

        void splice_before(iterator pos, list &other);
        void splice_before(iterator pos, list &other, iterator it);
        void splice_before(iterator pos, list &&other, iterator it);
        void splice_before(iterator pos, list &&other);
        void splice_before(iterator pos, list &other, iterator first, iterator last);
        void splice_before(iterator pos, list &&other, iterator first, iterator last);

        void remove(const_reference value)
        {
            for (iterator it = begin(); it != end();) it = *it == value ? erase(it) : ++it;
        }

        template <typename Predicate>
        void remove_if(Predicate pred)
        {
            for (iterator it = begin(); it != end();) it = pred(*it) ? erase(it) : ++it;
        }

    private:
        pointer _head = nullptr;
        pointer _last = nullptr;

        pointer split(pointer start, int size)
        {
            pointer cur = start;
            for (int i = 1; cur && i < size; ++i) cur = cur->next;
            if (!cur) return nullptr;
            pointer next = cur->next;
            cur->next = nullptr;
            if (next) next->prev = nullptr;
            return next;
        }
    };

    template <typename T, typename Allocator>
    class list<T, Allocator>::Iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = list::pointer;
        using reference = T &;
        using const_reference = const T &;

        Iterator(pointer ptr = nullptr) : _ptr(ptr) {}

        Iterator &operator=(const Iterator &other)
        {
            if (this != &other) _ptr = other._ptr;
            return *this;
        }

        Iterator &operator++()
        {
            _ptr = _ptr->next;
            return *this;
        }
        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }
        Iterator &operator--()
        {
            _ptr = _ptr->prev;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        reference operator*() { return _ptr->data; }
        reference operator*() const { return _ptr->data; }
        pointer operator->() const { return _ptr; }

        friend bool operator==(const Iterator &a, const Iterator &b) { return a._ptr == b._ptr; }
        friend bool operator!=(const Iterator &a, const Iterator &b) { return a._ptr != b._ptr; }
        friend bool operator<(const Iterator &a, const Iterator &b) { return a._ptr < b._ptr; }
        friend bool operator>(const Iterator &a, const Iterator &b) { return a._ptr > b._ptr; }
        friend bool operator<=(const Iterator &a, const Iterator &b) { return a._ptr <= b._ptr; }
        friend bool operator>=(const Iterator &a, const Iterator &b) { return a._ptr >= b._ptr; }
        friend class list;

    private:
        pointer _ptr;
    };

    template <typename T, typename Allocator>
    class list<T, Allocator>::ReverseIterator
    {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = list::pointer;
        using reference = T &;
        using const_reference = const T &;

        ReverseIterator(pointer ptr = nullptr) : _ptr(ptr) {}

        ReverseIterator &operator=(const ReverseIterator &other)
        {
            if (this != &other) _ptr = other._ptr;
            return *this;
        }

        ReverseIterator &operator++()
        {
            _ptr = _ptr->prev;
            return *this;
        }
        ReverseIterator operator++(int)
        {
            ReverseIterator temp = *this;
            ++(*this);
            return temp;
        }
        ReverseIterator &operator--()
        {
            _ptr = _ptr->next;
            return *this;
        }

        ReverseIterator operator--(int)
        {
            ReverseIterator temp = *this;
            --(*this);
            return temp;
        }

        reference operator*() { return _ptr->data; }
        reference operator*() const { return _ptr->data; }
        pointer operator->() const { return _ptr; }

        friend bool operator==(const ReverseIterator &a, const ReverseIterator &b) { return a._ptr == b._ptr; }
        friend bool operator!=(const ReverseIterator &a, const ReverseIterator &b) { return a._ptr != b._ptr; }
        friend bool operator<(const ReverseIterator &a, const ReverseIterator &b) { return a._ptr < b._ptr; }
        friend bool operator>(const ReverseIterator &a, const ReverseIterator &b) { return a._ptr > b._ptr; }
        friend bool operator<=(const ReverseIterator &a, const ReverseIterator &b) { return a._ptr <= b._ptr; }
        friend bool operator>=(const ReverseIterator &a, const ReverseIterator &b) { return a._ptr >= b._ptr; }
        friend class list;

    private:
        pointer _ptr;
    };

    template <typename T, typename Allocator>
    template <typename InputIt>
    void list<T, Allocator>::insert(iterator pos, InputIt first, InputIt last)
    {
        for (; first != last; ++first) insert(pos, *first);
    }

    template <typename T, typename Allocator>
    list<T, Allocator>::iterator list<T, Allocator>::erase(iterator first, iterator last)
    {
        while (first != last) first = erase(first);
        return last;
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_after(iterator pos, list &other)
    {
        if (other.empty() || this == &other) return;
        other._last->next = pos._ptr->next;
        if (pos._ptr->next) pos._ptr->next->prev = other._last;
        pos->next = other._head;
        other._head->prev = pos._ptr;
        if (pos._ptr == _last) _last = other._last;
        other._head = other._last = nullptr;
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_after(iterator pos, list &other, iterator it)
    {
        if (it._ptr == other._last || this == &other) return;
        pointer next = it._ptr->next;
        if (next) next->prev = it._ptr->prev;
        it._ptr->prev->next = next;
        it._ptr->next = pos._ptr->next;
        it._ptr->prev = pos._ptr;
        if (pos._ptr->next) pos._ptr->next->prev = it._ptr;
        pos._ptr->next = it._ptr;
        if (pos._ptr == _last) _last = it._ptr;
        if (it._ptr == other._last) other._last = it._ptr->prev;
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_after(iterator pos, list &&other, iterator it)
    {
        splice_after(pos, other, it);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_after(iterator pos, list &&other)
    {
        splice_after(pos, other);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_after(iterator pos, list &other, iterator first, iterator last)
    {
        if (first == last || this == &other) return;
        pointer first_node = first._ptr->next;
        pointer last_node = last._ptr->prev;
        first._ptr->next = last._ptr;
        last._ptr->prev = first._ptr;
        last_node->next = pos._ptr->next;
        if (pos._ptr->next) pos._ptr->next->prev = last_node;
        first_node->prev = pos._ptr;
        pos._ptr->next = first_node;
        if (pos._ptr == _last) _last = last_node;
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_after(iterator pos, list &&other, iterator first, iterator last)
    {
        splice_after(pos, other, first, last);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_before(iterator pos, list &other)
    {
        if (other.empty() || this == &other || pos == begin()) return;
        splice_after(--pos, other);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_before(iterator pos, list &other, iterator it)
    {
        if (pos == begin() || it == other.begin()) return;
        splice_after(--pos, other, it);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_before(iterator pos, list &&other, iterator it)
    {
        splice_before(pos, other, it);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_before(iterator pos, list &&other)
    {
        splice_before(pos, other);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_before(iterator pos, list &other, iterator first, iterator last)
    {
        if (pos == begin() || first == other.begin() || first == last) return;
        splice_after(--pos, other, first, last);
    }

    template <typename T, typename Allocator>
    void list<T, Allocator>::splice_before(iterator pos, list &&other, iterator first, iterator last)
    {
        splice_before(pos, other, first, last);
    }

} // namespace acul