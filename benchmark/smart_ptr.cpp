#include <benchmark/benchmark.h>

#include <cfloat>
#include <chrono>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <acul/memory/smart_ptr.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>


struct NonTriv
{
    acul::string s;
    int v;
    NonTriv() noexcept : s(), v(0) {}
    NonTriv(const char *cs, int vv) : s(cs), v(vv) {}
    NonTriv(const acul::string &ss, int vv) : s(ss), v(vv) {}
    NonTriv(const NonTriv &) = default;
    NonTriv(NonTriv &&) noexcept = default;
    ~NonTriv() noexcept {}
};

#ifndef BENCHMARK_WINDOW_TIME
    #define BENCHMARK_WINDOW_TIME 100
#endif

#ifndef RUN_BENCHMARK
    #define RUN_BENCHMARK(state, ops_per_iter, bytes_per_op, SETUP_BLOCK, BODY_BLOCK)                        \
        do {                                                                                                 \
            const double target_ms = BENCHMARK_WINDOW_TIME;                                                  \
            const double target_s = target_ms / 1000.0;                                                      \
            double min_cps = DBL_MAX, max_cps = 0.0;                                                         \
            double total_dt = 0.0;                                                                           \
            uint64_t total_ops = 0;                                                                          \
            for (auto _ : state)                                                                             \
            {                                                                                                \
                double acc_dt = 0.0;                                                                         \
                uint64_t acc_ops = 0;                                                                        \
                do {                                                                                         \
                    state.PauseTiming();                                                                     \
                    {SETUP_BLOCK} state.ResumeTiming();                                                      \
                    auto t0 = std::chrono::high_resolution_clock::now();                                     \
                    {                                                                                        \
                        BODY_BLOCK                                                                           \
                    }                                                                                        \
                    auto t1 = std::chrono::high_resolution_clock::now();                                     \
                    double dt = std::chrono::duration<double>(t1 - t0).count();                              \
                    acc_dt += dt;                                                                            \
                    acc_ops += (ops_per_iter);                                                               \
                } while (acc_dt < target_s);                                                                 \
                const double cps = acc_ops / acc_dt;                                                         \
                if (cps < min_cps) min_cps = cps;                                                            \
                if (cps > max_cps) max_cps = cps;                                                            \
                total_dt += acc_dt;                                                                          \
                total_ops += acc_ops;                                                                        \
                state.SetIterationTime(acc_dt);                                                              \
            }                                                                                                \
            const double cps_avg = (total_dt > 0.0) ? (double(total_ops) / total_dt) : 0.0;                  \
            state.counters["cps_avg"] = cps_avg;                                                             \
            state.counters["cps_min"] = min_cps;                                                             \
            state.counters["cps_max"] = max_cps;                                                             \
            state.counters["bw_mib_s"] = benchmark::Counter(                                                 \
                cps_avg * double(bytes_per_op), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024); \
        } while (0)
#endif

template <class>
struct is_std_shared_ptr : std::false_type
{
};
template <class T>
struct is_std_shared_ptr<std::shared_ptr<T>> : std::true_type
{
};

namespace acul
{
    template <typename T, typename Allocator>
    class shared_ptr;
}

template <class>
struct is_acul_shared_ptr : std::false_type
{
};
template <class T, class Alloc>
struct is_acul_shared_ptr<acul::shared_ptr<T, Alloc>> : std::true_type
{
};

template <class Ptr>
struct pointee;
template <class T>
struct pointee<std::shared_ptr<T>>
{
    using type = T;
};
template <class T, class Alloc>
struct pointee<acul::shared_ptr<T, Alloc>>
{
    using type = T;
};

template <class Ptr>
using pointee_t = typename pointee<std::decay_t<Ptr>>::type;

template <class T>
struct dependent_false : std::false_type
{
};

template <class Ptr, class... Args>
Ptr make_shared_for_ptr(Args &&...args)
{
    using P = std::decay_t<Ptr>;
    using T = pointee_t<P>;
    if constexpr (is_std_shared_ptr<P>::value) { return std::make_shared<T>(std::forward<Args>(args)...); }
    else if constexpr (is_acul_shared_ptr<P>::value) { return acul::make_shared<T>(std::forward<Args>(args)...); }
    else { static_assert(dependent_false<P>::value, "Ptr must be std::shared_ptr<T> or acul::shared_ptr<T, Alloc>"); }
}


template <class Container, class... Args>
void emplace_shared(Container &c, Args &&...args)
{
    using Ptr = std::decay_t<typename Container::value_type>;
    static_assert(is_std_shared_ptr<Ptr>::value || is_acul_shared_ptr<Ptr>::value,
                  "Container::value_type must be std::shared_ptr<T> or acul::shared_ptr<T, Alloc>");
    auto p = make_shared_for_ptr<Ptr>(std::forward<Args>(args)...);
    c.emplace_back(std::move(p));
}


