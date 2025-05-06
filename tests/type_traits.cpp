#include <acul/list.hpp>
#include <acul/type_traits.hpp>
#include <acul/vector.hpp>
#include <cassert>
#include <iterator>

void test_lambda_arg_traits()
{
    auto lambda = [](int x) { return x + 1; };
    using arg_type = typename acul::lambda_arg_traits<decltype(lambda)>::argument_type;
    assert((std::is_same_v<arg_type, int>));

    std::function<float(float)> func = [](float f) { return f * 2.0f; };
    using arg_type_func = typename acul::lambda_arg_traits<decltype(func)>::argument_type;
    assert((std::is_same_v<arg_type_func, float>));

    auto normal_func = [](double) -> double { return 0.0; };
    using normal_func_arg = typename acul::lambda_arg_traits<decltype(normal_func)>::argument_type;
    assert((std::is_same_v<normal_func_arg, double>));
}

void test_iterator_traits()
{
    {
        // input_iterator (istream_iterator)
        bool is_input = acul::is_input_iterator<std::istream_iterator<int>>::value;
        assert(is_input);

        bool is_forward = acul::is_forward_iterator<std::istream_iterator<int>>::value;
        assert(!is_forward);

        bool is_forward_based = acul::is_forward_iterator_based<std::istream_iterator<int>>::value;
        assert(!is_forward_based);
    }
    {
        // random_access_iterator (vector)
        bool is_input = acul::is_input_iterator<acul::vector<int>::iterator>::value;
        assert(!is_input);

        bool is_forward = acul::is_forward_iterator<acul::vector<int>::iterator>::value;
        assert(!is_forward);
        bool is_forward_based = acul::is_forward_iterator_based<acul::vector<int>::iterator>::value;
        assert(is_forward_based);
    }
    {
        // bidirectional_iterator (list)
        bool is_forward = acul::is_forward_iterator<acul::list<int>::iterator>::value;
        assert(!is_forward);
        bool is_forward_based = acul::is_forward_iterator_based<acul::list<int>::iterator>::value;
        assert(is_forward_based);
    }
}

struct A
{
};
struct B : A
{
};

void test_is_same_base()
{
    bool base1 = acul::is_same_base<B, A>::value;
    bool base2 = acul::is_same_base<A, B>::value;
    bool base3 = acul::is_same_base<A, A>::value;
    bool base4 = acul::is_same_base<B, B>::value;

    assert(base1);
    assert(base2);
    assert(base3);
    assert(base4);
}

void test_has_args()
{
    bool no_args = acul::has_args<>();
    bool one_arg = acul::has_args<int>();
    bool multiple_args = acul::has_args<int, double, char>();

    assert(!no_args);
    assert(one_arg);
    assert(multiple_args);
}

void test_is_char_and_integer()
{
    bool is_char1 = acul::is_char<char>::value;
    bool is_char2 = acul::is_char<wchar_t>::value;
    bool is_nonchar1 = acul::is_nonchar_integer<int>::value;
    bool is_nonchar2 = acul::is_nonchar_integer<unsigned short>::value;
    bool is_nonchar3 = acul::is_nonchar_integer<char>::value;

    assert(is_char1);
    assert(is_char2);
    assert(is_nonchar1);
    assert(is_nonchar2);
    assert(!is_nonchar3);
}

void test_type_traits()
{
    test_lambda_arg_traits();
    test_iterator_traits();
    test_is_same_base();
    test_has_args();
    test_is_char_and_integer();
}
