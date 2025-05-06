#include <acul/event.hpp>
#include <cassert>

using namespace acul;
using namespace acul::events;

struct my_event : public event
{
    my_event(u64 id) : event(id) {}
};

void test_event()
{
    dispatcher disp;

    // Construct
    event e1(0x3FBC359786F6B8AF);
    event e2(0x3966512BA8F61A58);
    assert(e1 != e2);
    assert(e1 == event(0x3FBC359786F6B8AF));

    // Hash
    assert(std::hash<events::event>{}(e1) == 0x3FBC359786F6B8AF);

    // make_pre_event_id / make_post_event_id
    u64 id = 10;
    assert(make_pre_event_id(id) == (id | 0x4000000000000000ULL));
    assert(make_post_event_id(id) == (id | 0x8000000000000000ULL));

    // Bind & Dispatch
    int call_count = 0;

    void *owner = &call_count;
    disp.bind_event(owner, 0x30EA6A6AC2D99A37, [&](my_event &e) { call_count++; });

    assert(disp.exist(0x30EA6A6AC2D99A37));

    my_event event_instance(0x30EA6A6AC2D99A37);
    disp.dispatch(event_instance);
    disp.dispatch(0x30EA6A6AC2D99A37);
    assert(call_count == 2);

    // data_event check
    data_event<int> d_event(0x1CE4F151413C7BE9, 777);
    assert(d_event.data == 777);

    bool called2 = false;

    disp.addListener<event>(0x1CE4F151413C7BE9, [&called2](event &e) {
        auto &ev = static_cast<data_event<int> &>(e);
        assert(ev.data == 777);
        called2 = true;
    });

    disp.dispatch(d_event);
    assert(called2);

    // remove_listener
    disp.unbind_listeners(owner);
    assert(!disp.exist(0x30EA6A6AC2D99A37));

    // clear()
    disp.clear();
}