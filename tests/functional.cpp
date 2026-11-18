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

namespace
{
    struct call_failure
    {
        int code;
    };
    struct large_throwing_callable
    {
        int payload[32] = {73};
        int operator()() const { throw call_failure{payload[0]}; }
    };
    template <typename Callable>
    void expect_call_failure(const Callable &callable, int code)
    {
        bool caught = false;
        try
        {
            callable();
        }
        catch (const call_failure &error)
        {
            caught = error.code == code;
        }
        assert(caught);
    }
    void test_exception_propagation()
    {
        static_assert(!noexcept(std::declval<const function<int()> &>()()));
        static_assert(!noexcept(std::declval<const unique_function<int()> &>()()));
        function<int()> small = []() -> int { throw call_failure{41}; };
        function<int()> large = large_throwing_callable{};
        expect_call_failure(small, 41);
        expect_call_failure(large, 73);
        auto small_copy = small;
        auto large_copy = large;
        expect_call_failure(small_copy, 41);
        expect_call_failure(large_copy, 73);
        unique_function<int()> unique_small = []() -> int { throw call_failure{41}; };
        unique_function<int()> unique_large = large_throwing_callable{};
        expect_call_failure(unique_small, 41);
        expect_call_failure(unique_large, 73);
        auto moved = std::move(unique_large);
        expect_call_failure(moved, 73);
    }
} // namespace

void test_functional()
{
    test_function();
    test_unique_function();
    test_exception_propagation();
}
