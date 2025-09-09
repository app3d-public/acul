#pragma once

#include "memory/alloc.hpp"
#include "type_traits.hpp"

namespace acul
{
    template <typename T, typename Allocator = mem_allocator<T>>
    class forward_list
    {
        struct Node
        {
            using pointer = Node *;
            T data;
            pointer next;

            Node(const T &value) : data(value), next(nullptr) {}

            template <typename... Args>
            Node(Args &&...args) : data(std::forward<Args>(args)...), next(nullptr)
            {
            }
        };

    public:
        using node_allocator = typename Allocator::template rebind<Node>::other;
        using value_type = T;
        using reference = T &;
        using const_reference = const T &;
        using pointer = typename node_allocator::pointer;
        using const_pointer = typename node_allocator::const_pointer;
        using size_type = size_t;

        class Iterator;
        using iterator = Iterator;
        using const_iterator = const Iterator;

        forward_list() : _head(nullptr) {}

        forward_list(const forward_list &other) : _head(nullptr)
        {
            pointer *tail = &_head;
            for (pointer node = other._head; node != nullptr; node = node->next)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_copy_constructible_v<T>)
                    (*tail)->data = node->data;
                else
                    node_allocator::construct(*tail, node->data);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        forward_list(forward_list &&other) : _head(nullptr)
        {
            _head = other._head;
            other._head = nullptr;
        }

        forward_list &operator=(const forward_list &other)
        {
            if (this != &other)
            {
                clear();
                pointer current = other._head;
                pointer *tail = &_head;
                while (current != nullptr)
                {
                    *tail = node_allocator::allocate(1);
                    if constexpr (std::is_trivially_copy_constructible_v<T>)
                        (*tail)->data = current->data;
                    else
                        node_allocator::construct(*tail, current->data);
                    (*tail)->next = nullptr;
                    tail = &((*tail)->next);
                    current = current->next;
                }
            }
            return *this;
        }

        forward_list &operator=(forward_list &&other)
        {
            if (this != &other)
            {
                clear();
                _head = other._head;
                other._head = nullptr;
            }
            return *this;
        }

        explicit forward_list(size_t count, const T &value = T()) : _head(nullptr)
        {
            pointer *tail = &_head;
            for (size_t i = 0; i < count; ++i)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_copy_constructible_v<T>)
                    (*tail)->data = value;
                else
                    node_allocator::construct(*tail, value);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        forward_list(std::initializer_list<T> list) : _head(nullptr)
        {
            pointer *tail = &_head;
            for (const T &value : list)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_copy_constructible_v<T>)
                    (*tail)->data = value;
                else
                    node_allocator::construct(*tail, value);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        forward_list &operator=(std::initializer_list<T> list)
        {
            clear();
            pointer *tail = &_head;
            for (const T &value : list)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_copy_constructible_v<T>)
                    (*tail)->data = value;
                else
                    node_allocator::construct(*tail, value);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
            return *this;
        }

