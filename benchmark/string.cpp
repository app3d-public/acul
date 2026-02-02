#include <benchmark/benchmark.h>
#include <cfloat>
#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <acul/io/path.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/string.hpp>
#include <acul/string/utils.hpp>
#include <acul/vector.hpp>

#define BENCHMARK_WINDOW_TIME 100

static acul::vector<acul::string> gen_acul_strings(size_t N, size_t len = 16, uint64_t seed = 1234567)
{
    acul::vector<acul::string> out;
    out.reserve(N);
    std::mt19937_64 rng(seed);
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
static std::vector<std::string> gen_std_strings(size_t N, size_t len = 16, uint64_t seed = 1234567)
{
    std::vector<std::string> out;
    out.reserve(N);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dist('a', 'z');
    std::string tmp;
    tmp.resize(len);
    for (size_t i = 0; i < N; ++i)
    {
        for (size_t j = 0; j < len; ++j) tmp[j] = char(dist(rng));
        out.emplace_back(tmp);
    }
    return out;
}
template <class T>
static void shuffle_vec(T &v, uint64_t seed = 1234567)
{
    std::mt19937_64 rng(seed);
    std::shuffle(v.begin(), v.end(), rng);
}

#define RUN_BENCHMARK(state, ops_per_iter, bytes_per_op, SETUP_BLOCK, BODY_BLOCK)                                      \
    do {                                                                                                               \
        const double target_ms = BENCHMARK_WINDOW_TIME;                                                                \
        const double target_s = target_ms / 1000.0;                                                                    \
        double min_cps = DBL_MAX, max_cps = 0.0;                                                                       \
        double total_dt = 0.0;                                                                                         \
        uint64_t total_ops = 0;                                                                                        \
        for (auto _ : state)                                                                                           \
        {                                                                                                              \
            double acc_dt = 0.0;                                                                                       \
            uint64_t acc_ops = 0;                                                                                      \
            do {                                                                                                       \
                state.PauseTiming();                                                                                   \
                {SETUP_BLOCK} state.ResumeTiming();                                                                    \
                auto t0 = std::chrono::high_resolution_clock::now();                                                   \
                {                                                                                                      \
                    BODY_BLOCK                                                                                         \
                }                                                                                                      \
                auto t1 = std::chrono::high_resolution_clock::now();                                                   \
                double dt = std::chrono::duration<double>(t1 - t0).count();                                            \
                acc_dt += dt;                                                                                          \
                acc_ops += (ops_per_iter);                                                                             \
            } while (acc_dt < target_s);                                                                               \
            const double cps = acc_ops / acc_dt;                                                                       \
            if (cps < min_cps) min_cps = cps;                                                                          \
            if (cps > max_cps) max_cps = cps;                                                                          \
            total_dt += acc_dt;                                                                                        \
            total_ops += acc_ops;                                                                                      \
            state.SetIterationTime(acc_dt);                                                                            \
        }                                                                                                              \
        const double cps_avg = (total_dt > 0.0) ? (double(total_ops) / total_dt) : 0.0;                                \
        state.counters["cps_avg"] = cps_avg;                                                                           \
        state.counters["cps_min"] = min_cps;                                                                           \
        state.counters["cps_max"] = max_cps;                                                                           \
        state.counters["bw_mib_s"] = benchmark::Counter(cps_avg * double(bytes_per_op), benchmark::Counter::kDefaults, \
                                                        benchmark::Counter::kIs1024);                                  \
    } while (0)

static void BM_string_construct_small_acul(benchmark::State &state)
{
    const size_t N = state.range(0);
    const char *lit = "hello_world";
    const size_t OPS = N;
    const size_t BYTES = 11;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            acul::string s;
            for (size_t i = 0; i < N; ++i)
            {
                s = acul::string(lit);
                benchmark::DoNotOptimize(s);
            }
        });
}
BENCHMARK(BM_string_construct_small_acul)->Arg(1'000'000)->UseManualTime();

static void BM_string_construct_small_std(benchmark::State &state)
{
    const size_t N = state.range(0);
    const char *lit = "hello_world";
    const size_t OPS = N;
    const size_t BYTES = 11;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            std::string s;
            for (size_t i = 0; i < N; ++i)
            {
                s = std::string(lit);
                benchmark::DoNotOptimize(s);
            }
        });
}
BENCHMARK(BM_string_construct_small_std)->Arg(1'000'000)->UseManualTime();

