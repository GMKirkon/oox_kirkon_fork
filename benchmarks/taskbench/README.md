# OOX Task-Bench-Style Benchmark

This directory contains a single-node Task-Bench-style benchmark for OOX. It keeps graph and kernel definitions independent from the OOX runner so new dependency patterns can be reused by other runners.

Build with benchmarks enabled:

```bash
cmake --build build --target bench_taskbench_TBB.exe
```

Run a small OOX benchmark:

```bash
./build/benchmarks/taskbench/bench_taskbench_TBB.exe \
  --height=1000 \
  --width=auto \
  --threads=auto \
  --graphs=1 \
  --pattern=stencil \
  --kernel=compute \
  --iterations=1024 \
  --output-bytes=16 \
  --repetitions=5 \
  --csv
```

Run the benchmark workflow from the repository root:

```bash
python scripts/run_benchmarks.py --skip-ctest --skip-big-graph-smoke
```

For a fast smoke version:

```bash
python scripts/run_benchmarks.py --quick --skip-ctest --skip-big-graph-smoke
```

The workflow builds benchmark targets, captures machine metadata, runs TaskBench sweeps, and writes CSV outputs under `results/benchmarks-*/taskbench/`.

Run a correctness baseline through the serial runner:

```bash
./build/benchmarks/taskbench/bench_taskbench_TBB.exe \
  --runner=serial \
  --height=8 \
  --width=4 \
  --pattern=nearest \
  --radix=3 \
  --iterations=1 \
  --repetitions=1
```

Run direct runtime baselines from the matching backend executables:

```bash
./build/benchmarks/taskbench/bench_taskbench_TBB.exe --runner=tbb-flow --height=1000 --width=auto --threads=auto --pattern=stencil --iterations=1024 --csv
./build/benchmarks/taskbench/bench_taskbench_TF.exe --runner=taskflow --height=1000 --width=auto --threads=auto --pattern=stencil --iterations=1024 --csv
./build/benchmarks/taskbench/bench_taskbench_FOLLY.exe --runner=folly --height=1000 --width=auto --threads=auto --pattern=stencil --iterations=1024 --csv
./build/benchmarks/taskbench/bench_taskbench_OMP.exe --runner=openmp --height=1000 --width=auto --threads=auto --pattern=stencil --iterations=1024 --csv
```

`--runner=oox` measures OOX using the executable's selected backend. `--runner=tbb-flow`, `--runner=taskflow`, `--runner=openmp`, and `--runner=folly` measure direct runtime baselines against the same shared TaskBench graph/kernels. oneTBB Flow Graph, Taskflow, and OpenMP build task dependencies directly; the Folly baseline schedules one row of futures at a time because Folly futures are move-only and stencil fanout needs multi-consumer predecessor values.

The Task Bench paper includes many more systems: HPX/AMT, Cilk, Charm++/AM++, StarPU, Legion, PaRSEC, Dask, Ray, Parsl, Spark, Flink, Storm, and TensorFlow. Those are good candidates for optional adapter executables, but they should stay behind explicit dependency flags because they require larger runtime installations or distributed launchers.

The CSV rows include backend, runner, graph shape, kernel parameters, task and edge counts, wall time, estimated task granularity, throughput, and validation status. The executable reports one row per measured repetition; warmup runs are omitted.
