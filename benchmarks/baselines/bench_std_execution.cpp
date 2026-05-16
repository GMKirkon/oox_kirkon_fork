// Copyright (C) 2026 OOX contributors
//
// SPDX-License-Identifier: Apache-2.0
//
// std::execution / parallel-algorithms baseline. By design only data-parallel
// scenarios are implemented here; std::execution is not a task graph runtime
// and exception scenarios are intentionally NOT registered (WG21 says throws
// from parallel-algorithm element-access functions may call std::terminate).

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include <benchmark/benchmark.h>

#if defined(__has_include)
#  if __has_include(<execution>)
#    include <execution>
#    if defined(__cpp_lib_execution) && __cpp_lib_execution >= 201603L
#      define OOX_BENCH_HAS_PAR_EXECUTION 1
#    endif
#  endif
#endif

#include "baseline_cancellation.hpp"
#include "baseline_work.hpp"

namespace {

#if defined(OOX_BENCH_HAS_PAR_EXECUTION)

void Baseline_StdExecution_Success_ForEach(benchmark::State& state) {
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    std::vector<std::size_t> items(width);
    std::iota(items.begin(), items.end(), 0);
    for (auto _ : state) {
        std::for_each(std::execution::par, items.begin(), items.end(),
                      [work](auto i) {
                          benchmark::DoNotOptimize(i);
                          benchmark::DoNotOptimize(bench_base::spin_work(work));
                      });
    }
}
BENCHMARK(Baseline_StdExecution_Success_ForEach)
    ->Args({1, 0})->Args({16, 0})->Args({64, 0})->Args({64, 64})
    ->Unit(benchmark::kMicrosecond);

void Baseline_StdExecution_Branch_TokenCancel(benchmark::State& state) {
    bench_base::branch_config cfg{
        static_cast<std::size_t>(state.range(0)),
        static_cast<std::size_t>(state.range(1)),
        0,
        static_cast<std::size_t>(state.range(1)) / 2,
        16,
        static_cast<std::size_t>(state.range(2)),
    };
    std::vector<std::size_t> branches(cfg.branch_count);
    std::iota(branches.begin(), branches.end(), 0);

    std::uint64_t total_visited = 0;
    int last_winner = -1;
    for (auto _ : state) {
        std::atomic<std::uint64_t> visited{0};
        std::atomic<int> winner{-1};
        bench_base::cancellation_source source;
        auto token = source.token();

        std::for_each(std::execution::par, branches.begin(), branches.end(),
            [cfg, token, &visited, &winner, &source](std::size_t b) {
                for (std::size_t i = 0; i < cfg.leaves_per_branch; ++i) {
                    if (cfg.poll_interval > 0 &&
                        (i % cfg.poll_interval) == 0 &&
                        token.stop_requested()) {
                        return;
                    }
                    visited.fetch_add(1, std::memory_order_relaxed);
                    benchmark::DoNotOptimize(bench_base::spin_work(cfg.work_iterations));
                    if (b == cfg.winner_branch && i == cfg.winner_leaf) {
                        int expected = -1;
                        if (winner.compare_exchange_strong(expected, static_cast<int>(b))) {
                            source.request_cancel();
                        }
                        return;
                    }
                }
            });

        last_winner = winner.load(std::memory_order_relaxed);
        total_visited += visited.load(std::memory_order_relaxed);
    }

    state.counters["branches"] = static_cast<double>(cfg.branch_count);
    state.counters["leaves"] = static_cast<double>(cfg.leaves_per_branch);
    state.counters["work"] = static_cast<double>(cfg.work_iterations);
    state.counters["poll"] = static_cast<double>(cfg.poll_interval);
    state.counters["winner"] = static_cast<double>(last_winner);
    state.counters["visited_total"] = static_cast<double>(total_visited);
    state.counters["visited_per_iter"] = static_cast<double>(total_visited) /
        (state.iterations() > 0 ? static_cast<double>(state.iterations()) : 1.0);
}
BENCHMARK(Baseline_StdExecution_Branch_TokenCancel)
    ->Args({4, 1024, 16})->Args({8, 1024, 16})->Args({16, 1024, 16})
    ->Unit(benchmark::kMicrosecond);

#else

void Baseline_StdExecution_Unsupported(benchmark::State& state) {
    state.SkipWithError("std::execution::par is not available on this toolchain");
    for (auto _ : state) {}
}
BENCHMARK(Baseline_StdExecution_Unsupported);

#endif // OOX_BENCH_HAS_PAR_EXECUTION

} // namespace

BENCHMARK_MAIN();
