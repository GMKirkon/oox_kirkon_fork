// Copyright (C) 2026 OOX contributors
//
// SPDX-License-Identifier: Apache-2.0
//
// oneTBB Flow Graph cross-framework baseline benchmarks. This file is
// intentionally branch-agnostic (no <oox/oox.h>) and identical across all
// OOX branches.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <benchmark/benchmark.h>

#include <oneapi/tbb/flow_graph.h>
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/task_group.h> // task_group_context

#include "baseline_cancellation.hpp"
#include "baseline_work.hpp"

namespace tflow = oneapi::tbb::flow;

namespace {

oneapi::tbb::global_control& worker_control() {
    static oneapi::tbb::global_control control(
        oneapi::tbb::global_control::max_allowed_parallelism,
        bench_base::benchmark_workers());
    return control;
}

void touch_worker_control() { (void)worker_control(); }

// --------------------------------------------------------------------------
// Success
// --------------------------------------------------------------------------

void Baseline_TbbFlow_Success_Single(benchmark::State& state) {
    touch_worker_control();
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);
        tflow::continue_node<tflow::continue_msg> node(
            g, [work](tflow::continue_msg) {
                benchmark::DoNotOptimize(bench_base::spin_work(work));
            });
        make_edge(start, node);
        start.try_put({});
        g.wait_for_all();
    }
}
BENCHMARK(Baseline_TbbFlow_Success_Single)
    ->Args({1, 0})->Args({1, 16})->Args({1, 256})
    ->Unit(benchmark::kMicrosecond);

void Baseline_TbbFlow_Success_Chain(benchmark::State& state) {
    touch_worker_control();
    const std::size_t length = static_cast<std::size_t>(state.range(0));
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);

        std::vector<tflow::continue_node<tflow::continue_msg>> nodes;
        nodes.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            nodes.emplace_back(g, [work](tflow::continue_msg) {
                benchmark::DoNotOptimize(bench_base::spin_work(work));
            });
        }
        if (!nodes.empty()) {
            make_edge(start, nodes.front());
            for (std::size_t i = 1; i < nodes.size(); ++i) {
                make_edge(nodes[i - 1], nodes[i]);
            }
        }
        start.try_put({});
        g.wait_for_all();
    }
}
BENCHMARK(Baseline_TbbFlow_Success_Chain)
    ->Args({1, 0})->Args({16, 0})->Args({64, 0})->Args({64, 64})
    ->Unit(benchmark::kMicrosecond);

void Baseline_TbbFlow_Success_Fanout(benchmark::State& state) {
    touch_worker_control();
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    const std::size_t work = static_cast<std::size_t>(state.range(1));
    for (auto _ : state) {
        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);

        std::vector<tflow::continue_node<tflow::continue_msg>> nodes;
        nodes.reserve(width);
        for (std::size_t i = 0; i < width; ++i) {
            nodes.emplace_back(g, [work](tflow::continue_msg) {
                benchmark::DoNotOptimize(bench_base::spin_work(work));
            });
            make_edge(start, nodes.back());
        }
        start.try_put({});
        g.wait_for_all();
    }
}
BENCHMARK(Baseline_TbbFlow_Success_Fanout)
    ->Args({1, 0})->Args({16, 0})->Args({64, 0})->Args({64, 64})
    ->Unit(benchmark::kMicrosecond);

// --------------------------------------------------------------------------
// Branch cancellation via the shared cooperative token.
// --------------------------------------------------------------------------

void Baseline_TbbFlow_Branch_TokenCancel(benchmark::State& state) {
    touch_worker_control();
    bench_base::branch_config cfg{
        static_cast<std::size_t>(state.range(0)),
        static_cast<std::size_t>(state.range(1)),
        0, // winner_branch (first-winner; vary across multiple benchmarks)
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

        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);

        std::vector<tflow::continue_node<tflow::continue_msg>> nodes;
        nodes.reserve(cfg.branch_count);
        for (std::size_t b = 0; b < cfg.branch_count; ++b) {
            nodes.emplace_back(g, [b, cfg, token, &visited, &winner, &source](tflow::continue_msg) {
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
            make_edge(start, nodes.back());
        }
        start.try_put({});
        g.wait_for_all();

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
BENCHMARK(Baseline_TbbFlow_Branch_TokenCancel)
    ->Args({4, 1024, 16})->Args({8, 1024, 16})->Args({16, 1024, 16})
    ->Unit(benchmark::kMicrosecond);

// --------------------------------------------------------------------------
// Exception scenarios. oneTBB propagates exceptions thrown inside graph
// node bodies through wait_for_all on the invoking thread; the graph's
// task_group_context becomes cancelled and pending work is dropped.
// --------------------------------------------------------------------------

void Baseline_TbbFlow_Exception_SingleThrow(benchmark::State& state) {
    touch_worker_control();
    for (auto _ : state) {
        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);
        tflow::continue_node<tflow::continue_msg> bad(
            g, [](tflow::continue_msg) {
                throw bench_base::bench_exception{};
            });
        make_edge(start, bad);
        start.try_put({});
        try {
            g.wait_for_all();
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_TbbFlow_Exception_SingleThrow)->Unit(benchmark::kMicrosecond);

void Baseline_TbbFlow_Exception_ChainRootThrows(benchmark::State& state) {
    touch_worker_control();
    const std::size_t length = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);
        tflow::continue_node<tflow::continue_msg> bad(
            g, [](tflow::continue_msg) {
                throw bench_base::bench_exception{};
            });
        make_edge(start, bad);

        std::vector<tflow::continue_node<tflow::continue_msg>> chain;
        chain.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            chain.emplace_back(g, [](tflow::continue_msg) {});
            if (i == 0) {
                make_edge(bad, chain.front());
            } else {
                make_edge(chain[i - 1], chain.back());
            }
        }
        start.try_put({});
        try {
            g.wait_for_all();
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_TbbFlow_Exception_ChainRootThrows)
    ->Arg(1)->Arg(8)->Arg(64)->Unit(benchmark::kMicrosecond);

void Baseline_TbbFlow_Exception_FanoutRootThrows(benchmark::State& state) {
    touch_worker_control();
    const std::size_t width = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        tflow::graph g;
        tflow::broadcast_node<tflow::continue_msg> start(g);
        tflow::continue_node<tflow::continue_msg> bad(
            g, [](tflow::continue_msg) {
                throw bench_base::bench_exception{};
            });
        make_edge(start, bad);

        std::vector<tflow::continue_node<tflow::continue_msg>> leaves;
        leaves.reserve(width);
        for (std::size_t i = 0; i < width; ++i) {
            leaves.emplace_back(g, [](tflow::continue_msg) {});
            make_edge(bad, leaves.back());
        }
        start.try_put({});
        try {
            g.wait_for_all();
            state.SkipWithError("expected exception was not thrown");
            break;
        } catch (const bench_base::bench_exception&) {
            benchmark::DoNotOptimize(true);
        }
    }
}
BENCHMARK(Baseline_TbbFlow_Exception_FanoutRootThrows)
    ->Arg(1)->Arg(8)->Arg(64)->Unit(benchmark::kMicrosecond);

} // namespace

BENCHMARK_MAIN();
