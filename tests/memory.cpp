#include <acul/memory/smart_ptr.hpp>
#include <cassert>

struct Dummy
{
    int value = 42;
};

void test_shared_ptr()
{
    auto p1 = acul::make_shared<Dummy>();
    assert(p1);
    assert(p1->value == 42);

    auto p2 = p1;
    assert(p1.use_count() == 2);
    assert(p2.use_count() == 2);

    auto p3 = std::move(p2);
    assert(p1.use_count() == 2);
    assert(p2 == nullptr);

    p1.reset();
    assert(p3.use_count() == 1);

    p3.reset();
}

void test_weak_ptr()
{
    acul::weak_ptr<Dummy> wp;
    {
        acul::shared_ptr<Dummy> sp = acul::make_shared<Dummy>();
        wp = sp;

        assert(!wp.expired());

        auto locked = wp.lock();
        assert(locked);
        assert(locked->value == 42);

        sp.reset();
    }
    assert(wp.expired());
    assert(!wp.lock());
}

struct ComplexDummy
{
    int a;
    float b;

    ComplexDummy(int a_, float b_) : a(a_), b(b_) {}
};

void test_unique_ptr()
{
    acul::unique_ptr<Dummy> uptr(acul::alloc<Dummy>());
    assert(uptr);
    assert(uptr->value == 42);

    auto raw = uptr.get();
    assert(raw != nullptr);

    acul::unique_ptr<Dummy> moved = std::move(uptr);
    assert(!uptr);
    assert(moved);
    assert(moved.get() == raw);

    moved.reset();
    assert(!moved);

    // Non-Trivial
    auto uptr2 = acul::make_unique<ComplexDummy>(5, 3.14f);
    assert(uptr2);
    assert(uptr2->a == 5);
    assert(uptr2->b == 3.14f);
}

void test_alloc_release()
{
    Dummy *obj = acul::alloc<Dummy>();
    assert(obj);
    assert(obj->value == 42);

    acul::release(obj);
}

void test_alloc_array_release()
{
    Dummy *arr = acul::alloc_n<Dummy>(10);
    assert(arr);

    for (int i = 0; i < 10; ++i) assert(arr[i].value == 42);

    acul::release(arr);
}

void test_memory()
{
    test_shared_ptr();
    test_weak_ptr();
    test_unique_ptr();
    test_alloc_release();
    test_alloc_array_release();
}