template <class Ptr, class... Args>
void make_into(Ptr &out, Args &&...args)
{
    using P = std::decay_t<Ptr>;
    static_assert(is_std_shared_ptr<P>::value || is_acul_shared_ptr<P>::value,
                  "Ptr must be std::shared_ptr<T> or acul::shared_ptr<T, Alloc>");
    out = make_shared_for_ptr<P>(std::forward<Args>(args)...);
}

template <class T>
using StdSP = std::shared_ptr<T>;
template <class T>
using StdWP = std::weak_ptr<T>;
template <class T>
using AculSP = acul::shared_ptr<T>;
template <class T>
using AculWP = acul::weak_ptr<T>;

template <class T>
static const char *type_name()
{
    if constexpr (std::is_same_v<T, uint64_t>) return "uint64_t (trivial)";
    if constexpr (std::is_same_v<T, NonTriv>) return "NonTriv (non-trivial)";
    return "T";
}
template <template <class> class SP, class T>
static const char *sp_name()
{
    if constexpr (std::is_same_v<SP<T>, AculSP<T>>)
        return "acul::shared_ptr";
    else
        return "std::shared_ptr";
}
template <template <class> class WP, class T>
static const char *wp_name()
{
    if constexpr (std::is_same_v<WP<T>, AculWP<T>>)
        return "acul::weak_ptr";
    else
        return "std::weak_ptr";
}




template <template <class> class SP, class T>
static void BM_sp_make_reset(benchmark::State &state)
{
    using SPT = SP<T>;
    constexpr bool kIsU64 = std::is_same_v<T, uint64_t>;
    const size_t N = size_t(state.range(0));
    const size_t OPS = N, BYTES = sizeof(T);

    state.SetLabel(std::string("make_reset<") + type_name<T>() + "> " + sp_name<SP, T>());

    RUN_BENCHMARK(state, OPS, BYTES, {/* no-op */}, ([&] {
                      for (size_t i = 0; i < N; ++i)
                      {
                          SPT p;
                          if constexpr (kIsU64)
                              make_into(p, i);
                          else
                              make_into(p, "val", int(i & 0x7fff));
                          benchmark::DoNotOptimize(p);
                          p.reset();
                      }
                      benchmark::ClobberMemory();
                  }()););
};


template <template <class> class SP, class T>
static void BM_sp_copy(benchmark::State &state)
{
    using SPT = SP<T>;
    constexpr bool kIsU64 = std::is_same_v<T, uint64_t>;
    const size_t N = size_t(state.range(0));
    const size_t OPS = N, BYTES = sizeof(T);

    acul::vector<SPT> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        if constexpr (kIsU64)
            emplace_shared(v, i);
        else
            emplace_shared(v, "val", int(i & 0x7fff));
    }

    state.SetLabel(std::string("copy<") + type_name<T>() + "> " + sp_name<SP, T>());

    RUN_BENCHMARK(state, OPS, BYTES, {/* no-op */}, ([&] {
                      for (size_t i = 0; i < N; ++i)
                      {
                          SPT p;
                          if constexpr (kIsU64)
                              make_into(p, i);
                          else
                              make_into(p, "val", int(i & 0x7fff));
                          benchmark::DoNotOptimize(p);
                          p.reset();
                      }
                      benchmark::ClobberMemory();
                  }()););
}


template <template <class> class SP, class T>
static void BM_sp_move(benchmark::State &state)
{
    using SPT = SP<T>;
    constexpr bool kIsU64 = std::is_same_v<T, uint64_t>;
    const size_t N = size_t(state.range(0));
    const size_t OPS = N, BYTES = sizeof(T);

    state.SetLabel(std::string("move<") + type_name<T>() + "> " + sp_name<SP, T>());

    RUN_BENCHMARK(state, OPS, BYTES, {/* no-op */}, ([&] {
                      acul::vector<SPT> src;
                      src.reserve(N);
                      for (size_t i = 0; i < N; ++i)
                      {
                          if constexpr (kIsU64)
                              emplace_shared(src, i);
                          else
                              emplace_shared(src, "val", int(i & 0x7fff));
                      }
                      acul::vector<SPT> dst;
                      dst.reserve(N);
                      for (size_t i = 0; i < N; ++i) dst.emplace_back(std::move(src[i]));
                      benchmark::DoNotOptimize(dst.data());
                      benchmark::ClobberMemory();
                  }()););
}


