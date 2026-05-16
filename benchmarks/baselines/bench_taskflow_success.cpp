// Copyright (C) 2026 OOX contributors
//
// SPDX-License-Identifier: Apache-2.0
//
// Taskflow cross-framework baseline benchmarks. Branch-agnostic — identical
// across main, PR24, PR28, PR29. Reuses a single tf::Executor per benchmark
// function so executor construction stays out of the timed loop.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <benchmark/benchmark.h>

#include <taskflow/taskflow.hpp>

#include "baseline_cancellation.hpp"
#include "baseline_work.hpp"

namespace {

tf::Executor& shared_executor() {
    static tf::Executor executor(bench_base::benchmark_workers());
    return executor;
}

// --------------------------------------------------------------------------
// Success
// --------------------------------------------------------------------------

void Baseline_Taskflow_Success_Single(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        tf::Taskflow taskflow;
        taskflow.emplace([work] {
            benchmark::DoNotOptimize(bench_base::spin_work(work));
        });
        executor.run(taskflow).wait();
    }
}
BENCHMARK(Baseline_Taskflow_Success_Single)
    ->Args({1, 0})->Args({1, 16})->Args({1, 256})
    ->Unit(benchmark::kMicrosecond);

void Baseline_Taskflow_Success_Chain(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
    const std::size_t length = static_cast<std::size_t>(state.range(0));
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        tf::Taskflow taskflow;
        std::vector<tf::Task> tasks;
        tasks.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            tasks.push_back(taskflow.emplace([work] {
                benchmark::DoNotOptimize(bench_base::spin_work(work));
            }));
            if (i > 0) {
                tasks[i - 1].precede(tasks[i]);
            }
        }
        executor.run(taskflow).wait();
    }
}
BENCHMARK(Baseline_Taskflow_Success_Chain)
    ->Args({1, 0})->Args({16, 0})->Args({64, 0})->Args({64, 64})
    ->Unit(benchmark::kMicrosecond);

void Baseline_Taskflow_Success_Fanout(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        tf::Taskflow taskflow;
        auto start = taskflow.emplace([] {});
        for (std::size_t i = 0; i < width; ++i) {
            auto t = taskflow.emplace([work] {
                benchmark::DoNotOptimize(bench_base::spin_work(work));
            });
            start.precede(t);
        }
        executor.run(taskflow).wait();
    }
}
BENCHMARK(Baseline_Taskflow_Success_Fanout)
    ->Args({1, 0})->Args({16, 0})->Args({64, 0})->Args({64, 64})
    ->Unit(benchmark::kMicrosecond);

// --------------------------------------------------------------------------
// Branch cancellation via the shared cooperative token.
// --------------------------------------------------------------------------

void Baseline_Taskflow_Branch_TokenCancel(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
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

        tf::Taskflow taskflow;
        auto start = taskflow.emplace([] {});
        for (std::size_t b = 0; b < cfg.branch_count; ++b) {
            auto t = taskflow.emplace([b, cfg, token, &visited, &winner, &source] {
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
            start.precede(t);
        }
        executor.run(taskflow).wait();

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
BENCHMARK(Baseline_Taskflow_Branch_TokenCancel)
    ->Args({4, 1024, 16})->Args({8, 1024, 16})->Args({16, 1024, 16})
    ->Unit(benchmark::kMicrosecond);

// --------------------------------------------------------------------------
// Exception scenarios. Taskflow propagates the exception out of the future
// returned by executor.run(); .get() rethrows. Requires Taskflow >= 3.7.0;
// older versions silently swallow exceptions from task bodies, so these
// rows will SkipWithError if linked against an older Taskflow.
// --------------------------------------------------------------------------

void Baseline_Taskflow_Exception_SingleThrow(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
    for (auto _ : state) {
        tf::Taskflow taskflow;
        taskflow.emplace([] { throw bench_base::bench_exception{}; });
        try {
            executor.run(taskflow).get();
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_Taskflow_Exception_SingleThrow)->Unit(benchmark::kMicrosecond);

void Baseline_Taskflow_Exception_ChainRootThrows(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
    const std::size_t length = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        tf::Taskflow taskflow;
        auto bad = taskflow.emplace([] { throw bench_base::bench_exception{}; });
        tf::Task prev = bad;
        for (std::size_t i = 0; i < length; ++i) {
            auto t = taskflow.emplace([] {});
            prev.precede(t);
            prev = t;
        }
        try {
            executor.run(taskflow).get();
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_Taskflow_Exception_ChainRootThrows)
    ->Arg(1)->Arg(8)->Arg(64)->Unit(benchmark::kMicrosecond);

void Baseline_Taskflow_Exception_FanoutRootThrows(benchmark::State& state) {
    tf::Executor& executor = shared_executor();
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        tf::Taskflow taskflow;
        auto bad = taskflow.emplace([] { throw bench_base::bench_exception{}; });
        for (std::size_t i = 0; i < width; ++i) {
            auto t = taskflow.emplace([] {});
            bad.precede(t);
        }
        try {
            executor.run(taskflow).get();
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_Taskflow_Exception_FanoutRootThrows)
    ->Arg(1)->Arg(8)->Arg(64)->Unit(benchmark::kMicrosecond);

} // namespace

BENCHMARK_MAIN();
