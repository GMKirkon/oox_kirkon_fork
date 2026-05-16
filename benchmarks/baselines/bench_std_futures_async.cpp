// Copyright (C) 2026 OOX contributors
//
// SPDX-License-Identifier: Apache-2.0
//
// std::future / std::async baseline. Always uses std::launch::async so the
// implementation cannot pick deferred execution. Branch-agnostic and depends
// only on the standard library.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <vector>

#include <benchmark/benchmark.h>

#include "baseline_cancellation.hpp"
#include "baseline_work.hpp"

namespace {

void Baseline_FuturesAsync_Success_Single(benchmark::State& state) {
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        auto f = std::async(std::launch::async, [work] {
            return bench_base::spin_work(work);
        });
        benchmark::DoNotOptimize(f.get());
    }
}
BENCHMARK(Baseline_FuturesAsync_Success_Single)
    ->Args({1, 0})->Args({1, 16})->Args({1, 256})
    ->Unit(benchmark::kMicrosecond);

void Baseline_FuturesAsync_Success_Fanout(benchmark::State& state) {
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        auto root = std::async(std::launch::async, [work] {
            return bench_base::spin_work(work);
        }).share();

        std::vector<std::future<std::uint64_t>> branches;
        branches.reserve(width);
        for (std::size_t i = 0; i < width; ++i) {
            branches.push_back(std::async(std::launch::async, [root, work] {
                auto x = root.get();
                benchmark::DoNotOptimize(x);
                return bench_base::spin_work(work);
            }));
        }
        std::uint64_t sum = 0;
        for (auto& f : branches) {
            sum += f.get();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(Baseline_FuturesAsync_Success_Fanout)
    ->Args({1, 0})->Args({16, 0})->Args({64, 0})->Args({64, 64})
    ->Unit(benchmark::kMicrosecond);

void Baseline_FuturesAsync_Exception_SingleThrow(benchmark::State& state) {
    for (auto _ : state) {
        try {
            auto f = std::async(std::launch::async, []() -> int {
                throw bench_base::bench_exception{};
            });
            benchmark::DoNotOptimize(f.get());
            state.SkipWithError("expected exception was not thrown");
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_FuturesAsync_Exception_SingleThrow)
    ->Unit(benchmark::kMicrosecond);

void Baseline_FuturesAsync_Exception_ChainRootThrows(benchmark::State& state) {
    const std::size_t length = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::shared_future<int> prev = std::async(std::launch::async, []() -> int {
            throw bench_base::bench_exception{};
        }).share();
        for (std::size_t i = 0; i < length; ++i) {
            prev = std::async(std::launch::async, [prev] {
                return prev.get() + 1;
            }).share();
        }
        try {
            benchmark::DoNotOptimize(prev.get());
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_FuturesAsync_Exception_ChainRootThrows)
    ->Arg(1)->Arg(8)->Arg(64)->Unit(benchmark::kMicrosecond);

void Baseline_FuturesAsync_Exception_FanoutRootThrows(benchmark::State& state) {
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        auto root = std::async(std::launch::async, []() -> int {
            throw bench_base::bench_exception{};
        }).share();
        std::vector<std::future<int>> leaves;
        leaves.reserve(width);
        for (std::size_t i = 0; i < width; ++i) {
            leaves.push_back(std::async(std::launch::async, [root] {
                return root.get() + 1;
            }));
        }
        std::size_t failures = 0;
        for (auto& f : leaves) {
            try {
                benchmark::DoNotOptimize(f.get());
            } catch (const bench_base::bench_exception&) {
                ++failures;
            }
        }
        if (failures != width) {
            state.SkipWithError("not all leaves observed the root exception");
            break;
        }
    }
}
BENCHMARK(Baseline_FuturesAsync_Exception_FanoutRootThrows)
    ->Arg(1)->Arg(8)->Arg(64)->Unit(benchmark::kMicrosecond);

void Baseline_FuturesAsync_Branch_TokenCancel(benchmark::State& state) {
    bench_base::branch_config cfg{
        static_cast<std::size_t>(state.range(0)),
        static_cast<std::size_t>(state.range(1)),
        0,
        static_cast<std::size_t>(state.range(1)) / 2,
        16,
        static_cast<std::size_t>(state.range(2)),
    };

    std::uint64_t total_visited = 0;
    int last_winner = -1;
    for (auto _ : state) {
        std::atomic<std::uint64_t> visited{0};
        std::atomic<int> winner{-1};
        bench_base::cancellation_source source;
        auto token = source.token();

        std::vector<std::future<void>> futures;
        futures.reserve(cfg.branch_count);
        for (std::size_t b = 0; b < cfg.branch_count; ++b) {
            futures.push_back(std::async(std::launch::async,
                [b, cfg, token, &visited, &winner, &source] {
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
                }));
        }
        for (auto& f : futures) {
            f.get();
        }

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
BENCHMARK(Baseline_FuturesAsync_Branch_TokenCancel)
    ->Args({4, 1024, 16})->Args({8, 1024, 16})->Args({16, 1024, 16})
    ->Unit(benchmark::kMicrosecond);

} // namespace

BENCHMARK_MAIN();
