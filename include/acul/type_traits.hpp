#pragma once

#include <functional>

namespace acul
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

    template <typename Iter>
    using is_input_iterator = std::integral_constant<
        bool,
        std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<Iter>::iterator_category> &&
            !std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>>;

    template <typename Iter>
    using is_input_iterator_based = std::integral_constant<
        bool, std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>>;

    template <typename Iter>
    using is_forward_iterator = std::integral_constant<
        bool, std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category> &&
                  !std::is_base_of_v<std::bidirectional_iterator_tag,
                                     typename std::iterator_traits<Iter>::iterator_category>>;

    template <typename Iter>
    using is_forward_iterator_based = std::integral_constant<
        bool, std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>>;

    template <typename T, typename U>
    using is_same_base = std::integral_constant<bool, std::is_base_of_v<U, T> || std::is_base_of_v<T, U>>;

    template <typename... Args>
    inline constexpr bool has_args()
    {
        return sizeof...(Args) > 0;
    }

    template <typename T>
    using is_char = std::integral_constant<bool, std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
                                                     std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> ||
                                                     std::is_same_v<T, char32_t>>;

    template <typename T>
    using is_nonchar_integer = std::integral_constant<bool, std::is_integral_v<T> && !is_char<T>::value>;
} // namespace acul