        template <typename InputIt, typename = std::enable_if_t<is_input_iterator_based<InputIt>::value>>
        forward_list(InputIt first, InputIt last) : _head(nullptr)
        {
            pointer *tail = &_head;
            for (; first != last; ++first)
            {
                *tail = node_allocator::allocate(1);
                node_allocator::construct(*tail, *first);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        ~forward_list()
        {
            while (_head)
            {
                pointer p = _head;
                _head = _head->next;
                node_allocator::destroy(p);
                node_allocator::deallocate(p, 1);
            }
        }

        void clear()
        {
            pointer current = _head;
            while (current)
            {
                pointer next = current->next;
                node_allocator::destroy(current);
                node_allocator::deallocate(current, 1);
                current = next;
            }
            _head = nullptr;
        }

        reference front() { return _head->data; }

        const_reference front() const { return _head->data; }

        void push_front(const T &value)
        {
            pointer head = _head;
            _head = node_allocator::allocate(1);
            if constexpr (std::is_trivially_copyable_v<T>)
                _head->data = value;
            else
                node_allocator::construct(_head, value);
            _head->next = head;
        }

        void push_front(T &&value)
        {
            pointer head = _head;
            _head = node_allocator::allocate(1);
            if constexpr (std::is_trivially_move_constructible_v<T>)
                _head->data = std::forward<T>(value);
            else
                node_allocator::construct(_head, std::forward<T>(value));
            _head->next = head;
        }

        template <typename... Args>
        void emplace_front(Args &&...args)
        {
            pointer head = _head;
            _head = node_allocator::allocate(1);
            node_allocator::construct(_head, std::forward<Args>(args)...);
            _head->next = head;
        }

        void pop_front()
        {
            if (!_head) return;
            pointer temp = _head;
            _head = _head->next;
            node_allocator::destroy(temp);
            node_allocator::deallocate(temp, 1);
        }

        void resize(size_type count, const T &value = T())
        {
            pointer current = _head;
            pointer last = nullptr;
            size_type current_size = 0;

            while (current && current_size < count)
            {
                last = current;
                current = current->next;
                ++current_size;
            }

            if (current_size < count)
            {
                while (current_size < count)
                {
                    pointer node = node_allocator::allocate(1);
                    if constexpr (std::is_trivially_constructible_v<T>)
                        node->data = value;
                    else
                        node_allocator::construct(node, value);
                    node->next = nullptr;
                    if (last)
                        last->next = node;
                    else
                        _head = node;
                    last = node;
                    ++current_size;
                }
                if (last) last->next = nullptr;
            }
            else if (current_size > count && last)
            {
                clear_from(last->next);
                last->next = nullptr;
            }
        }

        iterator before_begin() { return iterator(nullptr); }
        const_iterator before_begin() const { return const_iterator(nullptr); }
        const_iterator cbefore_begin() const { return const_iterator(nullptr); }

        iterator begin() { return _head; }
        const_iterator begin() const { return _head; }
        const_iterator cbegin() const { return _head; }

        iterator end() { return iterator(nullptr); }
        const_iterator end() const { return const_iterator(nullptr); }
        const_iterator cend() const { return const_iterator(nullptr); }

        iterator insert_after(const_iterator pos, const_reference value);
        iterator insert_after(const_iterator pos, T &&value);
        iterator insert_after(const_iterator pos, size_type count, const_reference value);

        template <class InputIt>
        iterator insert_after(const_iterator pos, InputIt first, InputIt last);
        iterator insert_after(const_iterator pos, std::initializer_list<T> ilist);

        iterator erase_after(const_iterator pos);
        iterator erase_after(const_iterator first, const_iterator last);

        void remove(const_reference value)
        {
            pointer *current = &_head;
            while (*current != nullptr)
            {
                if ((*current)->data == value)
                {
                    pointer to_delete = *current;
                    *current = (*current)->next;
                    node_allocator::destroy(to_delete);
                    node_allocator::deallocate(to_delete, 1);
                }
                else
                    current = &((*current)->next);
            }
        }

        template <class UnaryPredicate>
        void remove_if(UnaryPredicate p)
        {
            pointer *current = &_head;
            while (*current != nullptr)
            {
                if (p((*current)->data))
                {
                    pointer to_delete = *current;
                    *current = (*current)->next;
                    node_allocator::destroy(to_delete);
                    node_allocator::deallocate(to_delete, 1);
                }
                else
                    current = &((*current)->next);
            }
        }

        void swap(forward_list &other) noexcept { std::swap(_head, other._head); }

        void merge(forward_list &other) { merge(other, std::less<T>()); }

        void merge(forward_list &&other) { merge(other, std::less<T>()); }

        template <class Compare>
        void merge(forward_list &other, Compare comp)
        {
            pointer tail = nullptr;
            pointer *lastPtr = &_head;

            while (_head != nullptr && other._head != nullptr)
            {
                if (comp(other._head->data, _head->data))
                {
                    tail = other._head;
                    other._head = other._head->next;
                    tail->next = _head;
                    _head = tail;
                }
                else
                {
                    tail = _head;
                    _head = _head->next;
                }
                lastPtr = &(tail->next);
            }

            if (other._head != nullptr) *lastPtr = other._head;
            other._head = nullptr;
        }

        template <class Compare>
        void merge(forward_list &&other, Compare comp)
        {
            merge(other, comp);
        }

        void splice_after(const_iterator pos, forward_list &other);
        void splice_after(const_iterator pos, forward_list &other, const_iterator it);
        void splice_after(const_iterator pos, forward_list &&other, const_iterator it);
        void splice_after(const_iterator pos, forward_list &&other);
        void splice_after(const_iterator pos, forward_list &other, const_iterator first, const_iterator last);
        void splice_after(const_iterator pos, forward_list &&other, const_iterator first, const_iterator last);

        void reverse() noexcept
        {
            pointer prev = nullptr;
            pointer current = _head;
            pointer next = nullptr;
            while (current != nullptr)
            {
                next = current->next;
                current->next = prev;
                prev = current;
                current = next;
            }
            _head = prev;
        }

        void assign(size_type count, const_reference value)
        {
            clear();
            pointer *tail = &_head;
            for (size_t i = 0; i < count; ++i)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_constructible_v<T>)
                    (*tail)->data = value;
                else
                    node_allocator::construct(*tail, value);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        template <class InputIt>
        void assign(InputIt first, InputIt last)
        {
            clear();
            pointer *tail = &_head;
            for (InputIt it = first; it != last; ++it)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_constructible_v<T>)
                    (*tail)->data = *it;
                else
                    node_allocator::construct(*tail, *it);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        void assign(std::initializer_list<T> ilist)
        {
            clear();
            pointer *tail = &_head;
            for (const T &value : ilist)
            {
                *tail = node_allocator::allocate(1);
                if constexpr (std::is_trivially_constructible_v<T>)
                    (*tail)->data = value;
                else
                    node_allocator::construct(*tail, value);
                (*tail)->next = nullptr;
                tail = &((*tail)->next);
            }
        }

        void unique() { unique(std::equal_to<T>()); }

        template <class BinaryPredicate>
        void unique(BinaryPredicate p)
        {
            pointer current = _head;
            while (current && current->next)
            {
                if (p(current->data, current->next->data))
                {
                    pointer to_delete = current->next;
                    current->next = to_delete->next;
                    node_allocator::destroy(to_delete);
                    node_allocator::deallocate(to_delete, 1);
                }
                else
                    current = current->next;
            }
        }

        void sort() { sort(std::less<T>()); }

        template <class Compare>
        void sort(Compare comp)
        {
            if (!_head || !_head->next) return;
            _head = mergeSort(_head, comp);
        }

    private:
        pointer _head;

        void clear_from(pointer start)
        {
            while (start)
            {
                pointer next = start->next;
                node_allocator::destroy(start);
                node_allocator::deallocate(start, 1);
                start = next;
            }
        }

        template <class Compare>
        pointer mergeSort(pointer head, Compare comp)
        {
            if (!head || !head->next) return head;
            pointer middle = getMiddle(head);
            pointer next_of_middle = middle->next;
            middle->next = nullptr;
            pointer left = mergeSort(head, comp);
            pointer right = mergeSort(next_of_middle, comp);
            return sortedMerge(left, right, comp);
        }

        template <class Compare>
        pointer sortedMerge(pointer a, pointer b, Compare comp)
        {
            pointer result = nullptr;
            if (!a)
                return b;
            else if (!b)
                return a;

            if (comp(a->data, b->data))
            {
                result = a;
                result->next = sortedMerge(a->next, b, comp);
            }
            else
            {
                result = b;
                result->next = sortedMerge(a, b->next, comp);
            }

            return result;
        }

        pointer getMiddle(pointer head)
        {
            if (!head) return head;
            pointer slow = head;
            pointer fast = head->next;
            while (fast && fast->next)
            {
                slow = slow->next;
                fast = fast->next->next;
            }
            return slow;
        }
    };

    template <typename T, typename Allocator>
    class forward_list<T, Allocator>::Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = forward_list::pointer;
        using reference = T &;

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
            Iterator tmp = *this;
            ++(*this);
            return tmp;
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
        friend class forward_list;

    private:
        pointer _ptr;
    };

