#include <acul/vector.hpp>
#include <benchmark/benchmark.h>
#include <cfloat>
#include <chrono>
#include <random>
#include <type_traits>

#include <absl/container/flat_hash_map.h>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/hl_hashmap.hpp>
#include <acul/string/string.hpp>
#include <llvm/ADT/DenseMap.h>
#include <unordered_map>
#include "hash_table5.hpp"
#include "hash_table7.hpp"

#define BENCHMARK_WINDOW_TIME 100

struct Vec3f
{
    float x, y, z;
    bool operator==(const Vec3f &o) const noexcept { return x == o.x && y == o.y && z == o.z; }
};
namespace std
{
    template <>
    struct hash<Vec3f>
    {
        size_t operator()(const Vec3f &v) const noexcept
        {
            size_t h1 = std::hash<float>{}(v.x);
            size_t h2 = std::hash<float>{}(v.y);
            size_t h3 = std::hash<float>{}(v.z);
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
} // namespace std

namespace llvm
{

    template <>
    struct DenseMapInfo<acul::string>
    {
        static inline acul::string getEmptyKey()
        {
            static const char k = '\x01';
            return acul::string(&k, 1);
        }

        static inline acul::string getTombstoneKey()
        {
            static const char k = '\x02';
            return acul::string(&k, 1);
        }

        static inline unsigned getHashValue(const acul::string &s) { return std::hash<acul::string>{}(s); }

        static inline bool isEqual(const acul::string &a, const acul::string &b) { return a == b; }
    };

