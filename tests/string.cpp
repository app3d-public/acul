#include <acul/string/refstring.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/string.hpp>
#include <acul/string/string_view.hpp>
#include <acul/string/string_view_pool.hpp>
#include <acul/string/utils.hpp>
#include <cassert>


void test_basic_string()
{
    acul::string str("hello");
    assert(str.size() == 5);
    assert(str == "hello");

    acul::string copy = str;
    assert(copy == "hello");

    str += " world";
    assert(str == "hello world");

    copy = std::move(str);
    assert(copy == "hello world");
}

void test_refstring()
{
    using namespace acul;

    {
        refstring str1("hello");
        assert(str1.c_str() != nullptr);
        assert(std::string(str1.c_str()) == "hello");

        // Copy
        refstring str2 = str1;
        assert(str2.c_str() == str1.c_str());
        assert(std::string(str2.c_str()) == "hello");

        // Assign
        refstring str3;
        str3 = str2;
        assert(str3.c_str() == str1.c_str());
        assert(std::string(str3.c_str()) == "hello");

        str3 = "world";
        assert(std::string(str3.c_str()) == "world");
        assert(str3.c_str() != str1.c_str());
    }
}

void test_sstream()
{
    {
        acul::stringstream ss;
        ss << "Hello" << " " << 123 << '1';
        assert(ss.str() == "Hello 1231");

        ss.clear();
        assert(ss.str().empty());
    }
    {
        acul::string_view view = "hello_view";
        acul::stringstream ss;
        ss << view;
        assert(ss.str() == "hello_view");
    }
}

void test_string_view_pool()
{
    {
        acul::string_view_pool<char> pool;
        pool.reserve(32);
        assert(pool.empty());

        pool.push("hello", 5);
        pool.push("world", 5);

        assert(pool.size() == 2);
        assert(acul::string(pool[0]) == "hello");
        assert(acul::string(pool[1]) == "world");

        pool.clear();
        assert(pool.empty());
    }
    {

        acul::string_view_pool<char> pool;
        pool.reserve(5);
        pool.push("abc", 3);
        assert(pool.size() == 1);
        assert(strcmp(pool[0].data(), "abc") == 0);

        pool.push("defg", 4);
        assert(pool.size() == 2);
        assert(strcmp(pool[1].data(), "defg") == 0);
        assert(strcmp(pool[0].data(), "abc") == 0);
    }
}

void test_string_view()
{
    acul::string_view sv("hello world");
    assert(sv.size() == 11);
    assert(sv.substr(0, 5) == "hello");

    assert(*sv.begin() == 'h');
    assert(*(sv.end() - 1) == 'd');

    auto find_pos = sv.find('w');
    assert(find_pos != acul::string::npos);

    acul::string_view sv_default;
    assert(sv_default.empty());

    acul::string_view sv_null(nullptr);
    assert(sv_null.empty());

    acul::string_view sv2("hello");
    acul::string_view sub = sv2.substr(10);
    assert(sub.empty());

    size_t pos = sv.find('z');
    assert(pos == acul::string::npos);
}

void test_utils()
{
    // utf8_to_utf16 / utf16_to_utf8
    acul::string utf8 = "Hello";
    acul::u16string utf16 = acul::utf8_to_utf16(utf8);
    acul::string back_to_utf8 = acul::utf16_to_utf8(utf16);
    assert(utf8 == back_to_utf8);

    // trim
    acul::string trimmed = acul::trim(acul::string("  hello world   "));
    assert(trimmed == "hello world");

    acul::u16string trimmed16 = acul::trim(acul::utf8_to_utf16("  hello world   "));
    assert(acul::utf16_to_utf8(trimmed16) == "hello world");

    // format
    acul::string fmt = acul::format("Value: %d", 123);
    assert(fmt == "Value: 123");

    // to_string
    acul::string num_str = acul::to_string(42);
    assert(num_str == "42");

    // to_u16string
    acul::u16string num_u16 = acul::to_u16string(42);
    assert(acul::utf16_to_utf8(num_u16) == "42");

    // stoi
    const char *str = "1234";
    int value = 0;
    bool ok = acul::stoi(str, value);
    assert(ok && value == 1234);

    // stoull
    const char *str2 = "123456789";
    unsigned long long uvalue = 0;
    ok = acul::stoull(str2, uvalue);
    assert(ok && uvalue == 123456789ull);

    // stoull_hex (ptr)
    const char *strP = "0xDEADBEEF";
    void *addr = nullptr;

    ok = acul::stoull_hex(strP, addr);
    assert(ok);
    assert(reinterpret_cast<u64>(addr) == 0xDEADBEEFull);

    // stof
    const char *str3 = "3.14";
    float fvalue = 0.0f;
    ok = acul::stof(str3, fvalue);
    assert(ok && (fvalue > 3.13f && fvalue < 3.15f));

    // str_range
    const char *str4 = "hello";
    acul::string range = acul::strip_controls(str4);
    assert(range == "hello");

    // trim_end
    acul::string trimEnd = acul::trim_end(acul::string("hello   "));
    assert(trimEnd == "hello");

    acul::string zero = acul::to_string(0);
    assert(zero == "0");

    // Negative
    acul::string negative = acul::to_string(-12345);
    assert(negative == "-12345");
}

void test_string()
{
    test_refstring();
    test_basic_string();
    test_sstream();
    test_string_view_pool();
    test_string_view();
    test_utils();
}
