#pragma once

#include <functional>

namespace astl
{

    template <typename T>
    struct lambda_arg_traits;

    template <typename R, typename Arg>
    struct lambda_arg_traits<R(Arg)>
    {
        using argument_type = Arg;
    };

    // std::function
    template <typename R, typename Arg>
    struct lambda_arg_traits<std::function<R(Arg)>>
    {
        using argument_type = Arg;
    };

    // Lambdas and other callers
    template <typename T>
    struct lambda_arg_traits : lambda_arg_traits<decltype(&T::operator())>
    {
    };

    // Pointers to members
    template <typename ClassType, typename ReturnType, typename Arg>
    struct lambda_arg_traits<ReturnType (ClassType::*)(Arg) const>
    {
        using argument_type = Arg;
    };
} // namespace astl