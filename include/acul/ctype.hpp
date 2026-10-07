#pragma once

namespace acul
{
    constexpr bool is_ascii(char c) noexcept { return static_cast<unsigned char>(c) <= 0x7F; }
    constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
    constexpr bool is_lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
    constexpr bool is_upper(char c) noexcept { return c >= 'A' && c <= 'Z'; }
    constexpr bool is_alpha(char c) noexcept { return is_lower(c) || is_upper(c); }
    constexpr bool is_alnum(char c) noexcept { return is_alpha(c) || is_digit(c); }
    constexpr bool is_blank(char c) noexcept { return c == ' ' || c == '\t'; }
    constexpr bool is_space(char c) noexcept { return c == ' ' || (c >= '\t' && c <= '\r'); }

    constexpr bool is_control(char c) noexcept
    {
        const unsigned char u = static_cast<unsigned char>(c);
        return u <= 0x1F || u == 0x7F;
    }

    constexpr bool is_print(char c) noexcept
    {
        const unsigned char u = static_cast<unsigned char>(c);
        return u >= 0x20 && u <= 0x7E;
    }

    constexpr bool is_graph(char c) noexcept
    {
        const unsigned char u = static_cast<unsigned char>(c);
        return u >= 0x21 && u <= 0x7E;
    }

    constexpr bool is_hex_digit(char c) noexcept
    {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    constexpr bool is_punct(char c) noexcept { return is_graph(c) && !is_alnum(c); }

    constexpr char to_lower(char c) noexcept
    {
        if (is_upper(c)) return static_cast<char>(c + ('a' - 'A'));
        return c;
    }

    constexpr char to_upper(char c) noexcept
    {
        if (is_lower(c)) return static_cast<char>(c - ('a' - 'A'));
        return c;
    }
} // namespace acul