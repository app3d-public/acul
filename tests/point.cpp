#include <acul/point.hpp>
#include <cassert>
#include <type_traits>

void test_point()
{
    using acul::ipoint;
    const ipoint a{12, -6};
    const ipoint b{4, 2};
    static_assert(std::is_same_v<decltype(a + b), ipoint>);
    static_assert(std::is_same_v<decltype(a - b), ipoint>);
    static_assert(std::is_same_v<decltype(-a), ipoint>);
    static_assert(std::is_same_v<decltype(a * 2), ipoint>);
    static_assert(std::is_same_v<decltype(a / 2), ipoint>);
    assert((a + b == ipoint{16, -4}));
    assert((a - b == ipoint{8, -8}));
    assert((-a == ipoint{-12, 6}));
    assert((a * 2 == ipoint{24, -12}));
    assert((a / 2 == ipoint{6, -3}));
    assert((a == ipoint{12, -6}));

    ipoint value = a;
    assert(&(value += b) == &value);
    assert((value == ipoint{16, -4}));
    assert(&(value -= b) == &value);
    assert(value == a);

    const acul::fpoint32 dpi{144.f, 192.f};
    assert((dpi / 96 == acul::fpoint32{1.5f, 2.f}));
}
