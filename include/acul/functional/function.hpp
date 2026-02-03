#pragma once

#include <cassert>
#include <type_traits>
#include <utility>
#include "../fwd/functional.hpp"

namespace acul
{
    template <class R, class... A, size_t S, template <class> class Alloc>
    class function<R(A...), S, Alloc>
    {
        static constexpr size_t storage_align = alignof(void *);
        struct vtable_t
        {
            R (*invoke)(void *, A &&...);
            void (*destroy)(void *);
            void (*move)(void *, void *);
            void (*copy)(void *, const void *);
        };

        alignas(storage_align) unsigned char _storage[S];
        const vtable_t *_vt = nullptr;

        template <class T>
        static T *&as_ptr(void *storage) noexcept
        {
            static_assert(S >= sizeof(void *), "SBO must be >= sizeof(void*) to allow heap fallback");
            return *reinterpret_cast<T **>(storage);
        }

        template <class T>
        static T *const &as_ptr(const void *storage) noexcept
        {
            static_assert(S >= sizeof(void *), "SBO must be >= sizeof(void*) to allow heap fallback");
            return *reinterpret_cast<T *const *>(storage);
        }

        template <class Fn>
        static const vtable_t *vt_inplace() noexcept
        {
            static const vtable_t vt{// invoke
                                     [](void *s, A &&...a) -> R {
                                         auto &f = *reinterpret_cast<Fn *>(s);
                                         if constexpr (std::is_void_v<R>) { f(std::forward<A>(a)...); }
                                         else
                                         {
                                             return f(std::forward<A>(a)...);
                                         }
                                     },
                                     // destroy
                                     [](void *s) { reinterpret_cast<Fn *>(s)->~Fn(); },
                                     // move
                                     [](void *dst, void *src) {
                                         Alloc<Fn>::construct(reinterpret_cast<Fn *>(dst),
                                                              std::move(*reinterpret_cast<Fn *>(src)));
                                         reinterpret_cast<Fn *>(src)->~Fn();
                                     },
                                     // copy
                                     [](void *dst, const void *src) {
                                         Alloc<Fn>::construct(reinterpret_cast<Fn *>(dst),
                                                              *reinterpret_cast<const Fn *>(src));
                                     }};
            return &vt;
        }

        template <class Fn>
        static const vtable_t *vt_heap() noexcept
        {
            static const vtable_t vt{// invoke (storage contains Fn*)
                                     [](void *s, A &&...a) -> R {
                                         Fn *p = as_ptr<Fn>(s);
                                         return (*p)(std::forward<A>(a)...);
                                     },
                                     // destroy
                                     [](void *s) {
                                         Fn *p = as_ptr<Fn>(s);
                                         if constexpr (!std::is_trivially_destructible_v<Fn>) Alloc<Fn>::destroy(p);
                                         Alloc<Fn>::deallocate(p);
                                         as_ptr<Fn>(s) = nullptr;
                                     },
                                     // move
                                     [](void *dst, void *src) {
                                         as_ptr<Fn>(dst) = as_ptr<Fn>(src);
                                         as_ptr<Fn>(src) = nullptr;
                                     },
                                     // copy
                                     [](void *dst, const void *src) {
                                         Fn *src_p = as_ptr<Fn>(src);
                                         Fn *p = Alloc<Fn>::allocate(1);
                                         Alloc<Fn>::construct(p, *src_p);
                                         as_ptr<Fn>(dst) = p;
                                     }};
            return &vt;
        }

        template <class F>
        void set(F &&f)
        {
            using Fn = std::decay_t<F>;
            static_assert(std::is_copy_constructible_v<Fn>, "function target must be copy constructible");
            reset();

            if constexpr (sizeof(Fn) <= S && alignof(Fn) <= storage_align)
            {
                Alloc<Fn>::construct(reinterpret_cast<Fn *>(_storage), std::forward<F>(f));
                _vt = vt_inplace<Fn>();
            }
            else
            {
                static_assert(S >= sizeof(void *), "SBO too small for heap fallback (must fit a pointer)");
                Fn *p = Alloc<Fn>::allocate(1);
                Alloc<Fn>::construct(p, std::forward<F>(f));
                as_ptr<Fn>(_storage) = p;
                _vt = vt_heap<Fn>();
            }
        }

        void move_from(function &other) noexcept
        {
            _vt = other._vt;
            if (!_vt) return;
            _vt->move((void *)_storage, (void *)other._storage);
            other._vt = nullptr;
        }

        void copy_from(const function &other)
        {
            _vt = other._vt;
            if (!_vt) return;
            _vt->copy((void *)_storage, (const void *)other._storage);
        }

    public:
        using result_type = R;

        function() noexcept = default;
        function(std::nullptr_t) noexcept {}

        ~function() { reset(); }

        function(function &&other) noexcept { move_from(other); }
        function &operator=(function &&other) noexcept
        {
            if (this != &other)
            {
                reset();
                move_from(other);
            }
            return *this;
        }

        function(const function &other) { copy_from(other); }
        function &operator=(const function &other)
        {
            if (this != &other)
            {
                reset();
                copy_from(other);
            }
            return *this;
        }

        template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function>>>
        function(F &&f)
        {
            set(std::forward<F>(f));
        }

        template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, function>>>
        function &operator=(F &&f)
        {
            set(std::forward<F>(f));
            return *this;
        }

        function &operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        R operator()(A... args) const noexcept
        {
            assert(_vt);
            return _vt->invoke((void *)_storage, std::forward<A>(args)...);
        }
        explicit operator bool() const noexcept { return _vt != nullptr; }

        void reset() noexcept
        {
            if (_vt) _vt->destroy((void *)_storage);
            _vt = nullptr;
        }

        void swap(function &other) noexcept
        {
            if (this == &other) return;
            function tmp(std::move(other));
            other = *this;
            *this = std::move(tmp);
        }
    };
} // namespace acul