static void BM_string_append_acul(benchmark::State &state)
{
    const size_t N = state.range(0);
    auto parts = gen_acul_strings(32, 16);
    const size_t OPS = N;
    const size_t BYTES = 16;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sink = 0;
            for (size_t i = 0; i < N; ++i)
            {
                acul::string s;
                for (auto const &p : parts) s += p;
                sink ^= s.size();
            }
            benchmark::DoNotOptimize(sink);
        });
}
BENCHMARK(BM_string_append_acul)->Arg(2000)->UseManualTime();

static void BM_string_append_std(benchmark::State &state)
{
    const size_t N = state.range(0);
    auto parts = gen_std_strings(32, 16);
    const size_t OPS = N;
    const size_t BYTES = 16;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sink = 0;
            for (size_t i = 0; i < N; ++i)
            {
                std::string s;
                for (auto const &p : parts) s += p;
                sink ^= s.size();
            }
            benchmark::DoNotOptimize(sink);
        });
}
BENCHMARK(BM_string_append_std)->Arg(2000)->UseManualTime();

static void BM_string_find_acul(benchmark::State &state)
{
    const size_t N = state.range(0);
    auto base = gen_acul_strings(1, 1024, 777)[0];
    acul::string needle("abc");
    const size_t OPS = N;
    const size_t BYTES = 1024;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sum = 0;
            for (size_t i = 0; i < N; ++i)
            {
                auto pos = base.find(needle);
                sum += (pos != acul::string::npos);
            }
            benchmark::DoNotOptimize(sum);
        });
}
BENCHMARK(BM_string_find_acul)->Arg(250'000)->UseManualTime();

static void BM_string_find_std(benchmark::State &state)
{
    const size_t N = state.range(0);
    auto base = gen_std_strings(1, 1024, 777)[0];
    std::string needle("abc");
    const size_t OPS = N;
    const size_t BYTES = 1024;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sum = 0;
            for (size_t i = 0; i < N; ++i)
            {
                auto pos = base.find(needle);
                sum += (pos != std::string::npos);
            }
            benchmark::DoNotOptimize(sum);
        });
}
BENCHMARK(BM_string_find_std)->Arg(250'000)->UseManualTime();

static void BM_to_string_u64_acul(benchmark::State &state)
{
    const size_t N = state.range(0);
    const size_t OPS = N;
    const size_t BYTES = sizeof(uint64_t);
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            acul::string s;
            for (size_t i = 0; i < N; ++i)
            {
                s = acul::to_string((uint64_t)i);
                benchmark::DoNotOptimize(s);
            }
        });
}
BENCHMARK(BM_to_string_u64_acul)->Arg(5'000'000)->UseManualTime();

static void BM_to_string_u64_std(benchmark::State &state)
{
    const size_t N = state.range(0);
    const size_t OPS = N;
    const size_t BYTES = sizeof(uint64_t);
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            std::string s;
            for (size_t i = 0; i < N; ++i)
            {
                s = std::to_string((uint64_t)i);
                benchmark::DoNotOptimize(s);
            }
        });
}
BENCHMARK(BM_to_string_u64_std)->Arg(5'000'000)->UseManualTime();

static void BM_stoull_acul(benchmark::State &state)
{
    const size_t N = state.range(0);
    auto data = gen_std_strings(N, 12, 424242);
    const size_t OPS = N;
    const size_t BYTES = 12;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            uint64_t sum = 0;
            for (size_t i = 0; i < N; ++i)
            {
                const char *p = data[i].c_str();
                unsigned long long v = 0;
                bool ok = acul::stoull(p, v);
                sum += v + (ok ? 1ull : 0ull);
            }
            benchmark::DoNotOptimize(sum);
        });
}
BENCHMARK(BM_stoull_acul)->Arg(1'000'000)->UseManualTime();

