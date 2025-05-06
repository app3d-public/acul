#include <acul/string/refstring.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/string.hpp>
#include <acul/string/string_pool.hpp>
#include <acul/string/string_view.hpp>
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

void test_string_pool()
{
    {
        acul::string_pool<char> pool(32);
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

        acul::string_pool<char> pool(5);
        pool.push("abc", 3);
        assert(pool.size() == 1);
        assert(strcmp(pool[0], "abc") == 0);

        pool.push("defg", 4);
        assert(pool.size() == 2);
        assert(std::strcmp(pool[1], "defg") == 0);
        assert(std::strcmp(pool[0], "abc") == 0);
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
#ifdef ACUL_GLM_ENABLE
void test_utils_glm()
{
    using namespace acul;

    {
        glm::vec2 v(1.0f, 2.0f);
        char buffer[64]{};
        int written = to_string(v, buffer, sizeof(buffer), 0);
        assert(written > 0);

        string result(buffer);
        assert(result.find('1') != string::npos);
        assert(result.find('2') != string::npos);
    }

    {
        glm::vec3 v(3.0f, 4.0f, 5.0f);
        char buffer[64]{};
        int written = to_string(v, buffer, sizeof(buffer), 0);
        assert(written > 0);

        string result(buffer);
        assert(result.find('3') != string::npos);
        assert(result.find('4') != string::npos);
        assert(result.find('5') != string::npos);
    }

    {
        const char *text = "10.5 20.25";
        glm::vec2 v(0.0f);
        bool ok = stov2(text, v);
        assert(ok);
        assert(v.x > 10.4f && v.x < 10.6f);
        assert(v.y > 20.2f && v.y < 20.3f);
    }

    {
        const char *text = "30.75";
        glm::vec2 v(0.0f);
        stov2_opt(text, v);
        assert(v.x > 30.7f && v.x < 30.8f);
        assert(v.y == 0.0f);
    }

    {
        const char *text = "1.1 2.2 3.3";
        glm::vec3 v(0.0f);
        bool ok = stov3(text, v);
        assert(ok);
        assert(v.x > 1.0f && v.x < 1.2f);
        assert(v.y > 2.1f && v.y < 2.3f);
        assert(v.z > 3.2f && v.z < 3.4f);
    }

    {
        const char *text = "7.7 8.8";
        glm::vec3 v(0.0f);
        stov3_opt(text, v);
        assert(v.x > 7.6f && v.x < 7.8f);
        assert(v.y > 8.7f && v.y < 8.9f);
        assert(v.z == 0.0f);
    }
}
#endif

void test_utils()
{
    // utf8_to_utf16 / utf16_to_utf8
    acul::string utf8 = "Hello";
    acul::u16string utf16 = acul::utf8_to_utf16(utf8);
    acul::string back_to_utf8 = acul::utf16_to_utf8(utf16);
    assert(utf8 == back_to_utf8);

    // trim
    acul::string trimmed = acul::trim("  hello world   ");
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

    // stoull (ptr)
    const char *strP = "0xDEADBEEF";
    void *addr = nullptr;

    ok = acul::stoull(strP, addr);
    assert(ok);
    assert(reinterpret_cast<u64>(addr) == 0xDEADBEEFull);

    // stof
    const char *str3 = "3.14";
    float fvalue = 0.0f;
    ok = acul::stof(str3, fvalue);
    assert(ok && (fvalue > 3.13f && fvalue < 3.15f));

    // str_range
    const char *str4 = "hello";
    acul::string range = acul::str_range(str4);
    assert(range == "hello");

    // trim_end
    acul::string trimEnd = acul::trim_end("hello   ");
    assert(trimEnd == "hello");

    acul::string zero = acul::to_string(0);
    assert(zero == "0");

    // Negative
    acul::string negative = acul::to_string(-12345);
    assert(negative == "-12345");

#ifdef ACUL_GLM_ENABLE
    test_utils_glm();
#endif
}

void test_string()
{
    test_refstring();
    test_basic_string();
    test_sstream();
    test_string_pool();
    test_string_view();
    test_utils();
}