#pragma once

namespace acul
{
    // Base structure for objects that require manual cleanup via a function pointer.
    template <typename... Args>
    struct destructible_data
    {
        using PFN_destruct = void (*)(Args...);
        PFN_destruct destruct = nullptr;
    };

    // A container that pairs a typed value with its destruction logic.
    template <typename T, typename... Args>
    struct destructible_value : destructible_data<Args...>
    {
        T value;

        destructible_value(const T &v) : destructible_data<Args...>{}, value(v) {}
    };
} // namespace acul