#pragma once

namespace acul
{
    template <typename T>
    class basic_string_view;

    using string_view = basic_string_view<char>;
    using u16string_view = basic_string_view<char16_t>;
    using wstring_view = basic_string_view<wchar_t>;
} // namespace acul