    template <typename T, typename Allocator>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(const_iterator pos,
                                                                                  const_reference value)
    {
        pointer node = pos._ptr;
        pointer new_node = node_allocator::allocate(1);
        if constexpr (std::is_trivially_constructible_v<T>)
            new_node->data = value;
        else
            node_allocator::construct(new_node, value);
        new_node->next = node->next;
        node->next = new_node;
        return iterator(new_node);
    }

    template <typename T, typename Allocator>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(const_iterator pos, T &&value)
    {
        pointer node = pos._ptr;
        pointer new_node = node_allocator::allocate(1);
        if constexpr (std::is_trivially_move_constructible_v<T>)
            new_node->data = std::forward<T>(value);
        else
            node_allocator::construct(new_node, std::forward<T>(value));
        new_node->next = node->next;
        node->next = new_node;
        return iterator(new_node);
    }

    template <typename T, typename Allocator>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(const_iterator pos, size_type count,
                                                                                  const_reference value)
    {
        pointer node = pos._ptr;
        pointer last = node;
        for (size_type i = 0; i < count; ++i)
        {
            pointer new_node = node_allocator::allocate(1);
            if constexpr (std::is_trivially_copy_constructible_v<T>)
                new_node->data = value;
            else
                node_allocator::construct(new_node, value);
            new_node->next = last->next;
            last->next = new_node;
            last = new_node;
        }
        return iterator(last);
    }

