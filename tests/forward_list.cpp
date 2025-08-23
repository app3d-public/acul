#include <acul/forward_list.hpp>
#include <acul/string/string.hpp>
#include <acul/string/utils.hpp>
#include <cassert>

void test_forward_list_construct()
{
    acul::forward_list<int> i0;
    assert(i0.begin() == i0.end());

    acul::forward_list<int> i1(10, 256);
    assert(std::distance(i1.begin(), i1.end()) == 10);

    acul::forward_list<int> i2 = i1;
    assert(i2.front() == i1.front());

    acul::forward_list<int> i3(i2);
    assert(i3.front() == i2.front());
}

void test_forward_list_resize()
{
    acul::forward_list<float> f0;
    f0.resize(10, 5.0f);
    assert(std::distance(f0.begin(), f0.end()) == 10);
}

void test_forward_list_it()
{
    acul::forward_list<acul::string> s0;
    for (int i = 0; i < 10; ++i) s0.push_front("t" + acul::to_string(i));
    auto it = std::find_if(s0.begin(), s0.end(), [](const acul::string &str) { return str == "t1"; });
    assert(it != s0.end());
}

void test_forward_list_front_back()
{
    acul::forward_list<int> v;
    v.push_front(5);
    v.push_front(10);
    v.emplace_front(15);

    assert(v.front() == 15);

    v.front() = 1;
    assert(v.front() == 1);
}

void test_forward_list_foreach()
{
    acul::forward_list<uint32_t> u0(10, 5);
    int counter = 0;
    for (uint32_t _: u0) ++counter;
    assert(counter == 10);
}

void test_forward_list()
{
    test_forward_list_construct();
    test_forward_list_resize();
    test_forward_list_it();
    test_forward_list_front_back();
}