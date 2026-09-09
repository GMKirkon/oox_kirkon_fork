# Benchmark porting plan coverage

This file distinguishes executable coverage from reproduction of published
experiments. A green CI run does not establish the latter.

| Plan item | Executable coverage | Remaining reproduction work |
| --- | --- | --- |
| Nested BFS | Flat, fixed-grain, and online adaptive traversal; configurable source; PASL topology variants and ten revision-pinned generator presets including SC15 trunk-first/RMat; PBBS RMat24/RMat27, random-local and cube-grid generation tooling (smoke-tested); text/binary loading | Original PASL generator execution; exact artifact dataset provenance; licensed real datasets and published machine-scale runs |
| QuickHull | PBBS canonical application plus native frontier-parallel QuickHull, with monotone-chain oracle and partition depth | Native variant uses extended-precision predicates and breadth-wise partitions; it is not a verbatim PBBS port or a floating-point reproduction of its circle survivor count |
| Deduplication | Scalar atomic hash table, native string hash table, synthetic word triples; canonical PBBS trigram cases through application adapter | Native word triples are not PBBS's language model; canonical generator remains in PBBS |
| Radix sort | Stable 32-bit and 64-bit scalar/pair kernels; 4/8/11-bit digit experiments; untimed per-pass samples; full-output oracle | Hardware-specific tuning and paper-scale measurements |
| Sample sort | Native scalar, string and 64-bit record sampling/bucketing; bounded recursive refinement; canonical PBBS generators via adapter | Paper-scale measurements |
| Secondary applications | All 12 PBBS application families through scheduler/OOX task adapters; eight original serial implementations, including divsufsort and both forests; native serial elision | Full dataset runs and independent serial timing baselines for the remaining geometry applications |
| Synthetic experiments | Nine cost distributions, concurrent caller sweeps, temporary worker occupation/release, fresh-mapping first-touch cases, NUMA placement, perf and LIKWID wrappers | Hardware-specific remote-access measurements and PAPI instrumentation |
| Metrics | Actual executing pool's task/steal/sleep counters; workload descriptors, preflight radix pass samples, graph SHA-256; Heartbeat comparison export | Exact utilization and Heartbeat promotion semantics are unmeasured, explicitly null or documented in the export |
| Historical comparison | Pinned Eigen/PBBS reference and revision-checked PASL/Heartbeat/SPTL invocation/metric-ingestion tools | Building and running historical environments and versioning all their auxiliary dependencies |

`OOX_TASKS` evaluates `oox::run` and `oox::var` continuation joins in the native
suite. The `oox-tasks` PBBS adapter uses the same task graph primitives for
parallel loops and fork/join. The older `oox` PBBS backend and `EIGEN_*` native
modes measure the lower-level scheduler adapter and remain separately named.

Native large primary cases are opt-in with `--paper-scale` (100 million
elements). CI runs bounded cases and independent correctness oracles. No
published speedup, work-efficiency ratio, or full-plan completion is claimed.

PASL binary layout was checked against the original
[graphio.hpp](https://github.com/deepsea-inria/pasl/blob/master/graph/include/graphio.hpp):
five 64-bit header fields, followed by n+1 offsets and m neighbors of the stated
32/64-bit width. The loader independently validates counts, monotonic offsets,
vertex bounds, truncation, and trailing data; both byte orders are supported.