    template <typename T, typename Allocator>
    template <class InputIt>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(const_iterator pos, InputIt first,
                                                                                  InputIt last)
    {
        pointer node = pos._ptr;
        pointer last_node = nullptr;
        for (InputIt it = first; it != last; ++it)
        {
            pointer new_node = node_allocator::allocate(1);
            if constexpr (std::is_trivially_copy_constructible_v<T>)
                new_node->data = *it;
            else
                node_allocator::construct(new_node, *it);
            new_node->next = last_node->next;
            last_node->next = new_node;
            last_node = new_node;
        }
        return iterator(last_node);
    }

    template <typename T, typename Allocator>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::insert_after(const_iterator pos,
                                                                                  std::initializer_list<T> ilist)
    {
        pointer node = pos._ptr;
        pointer last_inserted = node;
        for (const T &value : ilist)
        {
            pointer new_node = node_allocator::allocate(1);
            if constexpr (std::is_trivially_copy_constructible_v<T>)
                new_node->data = value;
            else
                node_allocator::construct(new_node, value);
            new_node->next = last_inserted->next;
            last_inserted->next = new_node;
            last_inserted = new_node;
        }
        return iterator(last_inserted);
    }

    template <typename T, typename Allocator>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::erase_after(const_iterator pos)
    {
        pointer current = pos._ptr;
        if (current && current->next)
        {
            pointer to_delete = current->next;
            current->next = to_delete->next;
            node_allocator::destroy(to_delete);
            node_allocator::deallocate(to_delete, 1);
        }
        return iterator(current->next);
    }

    template <typename T, typename Allocator>
    typename forward_list<T, Allocator>::iterator forward_list<T, Allocator>::erase_after(const_iterator first,
                                                                                 const_iterator last)
    {
        pointer current = first._ptr;
        if (current)
        {
            pointer stop = last._ptr;
            pointer to_delete = current->next;
            while (to_delete != stop)
            {
                pointer next = to_delete->next;
                node_allocator::destroy(to_delete);
                node_allocator::deallocate(to_delete, 1);
                to_delete = next;
            }
            current->next = stop;
        }
        return iterator(last);
    }

    template <typename T, typename Allocator>
    void forward_list<T, Allocator>::splice_after(const_iterator pos, forward_list &other)
    {
        if (other._head == nullptr || pos._ptr == other._head) return;
        pointer splice_start = other._head;
        pointer splice_end = nullptr;
        while (splice_start->next != nullptr)
        {
            splice_end = splice_start;
            splice_start = splice_start->next;
        }
        if (splice_end) splice_end->next = pos._ptr->next;
        pos._ptr->next = other._head;
        other._head = nullptr;
    }

    template <typename T, typename Allocator>
    void forward_list<T, Allocator>::splice_after(const_iterator pos, forward_list &other, const_iterator it)
    {
        if (it._ptr == nullptr || it._ptr->next == nullptr) return;
        pointer node_to_splice = it._ptr->next;
        it._ptr->next = node_to_splice->next;
        node_to_splice->next = pos._ptr->next;
        pos._ptr->next = node_to_splice;
    }

    template <typename T, typename Allocator>
    void forward_list<T, Allocator>::splice_after(const_iterator pos, forward_list &&other, const_iterator it)
    {
        splice_after(pos, other, it);
    }

    template <typename T, typename Allocator>
    void forward_list<T, Allocator>::splice_after(const_iterator pos, forward_list &&other)
    {
        splice_after(pos, other);
    }

    template <typename T, typename Allocator>
    void forward_list<T, Allocator>::splice_after(const_iterator pos, forward_list &other, const_iterator first,
                                                  const_iterator last)
    {
        if (first._ptr == last._ptr || first._ptr->next == nullptr) return;
        pointer first_node = first._ptr->next;
        pointer last_node = first_node;
        while (last_node->next != last._ptr && last_node->next != nullptr) last_node = last_node->next;
        first._ptr->next = last._ptr;
        last_node->next = pos._ptr->next;
        pos._ptr->next = first_node;
    }

    template <typename T, typename Allocator>
    void forward_list<T, Allocator>::splice_after(const_iterator pos, forward_list &&other, const_iterator first,
                                                  const_iterator last)
    {
        splice_after(pos, other, first, last);
    }
} // namespace acul