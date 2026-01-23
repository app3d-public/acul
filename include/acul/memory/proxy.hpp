#pragma once

namespace acul
{
    template <typename T>
    class proxy
    {
    public:
        proxy(T *ptr = nullptr) : _ptr(ptr) {}

        T *operator->() { return _ptr; }
        const T *operator->() const { return _ptr; }

        T &operator*() { return *_ptr; }
        const T &operator*() const { return *_ptr; }

        operator bool() const { return _ptr != nullptr; }

        void set(T *ptr) { _ptr = ptr; }

        T *get() { return _ptr; }

        void reset() { _ptr = nullptr; }

        void operator=(T *ptr) { _ptr = ptr; }
        bool operator==(T *ptr) { return _ptr == ptr; }
        bool operator!=(T *ptr) { return _ptr != ptr; }

        proxy &operator++()
        {
            ++_ptr;
            return *this;
        }

        proxy operator++(int)
        {
            proxy temp = *this;
            ++_ptr;
            return temp;
        }

        proxy &operator--()
        {
            --_ptr;
            return *this;
        }

        proxy operator--(int)
        {
            proxy temp = *this;
            --_ptr;
            return temp;
        }

    private:
        T *_ptr;
    };
} // namespace acul