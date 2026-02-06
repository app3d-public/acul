#pragma once

#include <cassert>
#include <type_traits>
#include <utility>
#include "../fwd/functional.hpp"

namespace acul
{
    template <class R, class... A, size_t S, template <class> class Alloc>
    class unique_function<R(A...), S, Alloc>
    {
        static constexpr size_t storage_align = alignof(void *);
        struct vtable_t
        {
            R (*invoke)(void *, A &&...);
            void (*destroy)(void *);
            void (*move)(void *, void *);
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
                                     }};
            return &vt;
        }

        template <class F>
        void set(F &&f)
        {
            using Fn = std::decay_t<F>;
            reset();

            if constexpr (std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>)
                if (!f) return;

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

        void move_from(unique_function &other) noexcept
        {
            _vt = other._vt;
            if (!_vt) return;
            _vt->move((void *)_storage, (void *)other._storage);
            other._vt = nullptr;
        }

    public:
        using result_type = R;

        unique_function() noexcept = default;
        unique_function(std::nullptr_t) noexcept {}

        ~unique_function() { reset(); }

        unique_function(unique_function &&other) noexcept { move_from(other); }
        unique_function &operator=(unique_function &&other) noexcept
        {
            if (this != &other)
            {
                reset();
                move_from(other);
            }
            return *this;
        }

        unique_function(const unique_function &) = delete;
        unique_function &operator=(const unique_function &) = delete;

        template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, unique_function>>>
        unique_function(F &&f)
        {
            set(std::forward<F>(f));
        }

        template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, unique_function>>>
        unique_function &operator=(F &&f)
        {
            set(std::forward<F>(f));
            return *this;
        }

        unique_function &operator=(std::nullptr_t) noexcept
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

        void swap(unique_function &other) noexcept
        {
            if (this == &other) return;
            unique_function tmp(std::move(other));
            other = std::move(*this);
            *this = std::move(tmp);
        }
    };
} // namespace acul
