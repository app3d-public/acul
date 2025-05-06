#include <acul/list.hpp>
#include <acul/stream.hpp>
#include <acul/string/string.hpp>
#include <cassert>
#include <cstring> // memcpy

#ifdef ACUL_GLM_ENABLE
    #include <glm/gtc/matrix_transform.hpp>
#endif

void test_bin_stream_basic_types()
{
    acul::bin_stream s;
    assert(!s.data());
    int a = 42;
    float b = 3.14f;
    s.write(a).write(b);

    int a2 = 0;
    float b2 = 0;
    s.read(a2).read(b2);

    assert(a2 == 42);
    assert(fabs(b2 - 3.14f) < 0.0001f);
}

void test_bin_stream_string()
{
    acul::string msg = "hello";
    acul::bin_stream s;
    s.write(msg);

    acul::string out;
    s.read(out);
    assert(out == "hello");
}

void test_bin_stream_list()
{
    acul::list<int> l = {1, 2, 3, 4};
    acul::bin_stream s;
    s.write(l);

    acul::list<int> l2;
    s.read(l2);
    assert(l2.size() == 4);
    int expected = 1;
    for (int v : l2) assert(v == expected++);
}

void test_bin_stream_raw_data()
{
    char raw[] = "bin_test";
    acul::bin_stream s;
    s.write(raw, sizeof(raw));

    char out[sizeof(raw)] = {};
    s.read(out, sizeof(out));
    assert(std::memcmp(raw, out, sizeof(raw)) == 0);
}

void test_bin_stream_positioning()
{
    acul::bin_stream s;
    s.write(1).write(2).write(3);

    int val = 0;
    acul::vector<char> b;
    b.insert(b.end(), s.data(), s.data() + s.size());
    acul::bin_stream r{std::move(b)};
    r.read(val);
    assert(val == 1);

    r.read(val);
    assert(val == 2);
}

void test_bin_stream_exceptions()
{
    acul::bin_stream s;

    bool caught = false;
    try
    {
        s.pos(10);
    }
    catch (const acul::out_of_range &e)
    {
        caught = true;
    }
    assert(caught);

    caught = false;
    try
    {
        char *ptr = nullptr;
        s.read(ptr, 1);
    }
    catch (const acul::runtime_error &e)
    {
        caught = true;
    }
    assert(caught);
}

#ifdef ACUL_GLM_ENABLE
void test_bin_stream_glm()
{
    acul::bin_stream s;

    glm::vec2 v2(1.0f, 2.0f);
    glm::vec3 v3(3.0f, 4.0f, 5.0f);
    glm::mat4 m(1.0f); // identity

    s.write(v2).write(v3).write(m);

    glm::vec2 v2r;
    glm::vec3 v3r;
    glm::mat4 mr;

    s.read(v2r).read(v3r).read(mr);

    assert(v2r == v2);
    assert(v3r == v3);
    assert(mr == m);
}
#endif

void test_stream()
{
    test_bin_stream_basic_types();
    test_bin_stream_string();
    test_bin_stream_list();
    test_bin_stream_raw_data();
    test_bin_stream_positioning();
    test_bin_stream_exceptions();
#ifdef ACUL_GLM_ENABLE
    test_bin_stream_glm();
#endif
}
