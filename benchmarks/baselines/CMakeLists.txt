# Copyright (C) 2026 OOX contributors
#
# SPDX-License-Identifier: Apache-2.0
#

option(OOX_BENCH_BASELINE_TBB_FLOW "Build oneTBB flow_graph baseline benchmarks" ON)
option(OOX_BENCH_BASELINE_TASKFLOW "Build Taskflow baseline benchmarks" ON)
option(OOX_BENCH_BASELINE_STD_EXEC "Build std::execution baseline benchmarks" ON)
option(OOX_BENCH_BASELINE_FUTURES "Build std::future baseline benchmarks" ON)

# ---------------------------------------------------------------------------
# oneTBB Flow Graph baseline
# ---------------------------------------------------------------------------
if(OOX_BENCH_BASELINE_TBB_FLOW)
  if(TBB_FOUND)
    add_executable(oox_bench_baseline_tbb_flow bench_tbb_flow_success.cpp)
    target_link_libraries(oox_bench_baseline_tbb_flow PRIVATE
      benchmark::benchmark ${TBB_LIBS})
    target_include_directories(oox_bench_baseline_tbb_flow PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(oox_bench_baseline_tbb_flow oox_bench_baseline_tbb_flow
      --benchmark_min_time=0 --benchmark_repetitions=1
      --benchmark_min_warmup_time=0
      --benchmark_filter=Baseline_TbbFlow_Success_Single)
  else()
    message(STATUS "Skipping oneTBB Flow Graph baseline: TBB not found")
  endif()
endif()

# ---------------------------------------------------------------------------
# Taskflow baseline
# ---------------------------------------------------------------------------
if(OOX_BENCH_BASELINE_TASKFLOW)
  if(Taskflow_FOUND)
    add_executable(oox_bench_baseline_taskflow bench_taskflow_success.cpp)
    target_link_libraries(oox_bench_baseline_taskflow PRIVATE
      benchmark::benchmark ${TF_LIBS})
    target_include_directories(oox_bench_baseline_taskflow PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(oox_bench_baseline_taskflow oox_bench_baseline_taskflow
      --benchmark_min_time=0 --benchmark_repetitions=1
      --benchmark_min_warmup_time=0
      --benchmark_filter=Baseline_Taskflow_Success_Single)
  else()
    message(STATUS "Skipping Taskflow baseline: Taskflow not found")
  endif()
endif()

# ---------------------------------------------------------------------------
# std::execution baseline
# ---------------------------------------------------------------------------
if(OOX_BENCH_BASELINE_STD_EXEC)
  include(CheckCXXSourceCompiles)
  set(_oox_par_exec_check
    [=[
      #include <algorithm>
      #include <execution>
      #include <vector>
      int main() {
          std::vector<int> v{1, 2, 3};
          std::for_each(std::execution::par, v.begin(), v.end(), [](int){});
          return 0;
      }
    ]=])
  check_cxx_source_compiles("${_oox_par_exec_check}" OOX_HAS_STD_EXECUTION_PAR)
  if(OOX_HAS_STD_EXECUTION_PAR)
    add_executable(oox_bench_baseline_std_execution bench_std_execution.cpp)
    target_link_libraries(oox_bench_baseline_std_execution PRIVATE
      benchmark::benchmark)
    target_include_directories(oox_bench_baseline_std_execution PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR})
    if(TBB_FOUND)
      # libstdc++ parallel algorithms typically need TBB at link time.
      target_link_libraries(oox_bench_baseline_std_execution PRIVATE ${TBB_LIBS})
    endif()
    add_test(oox_bench_baseline_std_execution oox_bench_baseline_std_execution
      --benchmark_min_time=0 --benchmark_repetitions=1
      --benchmark_min_warmup_time=0
      --benchmark_filter=Baseline_StdExecution_Success_ForEach)
  else()
    message(STATUS "Skipping std::execution baseline: std::execution::par unavailable")
  endif()
endif()

# ---------------------------------------------------------------------------
# std::future / std::async baseline
# ---------------------------------------------------------------------------
if(OOX_BENCH_BASELINE_FUTURES)
  add_executable(oox_bench_baseline_futures_async bench_std_futures_async.cpp)
  target_link_libraries(oox_bench_baseline_futures_async PRIVATE
    benchmark::benchmark Threads::Threads)
  target_include_directories(oox_bench_baseline_futures_async PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(oox_bench_baseline_futures_async oox_bench_baseline_futures_async
    --benchmark_min_time=0 --benchmark_repetitions=1
    --benchmark_min_warmup_time=0
    --benchmark_filter=Baseline_FuturesAsync_Success_Single)
endif()