static void BM_stoull_std(benchmark::State &state)
{
    const size_t N = state.range(0);
    auto data = gen_std_strings(N, 12, 424242);
    for (auto &s : data)
        for (auto &ch : s) ch = char('0' + (ch % 10));
    const size_t OPS = N;
    const size_t BYTES = 12;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            uint64_t sum = 0;
            for (size_t i = 0; i < N; ++i) { sum += std::stoull(data[i]); }
            benchmark::DoNotOptimize(sum);
        });
}
BENCHMARK(BM_stoull_std)->Arg(1'000'000)->UseManualTime();

static void BM_sstream_write_acul(benchmark::State &state)
{
    const size_t N = state.range(0);
    const size_t OPS = N;
    const size_t BYTES = 64;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sink = 0;
            for (size_t i = 0; i < N; ++i)
            {
                acul::stringstream ss;
                ss << "idx=" << i << ' ' << 3.1415926 << ' ' << -42;
                auto s = ss.str();
                sink ^= s.size();
            }
            benchmark::DoNotOptimize(sink);
        });
}
BENCHMARK(BM_sstream_write_acul)->Arg(250'000)->UseManualTime();

static void BM_sstream_write_std(benchmark::State &state)
{
    const size_t N = state.range(0);
    const size_t OPS = N;
    const size_t BYTES = 64;
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sink = 0;
            for (size_t i = 0; i < N; ++i)
            {
                std::stringstream ss;
                ss << "idx=" << i << ' ' << 3.1415926 << ' ' << -42;
                auto s = ss.str();
                sink ^= s.size();
            }
            benchmark::DoNotOptimize(sink);
        });
}
BENCHMARK(BM_sstream_write_std)->Arg(250'000)->UseManualTime();

template <class PathT>
static void BM_path_join_impl(benchmark::State &state, const char *label)
{
    const size_t N = state.range(0);
    const size_t OPS = N;
    const size_t BYTES = 64;
    state.SetLabel(label);
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sink = 0;
            for (size_t i = 0; i < N; ++i)
            {
                PathT p("root");
                p = p / "users" / "wusiki" / "project" / "assets" / "images" / "2025" / "08" / "25" / "test" /
                    "file.png";

                if constexpr (std::is_same_v<PathT, acul::path>)
                {
                    auto s = p.str();
                    sink ^= s.size();
                }
                else
                {
                    auto s = p.string();
                    sink ^= s.size();
                }
            }
            benchmark::DoNotOptimize(sink);
        });
}

static void BM_path_join_acul(benchmark::State &state)
{
    BM_path_join_impl<acul::path>(state, "path_join acul::path");
}
BENCHMARK(BM_path_join_acul)->Arg(250'000)->UseManualTime();

static void BM_path_join_std(benchmark::State &state)
{
    BM_path_join_impl<std::filesystem::path>(state, "path_join std::filesystem::path");
}
BENCHMARK(BM_path_join_std)->Arg(250'000)->UseManualTime();

template <class PathT>
static void BM_path_decompose_impl(benchmark::State &state, const char *label)
{
    const size_t N = state.range(0);
    const size_t OPS = N;
    const size_t BYTES = 64;
    state.SetLabel(label);
    RUN_BENCHMARK(
        state, OPS, BYTES, { (void)0; },
        {
            size_t sink = 0;
            for (size_t i = 0; i < N; ++i)
            {
                PathT p("root/users/wusiki/project/assets/images/2025/08/25/test/file.tar.gz");
                auto name = p.filename();
                auto par = p.parent_path();
                if constexpr (std::is_same_v<PathT, acul::path>)
                    sink ^= name.size() ^ par.str().size();
                else
                    sink ^= name.string().size() ^ par.string().size();
            }
            benchmark::DoNotOptimize(sink);
        });
}

static void BM_path_decompose_acul(benchmark::State &state)
{
    BM_path_decompose_impl<acul::path>(state, "path_decompose acul::path");
}
BENCHMARK(BM_path_decompose_acul)->Arg(250'000)->UseManualTime();

static void BM_path_decompose_std(benchmark::State &state)
{
    BM_path_decompose_impl<std::filesystem::path>(state, "path_decompose std::filesystem::path");
}
BENCHMARK(BM_path_decompose_std)->Arg(250'000)->UseManualTime();

BENCHMARK_MAIN();
