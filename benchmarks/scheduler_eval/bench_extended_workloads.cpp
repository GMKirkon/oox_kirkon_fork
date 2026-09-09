// SPDX-License-Identifier: Apache-2.0

#include "extended_workloads.h"
#include "scheduler_metrics.h"

#include <benchmark/benchmark.h>

namespace {

void ExtendedSizes(benchmark::internal::Benchmark *benchmark) {
  if (std::getenv("OOX_BENCH_PAPER_SCALE"))
    benchmark->Arg(100000000);
}

template <scheduler_eval::PointKind Kind>
void QuickHull(benchmark::State &state) {
  const auto points = scheduler_eval::MakePoints(Kind, state.range(0));
  scheduler_eval::QuickHullMetrics details;
  benchmark::DoNotOptimize(scheduler_eval::QuickHullParallel(points, &details));
  scheduler_eval::SchedulerMetricsScope metrics(state);
  for (auto _ : state)
    benchmark::DoNotOptimize(scheduler_eval::QuickHullParallel(points));
  state.counters["partition_depth"] = details.depth;
  state.counters["partitions"] = details.partitions;
  state.SetItemsProcessed(state.iterations() * points.size());
}

void TrigramDedup(benchmark::State &state) {
  const auto keys = scheduler_eval::MakeTrigrams(state.range(0));
  scheduler_eval::DedupMetrics details;
  benchmark::DoNotOptimize(
      scheduler_eval::RemoveDuplicateStrings(keys, &details));
  scheduler_eval::SchedulerMetricsScope metrics(state);
  for (auto _ : state)
    benchmark::DoNotOptimize(scheduler_eval::RemoveDuplicateStrings(keys));
  state.counters["unique_items"] = details.unique_items;
  state.counters["hash_probes_per_item"] =
      double(details.hash_probes) / keys.size();
  state.SetItemsProcessed(state.iterations() * keys.size());
}

template <scheduler_eval::KeyKind Kind>
void RadixSort64Pairs(benchmark::State &state) {
  const auto keys = scheduler_eval::MakeKeys64(Kind, state.range(0));
  std::vector<scheduler_eval::KeyValue64> pairs(keys.size());
  for (std::size_t i = 0; i < keys.size(); ++i)
    pairs[i] = {keys[i], i};
  scheduler_eval::SchedulerMetricsScope metrics(state);
  for (auto _ : state)
    benchmark::DoNotOptimize(scheduler_eval::RadixSort64PairsParallel(pairs));
  state.SetItemsProcessed(state.iterations() * pairs.size());
}

template <scheduler_eval::KeyKind Kind>
void RadixSort64(benchmark::State &state) {
  const auto keys = scheduler_eval::MakeKeys64(Kind, state.range(0));
  scheduler_eval::RadixSortMetrics details;
  benchmark::DoNotOptimize(scheduler_eval::RadixSort64Parallel(keys, &details));
  scheduler_eval::SchedulerMetricsScope metrics(state);
  for (auto _ : state)
    benchmark::DoNotOptimize(scheduler_eval::RadixSort64Parallel(keys));
  state.counters["radix_passes"] = details.passes;
  for (std::size_t i = 0; i < details.pass_nanoseconds.size(); ++i)
    state.counters["preflight_pass_" + std::to_string(i) + "_ns"] =
        details.pass_nanoseconds[i];
  state.SetItemsProcessed(state.iterations() * keys.size());
}

#define EXTENDED_CASE(name, kind)                                              \
  BENCHMARK_TEMPLATE(name, scheduler_eval::kind)                               \
      ->RangeMultiplier(4)                                                     \
      ->Range(1 << 10, 1 << 18)                                                \
      ->Apply(ExtendedSizes)                                                   \
      ->UseRealTime()

EXTENDED_CASE(QuickHull, PointKind::UniformSquare);
EXTENDED_CASE(QuickHull, PointKind::InDisk);
EXTENDED_CASE(QuickHull, PointKind::OnCircle);
EXTENDED_CASE(QuickHull, PointKind::Kuzmin);
EXTENDED_CASE(RadixSort64, KeyKind::Uniform);
EXTENDED_CASE(RadixSort64, KeyKind::Exponential);
EXTENDED_CASE(RadixSort64, KeyKind::DuplicateHeavy);
EXTENDED_CASE(RadixSort64, KeyKind::AlmostSorted);
EXTENDED_CASE(RadixSort64, KeyKind::ReverseSorted);
EXTENDED_CASE(RadixSort64Pairs, KeyKind::Uniform);
EXTENDED_CASE(RadixSort64Pairs, KeyKind::Exponential);
EXTENDED_CASE(RadixSort64Pairs, KeyKind::DuplicateHeavy);
EXTENDED_CASE(RadixSort64Pairs, KeyKind::AlmostSorted);
EXTENDED_CASE(RadixSort64Pairs, KeyKind::ReverseSorted);
BENCHMARK(TrigramDedup)
    ->RangeMultiplier(4)
    ->Range(1 << 10, 1 << 18)
    ->Apply(ExtendedSizes)
    ->UseRealTime();

} // namespace
