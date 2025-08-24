#pragma once

#include <cassert>
#include <cstddef>

template <typename T>
void test_hashset_basic()
{
    T s(8);
    assert(s.empty());
    assert(s.size() == 0);

    auto r1 = s.emplace(10);
    assert(r1.second == true);
    assert(*r1.first == 10);
    assert(!s.empty());
    assert(s.size() == 1);
    assert(s.contains(10));

    auto r2 = s.emplace(10);
    assert(r2.second == false);
    assert(s.size() == 1);
    assert(s.contains(10));

    assert(s.erase(10) == true);
    assert(!s.contains(10));
    assert(s.size() == 0);
    assert(s.empty());

    assert(s.erase(10) == false);
}

template <typename T>
void test_hashset_many_inserts_and_reads()
{
    T s(8);
    const int N = 50000;

    for (int i = 0; i < N; ++i)
    {
        if ((i & 1) == 0)
        {
            auto r = s.emplace(i);
            assert(r.second == true);
        }
        else
            (void)s.emplace(i);
    }
    assert(s.size() == N);

    for (int i = 0; i < N; ++i) { assert(s.contains(i)); }

    for (int i = 0; i < N; ++i)
    {
        auto r = s.emplace(i);
        assert(r.second == false);
    }
    assert(s.size() == N);
}

template <typename T>
void test_hashset_iteration()
{
    T s(8);
    for (int i = 0; i < 1000; ++i) (void)s.emplace(i);
    assert(s.size() == 1000);

    long long sum_keys = 0;
    size_t count = 0;
    for (auto &k : s)
    {
        sum_keys += k;
        ++count;
    }
    assert(count == 1000);

    long long expect_keys = 999LL * 1000 / 2;
    assert(sum_keys == expect_keys);
}

template <typename T>
void test_hashset_idempotent_emplace()
{
    T s(8);

    for (int i = 0; i < 1000; ++i)
    {
        auto r = s.emplace(42);
        if (i == 0)
            assert(r.second == true);
        else
            assert(r.second == false);
        assert(s.contains(42));
        assert(s.size() == 1);
    }

    for (int i = 0; i < 1000; ++i) (void)s.emplace(i);
    assert(s.size() == 1000);
    for (int i = 0; i < 1000; ++i) { assert(s.contains(i)); }
}

template <typename T>
void test_hashset_erase()
{
    T s(8);
    for (int i = 0; i < 1000; ++i) (void)s.emplace(i);
    assert(s.size() == 1000);

    for (int i = 0; i < 1000; i += 2) (void)s.erase(i);

    for (int i = 1; i < 1000; i += 2) assert(s.contains(i));
    for (int i = 0; i < 1000; i += 2) assert(!s.contains(i));

    for (int i = 0; i < 1000; i += 2)
    {
        auto r = s.emplace(i);
        assert(r.second == true);
        assert(s.contains(i));
    }
    assert(s.size() == 1000);
}