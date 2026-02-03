#include <acul/memory/alloc.hpp>
//
#include <acul/functional/function.hpp>
#include <acul/functional/unique_function.hpp>

using namespace acul;

void test_function()
{
    int data = 0;
    function<void()> fn_void = [&data]() { data = 1; };
    fn_void();
    assert(data == 1);

    function<int(int, int)> fn_int = [](int a, int b) { return a + b; };
    assert(fn_int(2, 3) == 5);
}

void test_unique_function()
{
    int data = 0;
    unique_function<void()> fn_void = [&data]() { data = 1; };
    fn_void();
    assert(data == 1);

    unique_function<int(int, int)> fn_int = [](int a, int b) { return a + b; };
    assert(fn_int(2, 3) == 5);
}

void test_functional()
{
    test_function();
    test_unique_function();
}