template <template <class> class SP, template <class> class WP, class T>
static void BM_wp_lock_hit(benchmark::State &state)
{
    using SPT = SP<T>;
    using WPT = WP<T>;
    constexpr bool kIsU64 = std::is_same_v<T, uint64_t>;
    const size_t N = size_t(state.range(0));
    const size_t OPS = N, BYTES = sizeof(T);

    acul::vector<SPT> owners;
    owners.reserve(N);
    acul::vector<WPT> weaks;
    weaks.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        if constexpr (kIsU64)
            emplace_shared(owners, i);
        else
            emplace_shared(owners, "val", int(i & 0x7fff));
        weaks.emplace_back(owners.back());
    }

    state.SetLabel(std::string("weak_lock_hit<") + type_name<T>() + "> " + wp_name<WP, T>());

    RUN_BENCHMARK(state, OPS, BYTES, {/* no-op */}, ([&] {
                      size_t hits = 0;
                      for (size_t i = 0; i < N; ++i)
                      {
                          SPT sp = weaks[i].lock();
                          hits += bool(sp);
                          benchmark::DoNotOptimize(sp);
                      }
                      benchmark::DoNotOptimize(hits);
                      benchmark::ClobberMemory();
                  }()););
}


template <template<class> class SP, template<class> class WP, class T>
static void BM_wp_lock_miss(benchmark::State& state) {
    using SPT = SP<T>;
    using WPT = WP<T>;
    constexpr bool kIsU64 = std::is_same_v<T, uint64_t>;
    const size_t N   = size_t(state.range(0));
    const size_t OPS = N, BYTES = sizeof(T);

    
    acul::vector<WPT> weaks;
    weaks.reserve(N);

    state.SetLabel(std::string("weak_lock_miss<") + type_name<T>() + "> " + wp_name<WP,T>());

    RUN_BENCHMARK(
        state, OPS, BYTES,
        {
            
            weaks.clear();               
            weaks.reserve(N);

            
            {
                acul::vector<SPT> tmp;
                tmp.reserve(N);
                for (size_t i = 0; i < N; ++i) {
                    if constexpr (kIsU64) emplace_shared(tmp, i);
                    else                  emplace_shared(tmp, "val", int(i & 0x7fff));
                    weaks.emplace_back(tmp.back());
                }
                
            }
        },
        ([&]{
            
            size_t misses = 0;
            for (size_t i = 0; i < N; ++i) {
                auto sp = weaks[i].lock();   
                misses += !bool(sp);
                benchmark::DoNotOptimize(sp);
            }
            benchmark::DoNotOptimize(misses);
            benchmark::ClobberMemory();
        }());  
    );
}



template <template <class> class SP, class T>
static void BM_sp_deref(benchmark::State &state)
{
    using SPT = SP<T>;
    constexpr bool kIsU64 = std::is_same_v<T, uint64_t>;
    const size_t N = size_t(state.range(0));
    const size_t OPS = N, BYTES = sizeof(T);

    acul::vector<SPT> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        if constexpr (kIsU64)
            emplace_shared(v, i);
        else
            emplace_shared(v, "val", int(i & 0x7fff));
    }

    state.SetLabel(std::string("deref<") + type_name<T>() + "> " + sp_name<SP, T>());

    RUN_BENCHMARK(state, OPS, BYTES, {/* no-op */}, ([&] {
                      uint64_t sink = 0;
                      if constexpr (kIsU64)
                      {
                          for (size_t i = 0; i < N; ++i) sink += *v[i];
                      }
                      else
                      {
                          for (size_t i = 0; i < N; ++i) sink += (uint64_t)v[i]->s.size() + uint64_t(v[i]->v);
                      }
                      benchmark::DoNotOptimize(sink);
                      benchmark::ClobberMemory();
                  }()););
}


#define REG_SHARED_SET(SP, WP, T, N)                                         \
    BENCHMARK_TEMPLATE(BM_sp_make_reset, SP, T)->Arg(N)->UseManualTime();    \
    BENCHMARK_TEMPLATE(BM_sp_copy, SP, T)->Arg(N)->UseManualTime();          \
    BENCHMARK_TEMPLATE(BM_sp_move, SP, T)->Arg(N)->UseManualTime();          \
    BENCHMARK_TEMPLATE(BM_wp_lock_hit, SP, WP, T)->Arg(N)->UseManualTime();  \
    BENCHMARK_TEMPLATE(BM_wp_lock_miss, SP, WP, T)->Arg(N)->UseManualTime(); \
    BENCHMARK_TEMPLATE(BM_sp_deref, SP, T)->Arg(N)->UseManualTime();

template <class T>
using StdSP = std::shared_ptr<T>;
template <class T>
using StdWP = std::weak_ptr<T>;
template <class T>
using AculSP = acul::shared_ptr<T>;
template <class T>
using AculWP = acul::weak_ptr<T>;

REG_SHARED_SET(AculSP, AculWP, uint64_t, 10000)
REG_SHARED_SET(StdSP, StdWP, uint64_t, 10000)

REG_SHARED_SET(AculSP, AculWP, NonTriv, 10000)
REG_SHARED_SET(StdSP, StdWP, NonTriv, 10000)

BENCHMARK_MAIN();
