#include <acul/vector.hpp>
#include <algorithm>
#include <cassert>
#include <numeric>

void test_vector_basic()
{
    acul::vector<int> v;
    assert(v.empty());
    assert(v.size() == 0);

    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    assert(!v.empty());
    assert(v.size() == 3);
    assert(v[0] == 1);
    assert(v[1] == 2);
    assert(v[2] == 3);

    v.clear();
    assert(v.empty());
    assert(v.size() == 0);

    acul::vector<uint32_t> v2(10, 7);
    assert(v2.size() == 10);
    assert(v2[0] == 7);
}

void test_vector_resize_reserve()
{
    acul::vector<int> v;
    v.reserve(10);
    assert(v.capacity() >= 10);
    assert(v.size() == 0);

    v.resize(5);
    assert(v.size() == 5);

    v[2] = 42;
    assert(v[2] == 42);

    v.resize(2);
    assert(v.size() == 2);
}

void test_vector_copy_move_assign()
{
    acul::vector<int> v1;
    v1.push_back(100);
    v1.push_back(200);

    // Copy
    acul::vector<int> v2 = v1;
    assert(v2.size() == 2);
    assert(v2[0] == 100);
    assert(v2[1] == 200);

    // Assign
    acul::vector<int> v3;
    v3 = v2;
    assert(v3.size() == 2);
    assert(v3[0] == 100);
    assert(v3[1] == 200);

    // Move
    acul::vector<int> v4 = std::move(v1);
    assert(v4.size() == 2);
    assert(v4[0] == 100);
    assert(v4[1] == 200);
    assert(v1.empty() || v1.size() == 0);

    // Assign move
    acul::vector<int> v5;
    v5 = std::move(v2);
    assert(v5.size() == 2);
    assert(v5[0] == 100);
    assert(v5[1] == 200);
    assert(v2.empty() || v2.size() == 0);
}

void test_vector_release()
{
    {
        acul::vector<int> v;
        v.push_back(7);
        v.push_back(8);
        v.push_back(9);

        int *raw = v.release();
        assert(raw != nullptr);

        assert(v.empty());
        assert(v.size() == 0);

        assert(raw[0] == 7);
        assert(raw[1] == 8);
        assert(raw[2] == 9);

        acul::release(raw);
    }

    {
        acul::vector<int> v;
        int *raw = v.release();
        assert(raw == nullptr);
        assert(v.empty());
        assert(v.size() == 0);
    }
}

void test_vector_front_back()
{
    acul::vector<int> v;
    v.push_back(5);
    v.push_back(10);
    v.push_back(15);

    assert(v.front() == 5);
    assert(v.back() == 15);

    v.front() = 1;
    v.back() = 99;

    assert(v.front() == 1);
    assert(v.back() == 99);
}

void test_vector_insert_erase()
{
    acul::vector<int> v = {1, 2, 4};

    v.insert(v.begin() + 2, 3);
    assert(v.size() == 4);
    assert(v[2] == 3);
    assert(v[3] == 4);

    v.erase(v.begin() + 1);
    assert(v.size() == 3);
    assert(v[0] == 1);
    assert(v[1] == 3);
    assert(v[2] == 4);
}

void test_vector_assign()
{
    acul::vector<int> v;
    v.assign(5, 42);
    assert(v.size() == 5);
    for (auto &e : v) assert(e == 42);
}

void test_vector_iterators()
{
    acul::vector<int> v(5);
    std::iota(v.begin(), v.end(), 10);

    // Check construct
    acul::vector<int> b{v.begin(), v.end()};

    assert(std::find(v.begin(), v.end(), 12) != v.end());
    assert(std::find(v.begin(), v.end(), 999) == v.end());

    auto it = std::find_if(v.begin(), v.end(), [](int x) { return x > 12; });
    assert(it != v.end());
    assert(*it == 13);
}

void test_vector()
{
    test_vector_basic();
    test_vector_resize_reserve();
    test_vector_copy_move_assign();
    test_vector_release();
    test_vector_front_back();
    test_vector_insert_erase();
    test_vector_assign();
    test_vector_iterators();
}
