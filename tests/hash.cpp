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

void test_cityhash64()
{
    const char *data = "The quick brown fox jumps over the lazy dog";
    u64 hash1 = acul::cityhash64(data, std::strlen(data));
    u64 hash2 = acul::cityhash64(data, std::strlen(data));

    assert(hash1 == hash2);

    const char *data2 = "The quick brown fox jumps over the lazy cog";
    u64 hash3 = acul::cityhash64(data2, std::strlen(data2));
    assert(hash1 != hash3);

    // Check 64+
    const char *long_data = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                            "Integer nec odio. Praesent libero. Sed cursus ante dapibus.";
    size_t long_len = std::strlen(long_data);

    u64 hash_long1 = acul::cityhash64(long_data, long_len);
    u64 hash_long2 = acul::cityhash64(long_data, long_len);
    assert(hash_long1 == hash_long2);

    // Just in case, hash should differ from short strings
    assert(hash_long1 != hash1);
}

void test_hash_combine()
{
    std::size_t seed = 0;
    acul::string s = "example";
    std::hash_combine(seed, s);
    assert(seed != 0);

    std::size_t seed2 = seed;
    std::hash_combine(seed2, 42);
    assert(seed != seed2);
}

void test_hash()
{
    test_id_gen();
    test_crc32();
    test_cityhash64();
    test_hash_combine();
}
