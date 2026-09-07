#include <acul/memory/smart_ptr.hpp>
#include <cassert>

struct Dummy
{
    int value = 42;
};

struct PolymorphicStorage
{
    virtual ~PolymorphicStorage() = default;
};

struct PolymorphicView
{
    virtual ~PolymorphicView() = default;
};

struct AliasedDummy final : PolymorphicStorage, PolymorphicView
{
    explicit AliasedDummy(bool &destroyed) : destroyed(destroyed) {}
    ~AliasedDummy() override { destroyed = true; }

    bool &destroyed;
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

    bool destroyed = false;
    auto *raw = acul::alloc<AliasedDummy>(destroyed);
    acul::shared_ptr<PolymorphicStorage> storage(static_cast<PolymorphicStorage *>(raw));
    auto concrete = acul::static_pointer_cast<AliasedDummy>(storage);
    acul::shared_ptr<PolymorphicView> view = concrete;
    storage.reset();
    concrete.reset();
    assert(!destroyed);
    view.reset();
    assert(destroyed);

    destroyed = false;
    auto owned = acul::make_shared<AliasedDummy>(destroyed);
    auto *expected_storage = static_cast<PolymorphicStorage *>(owned.get());
    auto *expected_view = static_cast<PolymorphicView *>(owned.get());
    acul::shared_ptr<PolymorphicStorage> owned_storage = owned;
    acul::shared_ptr<PolymorphicView> owned_view = owned;
    assert(owned_storage.get() == expected_storage);
    assert(owned_view.get() == expected_view);
    assert(static_cast<void *>(owned_storage.get()) != static_cast<void *>(owned_view.get()));
    owned.reset();
    owned_storage.reset();
    assert(!destroyed);
    owned_view.reset();
    assert(destroyed);

    destroyed = false;
    raw = acul::alloc<AliasedDummy>(destroyed);
    acul::shared_ptr<PolymorphicView> external_view(static_cast<PolymorphicView *>(raw));
    external_view.reset();
    assert(destroyed);

    destroyed = false;
    acul::shared_ptr<PolymorphicView> assigned_view;
    {
        auto concrete_owner = acul::make_shared<AliasedDummy>(destroyed);
        assigned_view = concrete_owner;
        assert(concrete_owner.use_count() == 2u);
    }
    assert(!destroyed);
    assigned_view.reset();
    assert(destroyed);
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
