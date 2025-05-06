#include <acul/list.hpp>
#include <acul/string/string.hpp>
#include <acul/string/utils.hpp>
#include <cassert>

void test_list_construct()
{
    acul::list<int> i0;
    assert(i0.empty());

    acul::list<int> i1(10, 256);
    assert(i1.size() == 10);

    acul::list<int> i2 = i1;
    assert(i2.size() == i1.size());

    acul::list<int> i3(i2);
    assert(!i3.empty());
}

void test_list_resize()
{
    acul::list<float> f0;
    f0.resize(10, 5.0f);
    assert(f0.size() == 10);
}

void test_list_it()
{
    acul::list<acul::string> s0;
    for (int i = 0; i < 10; ++i) s0.push_back("t" + acul::to_string(i));
    auto it = std::find_if(s0.begin(), s0.end(), [](const acul::string &str) { return str == "t1"; });
    assert(it != s0.end());
}

void test_list_front_back()
{
    acul::list<int> v;
    v.push_back(5);
    v.push_front(10);
    v.emplace_back(15);

    assert(v.front() == 10);
    assert(v.back() == 15);

    v.front() = 1;
    v.back() = 99;

    assert(v.front() == 1);
    assert(v.back() == 99);
}

void test_list_foreach()
{
    acul::list<uint32_t> u0(10, 5);
    int counter = 0;
    for (uint32_t v : u0) ++counter;
    assert(counter == 10);
}

void test_list()
{
    test_list_construct();
    test_list_resize();
    test_list_it();
    test_list_front_back();
}