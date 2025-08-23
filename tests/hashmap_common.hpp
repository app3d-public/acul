#pragma once

#include <cassert>
#include <cstddef>

template <typename T>
void test_hashmap_basic()
{
    T m(8);
    assert(m.empty());
    assert(m.size() == 0);

    m[10] = 123;
    assert(!m.empty());
    assert(m.size() == 1);
    assert(m[10] == 123);

    m[10] = 456;
    assert(m.size() == 1);
    assert(m[10] == 456);

    m[11] = 789;
    assert(m.size() == 2);
    assert(m[11] == 789);

    auto r1 = m.emplace(12, 900);
    assert(r1.second == true);
    assert(r1.first->first == 12 && r1.first->second == 900);
    assert(m.size() == 3);

    auto r2 = m.emplace(12, 901);
    assert(r2.second == false);
    assert(m.size() == 3);
    assert(m[12] == 900);
}

template <typename T>
void test_hashmap_many_inserts_and_reads()
{
    T m(8);
    const int N = 50000;

    for (int i = 0; i < N; ++i)
    {
        if ((i & 1) == 0)
            m[i] = i + 1;
        else
            m.emplace(i, i + 1);
    }
    assert(m.size() == N);

    for (int i = 0; i < N; ++i) { assert(m[i] == i + 1); }
}

template <typename T>
void test_hashmap_iteration()
{
    T m(8);
    for (int i = 0; i < 1000; ++i) m.emplace(i, i * 2);
    assert(m.size() == 1000);

    long long sum_keys = 0, sum_vals = 0;
    size_t count = 0;
    for (auto &kv : m)
    {
        sum_keys += kv.first;
        sum_vals += kv.second;
        ++count;
    }
    assert(count == 1000);

    long long expect_keys = 999LL * 1000 / 2;
    long long expect_vals = 2 * expect_keys;
    assert(sum_keys == expect_keys);
    assert(sum_vals == expect_vals);
}

template <typename T>
void test_hashmap_update_path()
{
    T m(8);
    m[42] = 1;
    m[42] = 2;
    m[42] = 3;
    assert(m.size() == 1);
    assert(m[42] == 3);

    for (int i = 0; i < 1000; ++i) m[i] = i;
    for (int i = 0; i < 1000; i += 3) m[i] = -i;
    for (int i = 0; i < 1000; ++i)
    {
        int expect = (i % 3 == 0) ? -i : i;
        assert(m[i] == expect);
    }
}

template <typename T>
void test_hashmap_erase()
{
    T m(8);
    for (int i = 0; i < 1000; ++i) m[i] = i;

    for (int i = 0; i < 1000; i += 2) { (void)m.erase(i); }
    for (int i = 1; i < 1000; i += 2) { assert(m[i] == i); }
    for (int i = 0; i < 1000; i += 2)
    {
        m.emplace(i, -i);
        assert(m[i] == -i);
    }
}
