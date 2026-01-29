#include <acul/hash/utils.hpp>
#include <acul/string/string.hpp>
#include <cassert>
#include <cstring>

void test_id_gen()
{
    acul::id_gen gen;

    u64 id1 = gen();
    u64 id2 = gen();
    u64 id3 = gen();

    assert(id1 != id2 || id2 != id3 || id1 != id3);
}

void test_crc32()
{
    const char *data = "The quick brown fox jumps over the lazy dog";
    u32 crc = acul::crc32(0, data, strlen(data));
    assert(crc != 0);
}

void test_hash_combine()
{
    std::size_t seed = 0;
    acul::string s = "example";
    acul::hash_combine(seed, s);
    assert(seed != 0);

    std::size_t seed2 = seed;
    acul::hash_combine(seed2, 42);
    assert(seed != seed2);
}

void test_hash_utils()
{
    test_id_gen();
    test_crc32();
    test_hash_combine();
}