    template <>
    struct DenseMapInfo<Vec3f>
    {
        static inline Vec3f getEmptyKey()
        {
            float q = std::numeric_limits<float>::quiet_NaN();
            return Vec3f{q, q, q};
        }
        static inline Vec3f getTombstoneKey()
        {
            float ninf = -std::numeric_limits<float>::infinity();
            return Vec3f{ninf, ninf, ninf};
        }
        static inline unsigned getHashValue(const Vec3f &v) { return static_cast<unsigned>(std::hash<Vec3f>{}(v)); }
        static inline bool isEqual(const Vec3f &a, const Vec3f &b)
        {
            auto isEmpty = [](const Vec3f &v) { return std::isnan(v.x) && std::isnan(v.y) && std::isnan(v.z); };
            auto isTomb = [](const Vec3f &v) {
                return std::isinf(v.x) && std::signbit(v.x) && std::isinf(v.y) && std::signbit(v.y) &&
                       std::isinf(v.z) && std::signbit(v.z);
            };
            if (isEmpty(a) && isEmpty(b)) return true;
            if (isTomb(a) && isTomb(b)) return true;
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
    };

} // namespace llvm

template <class Map, class K, class V>
auto insert_kv_impl(Map &m, const K &k, const V &v, int) -> decltype(m.try_emplace(k, v), void())
{
    m.try_emplace(k, v);
}

template <class Map, class K, class V>
auto insert_kv_impl(Map &m, const K &k, const V &v, long) -> decltype(m.emplace(k, v), void())
{
    m.emplace(k, v);
}

template <class Map, class K, class V>
void insert_kv_impl(Map &m, const K &k, const V &v, ...)
{
    m.insert(std::make_pair(k, v));
}

template <class Map, class K, class V>
inline void insert_kv(Map &m, const K &k, const V &v)
{
    insert_kv_impl(m, k, v, 0);
}

static acul::vector<uint64_t> gen_u64(size_t N)
{
    acul::vector<uint64_t> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) v.push_back(static_cast<uint64_t>(i));
    return v;
}
static acul::vector<Vec3f> gen_vec3(size_t N)
{
    acul::vector<Vec3f> out;
    out.reserve(N);
    for (size_t i = 0; i < N; ++i) out.push_back(Vec3f{float(i * 0.1f), float(i * 0.2f), float(i * 0.3f)});
    return out;
}
static acul::vector<acul::string> gen_strings(size_t N, size_t len = 16)
{
    acul::vector<acul::string> out;
    out.reserve(N);
    std::mt19937_64 rng(1234567);
    std::uniform_int_distribution<int> dist('a', 'z');
    acul::string tmp;
    tmp.resize(len);
    for (size_t i = 0; i < N; ++i)
    {
        for (size_t j = 0; j < len; ++j) tmp[j] = char(dist(rng));
        out.emplace_back(tmp.c_str(), (acul::string::size_type)len);
    }
    return out;
}

static acul::vector<acul::string> gen_strings_seed(size_t N, size_t len = 16, uint64_t seed = 1234567)
{
    acul::vector<acul::string> out;
    out.reserve(N);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dist('a', 'z');
    acul::string tmp(len, 'a');
    for (size_t i = 0; i < N; ++i)
    {
        for (size_t j = 0; j < len; ++j) tmp[j] = char(dist(rng));
        out.emplace_back(tmp.c_str(), (acul::string::size_type)len);
    }
    return out;
}

template <class T>
static void shuffle(acul::vector<T> &v, uint64_t seed = 1234567)
{
    std::mt19937_64 rng(seed);
    std::shuffle(v.begin(), v.end(), rng);
}

template <class K>
static acul::vector<K> make_keys(size_t N)
{
    if constexpr (std::is_same_v<K, uint64_t>)
        return gen_u64(N);
    else if constexpr (std::is_same_v<K, Vec3f>)
        return gen_vec3(N);
    else
        return gen_strings(N);
}

#define REPORT_STATS(bytes_per_iter)                                                                         \
    do {                                                                                                     \
        double avg_cps = sum_cps / state.iterations();                                                       \
        state.counters["cps_min (1/s)"] = min_cps;                                                           \
        state.counters["cps_avg (1/s)"] = avg_cps;                                                           \
        state.counters["cps_max (1/s)"] = max_cps;                                                           \
        state.counters["Bandwidth (MiB/s)"] =                                                                \
            benchmark::Counter(static_cast<double>(bytes_per_iter) * avg_cps, benchmark::Counter::kDefaults, \
                               benchmark::Counter::kIs1024);                                                 \
    } while (0)

#define RUN_BENCHMARK(state, ops_per_iter, bytes_per_op, SETUP_BLOCK, BODY_BLOCK)          \
    do {                                                                                   \
        const double target_ms = BENCHMARK_WINDOW_TIME;                                    \
        const double target_s = target_ms / 1000.0;                                        \
        double min_cps = DBL_MAX, max_cps = 0.0;                                           \
        double total_dt = 0.0;                                                             \
        uint64_t total_ops = 0;                                                            \
        for (auto _ : state)                                                               \
        {                                                                                  \
            double acc_dt = 0.0;                                                           \
            uint64_t acc_ops = 0;                                                          \
            do {                                                                           \
                state.PauseTiming();                                                       \
                {SETUP_BLOCK} state.ResumeTiming();                                        \
                auto t0 = std::chrono::high_resolution_clock::now();                       \
                {                                                                          \
                    BODY_BLOCK                                                             \
                }                                                                          \
                auto t1 = std::chrono::high_resolution_clock::now();                       \
                double dt = std::chrono::duration<double>(t1 - t0).count();                \
                acc_dt += dt;                                                              \
                acc_ops += (ops_per_iter);                                                 \
            } while (acc_dt < target_s);                                                   \
            const double cps = acc_ops / acc_dt;                                           \
            if (cps < min_cps) min_cps = cps;                                              \
            if (cps > max_cps) max_cps = cps;                                              \
            total_dt += acc_dt;                                                            \
            total_ops += acc_ops;                                                          \
            state.SetIterationTime(acc_dt);                                                \
        }                                                                                  \
        const double cps_avg = (total_dt > 0.0) ? (double(total_ops) / total_dt) : 0.0;    \
        const double bw_gib = cps_avg * double(bytes_per_op) / (1024.0 * 1024.0 * 1024.0); \
        state.counters["bw_gib_s"] = bw_gib;                                               \
        state.counters["cps_avg"] = cps_avg;                                               \
        state.counters["cps_min"] = min_cps;                                               \
        state.counters["cps_max"] = max_cps;                                               \
    } while (0)

template <class K>
constexpr size_t approx_key_bytes()
{
    return sizeof(K);
}

template <>
constexpr size_t approx_key_bytes<acul::string>()
{
    return 16;
}

template <class K, class V>
constexpr size_t approx_pair_bytes()
{
    return approx_key_bytes<K>() + sizeof(V);
}

template <class T>
struct MapName;
template <class K, class V>
struct MapName<acul::hl_hashmap<K, V>>
{
    static const char *value() { return "acul::hl_hashmap"; }
};
template <class K, class V>
struct MapName<acul::hashmap<K, V>>
{
    static const char *value() { return "acul::hashmap"; }
};
template <class K, class V>
struct MapName<emhash5::HashMap<K, V>>
{
    static const char *value() { return "emhash5::HashMap"; }
};

template <class K, class V>
struct MapName<emhash7::HashMap<K, V>>
{
    static const char *value() { return "emhash7::HashMap"; }
};

template <class K, class V>
struct MapName<absl::flat_hash_map<K, V>>
{
    static const char *value() { return "absl::flat_hash_map"; }
};
template <class K, class V>
struct MapName<llvm::DenseMap<K, V>>
{
    static const char *value() { return "llvm::DenseMap"; }
};

template <class K, class V>
struct MapName<std::unordered_map<K, V>>
{
    static const char *value() { return "std::unordered_map"; }
};

template <class MapT, class K, class V = int>
static void BM_erase_half(benchmark::State &state)
{
    const size_t N = size_t(state.range(0));

    acul::vector<K> keys;
    if constexpr (std::is_same_v<K, uint64_t>)
        keys = gen_u64(N);
    else if constexpr (std::is_same_v<K, Vec3f>)
        keys = gen_vec3(N);
    else
        keys = gen_strings(N);

    acul::vector<size_t> idx;
    idx.reserve(N / 2);
    for (size_t i = 1; i < N; i += 2) idx.push_back(i);

    state.SetLabel(std::string("erase_half<") + typeid(K).name() + "> " + MapName<MapT>::value());

    const size_t OPS = idx.size();
    const size_t BYTES_PER_OP = approx_key_bytes<K>();

    alignas(MapT) unsigned char buf[sizeof(MapT)];
    MapT &map = *::new (buf) MapT();

    RUN_BENCHMARK(
        state, OPS, BYTES_PER_OP,
        {
            map.~MapT();
            ::new (&map) MapT();
            for (size_t i = 0; i < N; ++i) insert_kv(map, keys[i], i);
        },
        {
            int64_t sum = 0;
            for (size_t j = 0; j < OPS; ++j) sum += map.erase(keys[idx[j]]);
            benchmark::DoNotOptimize(sum);
        });

    map.~MapT();
}

template <class MapT, class K, class V = int>
static void BM_insert_cold(benchmark::State &state)
{
    const size_t N = size_t(state.range(0));
    auto keys = make_keys<K>(N);
    shuffle(keys);

    state.SetLabel(std::string("insert_cold<") + typeid(K).name() + "> " + MapName<MapT>::value());
    const size_t OPS = N;
    const size_t BYTES_PER_OP = approx_pair_bytes<K, V>();

    alignas(MapT) unsigned char buf[sizeof(MapT)];
    MapT &map = *::new (buf) MapT();

    RUN_BENCHMARK(
        state, OPS, BYTES_PER_OP,
        {
            map.~MapT();
            ::new (&map) MapT();
        },
        {
            for (size_t i = 0; i < N; ++i) insert_kv(map, keys[i], i);
            benchmark::DoNotOptimize(map);
            benchmark::ClobberMemory();
        });

    map.~MapT();
}

template <class MapT, class K, class V = int>
static void BM_find_hit(benchmark::State &state)
{
    const size_t N = static_cast<size_t>(state.range(0));
    auto keys = make_keys<K>(N);
    shuffle(keys);

    state.SetLabel(std::string("find_hit<") + typeid(K).name() + "> " + MapName<MapT>::value());
    const size_t OPS = N;
    const size_t BYTES_PER_OP = approx_key_bytes<K>();

    alignas(MapT) unsigned char buf[sizeof(MapT)];
    MapT &map = *::new (buf) MapT();

    RUN_BENCHMARK(
        state, OPS, BYTES_PER_OP,
        {
            map.~MapT();
            ::new (&map) MapT();
            for (size_t i = 0; i < N; ++i) insert_kv(map, keys[i], V{});
        },
        {
            int64_t sum = 0;
            for (size_t i = 0; i < N; ++i) sum += map.find(keys[i]) != map.end();
            benchmark::DoNotOptimize(sum);
        });

    map.~MapT();
}

template <class MapT, class K, class V = int>
static void BM_find_miss(benchmark::State &state)
{
    const size_t N = size_t(state.range(0));

    acul::vector<K> keys_hit, keys_miss;
    if constexpr (std::is_same_v<K, uint64_t>)
    {
        keys_hit = gen_u64(N);
        keys_miss = gen_u64(N);
        for (auto &x : keys_miss) x += 0x9'0000'0000ULL;
    }
    else if constexpr (std::is_same_v<K, Vec3f>)
    {
        keys_hit = gen_vec3(N);
        keys_miss = gen_vec3(N);
        for (size_t i = 0; i < N; ++i) keys_miss[i].x += 1.0f;
    }
    else
    {
        keys_hit = gen_strings_seed(N, 16, 1234567);
        keys_miss = gen_strings_seed(N, 16, 987654321ULL);
    }
    shuffle(keys_hit);
    shuffle(keys_miss);

    state.SetLabel(std::string("find_miss<") + typeid(K).name() + "> " + MapName<MapT>::value());
    const size_t OPS = N;
    const size_t BYTES_PER_OP = approx_key_bytes<K>();

    alignas(MapT) unsigned char buf[sizeof(MapT)];
    MapT &map = *::new (buf) MapT();

    RUN_BENCHMARK(
        state, OPS, BYTES_PER_OP,
        {
            map.~MapT();
            ::new (&map) MapT();
            for (size_t i = 0; i < N; ++i) insert_kv(map, keys_hit[i], i);
        },
        {
            int64_t sum = 0;
            for (size_t i = 0; i < N; ++i) sum += (map.find(keys_miss[i]) == map.end());
            benchmark::DoNotOptimize(sum);
        });

    map.~MapT();
}

template <class MapT, class K, class V = int>
static void BM_insert_after_erase(benchmark::State &state)
{
    const size_t N = size_t(state.range(0));
    acul::vector<K> keysA, keysB;
    if constexpr (std::is_same_v<K, uint64_t>)
    {
        keysA = gen_u64(N);
        keysB = gen_u64(N);
        for (auto &x : keysB) x += 0x7fff'ffffULL;
    }
    else if constexpr (std::is_same_v<K, Vec3f>)
    {
        keysA = gen_vec3(N);
        keysB = gen_vec3(N);
        for (size_t i = 0; i < N; ++i) keysB[i].y += 2.0f;
    }
    else
    {
        keysA = gen_strings_seed(N, 16, 1234567);
        keysB = gen_strings_seed(N, 16, 424242ULL);
    }
    shuffle(keysA);
    shuffle(keysB);

    const size_t M = N / 2;
    state.SetLabel(std::string("insert_after_erase<") + typeid(K).name() + "> " + MapName<MapT>::value());
    const size_t OPS = M;
    const size_t BYTES_PER_OP = approx_pair_bytes<K, V>();

    alignas(MapT) unsigned char buf[sizeof(MapT)];
    MapT &map = *::new (buf) MapT();

    RUN_BENCHMARK(
        state, OPS, BYTES_PER_OP,
        {
            map.~MapT();
            ::new (&map) MapT();
            for (size_t i = 0; i < N; ++i) insert_kv(map, keysA[i], i);
            for (size_t i = 1; i < N; i += 2) map.erase(keysA[i]);
        },
        {
            for (size_t i = 0; i < M; ++i) insert_kv(map, keysB[i], i);
            benchmark::DoNotOptimize(map);
            benchmark::ClobberMemory();
        });

    map.~MapT();
}

template <class MapT, class K, class V = int>
static void BM_iterate(benchmark::State &state)
{
    const size_t N = size_t(state.range(0));
    auto keys = make_keys<K>(N);
    shuffle(keys);

    state.SetLabel(std::string("iterate<") + typeid(K).name() + "> " + MapName<MapT>::value());
    const size_t OPS = N;
    const size_t BYTES_PER_OP = approx_pair_bytes<K, V>();

    alignas(MapT) unsigned char buf[sizeof(MapT)];
    MapT &map = *::new (buf) MapT();

    RUN_BENCHMARK(
        state, OPS, BYTES_PER_OP,
        {
            map.~MapT();
            ::new (&map) MapT();
            for (size_t i = 0; i < N; ++i) insert_kv(map, keys[i], i);
        },
        {
            size_t count = 0;
            size_t sink = 0;
            for (auto const &kv : map)
            {
                sink ^= std::hash<K>{}(kv.first);
                sink += size_t(kv.second);
                ++count;
            }
            benchmark::DoNotOptimize(sink);
            benchmark::DoNotOptimize(count);
            benchmark::ClobberMemory();
        });

    map.~MapT();
}

#define REG_ALL_FOR(MapT, KeyT) \
    BENCHMARK_TEMPLATE(BM_iterate, MapT<KeyT, int>, KeyT)->Arg(1000)->Arg(1'000'000)->UseManualTime();

// acul::hashmap
template <class K, class V>
using AculHL = acul::hl_hashmap<K, V>;
template <class K, class V>
using Acul = acul::hashmap<K, V>;
template <class K, class V>
using EmhashMap5 = emhash5::HashMap<K, V>;
template <class K, class V>
using EmhashMap7 = emhash7::HashMap<K, V>;
template <class K, class V>
using Absl = absl::flat_hash_map<K, V>;
template <class K, class V>
using LLVMDenseMap = llvm::DenseMap<K, V>;
template <class K, class V>
using StdMap = std::unordered_map<K, V>;

// uint64_t
REG_ALL_FOR(Acul, uint64_t)
REG_ALL_FOR(AculHL, uint64_t)
REG_ALL_FOR(EmhashMap5, uint64_t)
REG_ALL_FOR(EmhashMap7, uint64_t)
REG_ALL_FOR(Absl, uint64_t)
REG_ALL_FOR(LLVMDenseMap, uint64_t)
REG_ALL_FOR(StdMap, uint64_t)

// Vec3f
REG_ALL_FOR(Acul, Vec3f)
REG_ALL_FOR(AculHL, Vec3f)
REG_ALL_FOR(EmhashMap5, Vec3f)
REG_ALL_FOR(EmhashMap7, Vec3f)
REG_ALL_FOR(Absl, Vec3f)
REG_ALL_FOR(LLVMDenseMap, Vec3f)
REG_ALL_FOR(StdMap, Vec3f)

// acul::string
REG_ALL_FOR(Acul, acul::string)
REG_ALL_FOR(AculHL, acul::string)
REG_ALL_FOR(EmhashMap5, acul::string)
REG_ALL_FOR(EmhashMap7, acul::string)
REG_ALL_FOR(Absl, acul::string)
REG_ALL_FOR(LLVMDenseMap, acul::string)
REG_ALL_FOR(StdMap, acul::string)

BENCHMARK_MAIN();