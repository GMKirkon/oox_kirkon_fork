// Copyright (C) 2026 OOX contributors
//
// SPDX-License-Identifier: Apache-2.0
//
// Deterministic synthetic work + a benchmark exception type shared by all
// cross-framework baseline benchmarks. Branch-agnostic — the file is identical
// across main, PR24, PR28, PR29.

#ifndef OOX_BENCH_BASELINE_WORK_HPP
#define OOX_BENCH_BASELINE_WORK_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

#include <benchmark/benchmark.h>

namespace bench_base {

inline std::uint64_t spin_work(std::size_t iterations) {
    std::uint64_t x = 0x9e3779b97f4a7c15ull;
    for (std::size_t i = 0; i < iterations; ++i) {
        x ^= x << 7;
        x ^= x >> 9;
        x += i + 0x85ebca6bull;
        benchmark::DoNotOptimize(x);
    }
    return x;
}

inline void counted_work(std::size_t iterations,
                         std::atomic<std::uint64_t>& visited) {
    visited.fetch_add(1, std::memory_order_relaxed);
    benchmark::DoNotOptimize(spin_work(iterations));
}

struct bench_exception : std::runtime_error {
    bench_exception() : std::runtime_error("benchmark exception") {}
};

inline std::size_t benchmark_workers() {
    if (const char* env = std::getenv("OOX_BENCH_WORKERS")) {
        const auto n = std::strtoull(env, nullptr, 10);
        if (n > 0) {
            return static_cast<std::size_t>(n);
        }
    }
    const auto hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1u : hw;
}

} // namespace bench_base

#endif // OOX_BENCH_BASELINE_WORK_HPP
