# Benchmark porting plan coverage

This file distinguishes executable coverage from reproduction of published
experiments. A green CI run does not establish the latter.

| Plan item | Executable coverage | Remaining reproduction work |
| --- | --- | --- |
| Nested BFS | Flat, fixed-grain, and online adaptive traversal; synthetic graph families; PBBS text and PASL binary graph loading; serial level oracle | Exact PASL generator parameter presets and licensed LiveJournal/Twitter/Wikipedia/Europe inputs; published machine-scale runs |
| QuickHull | PBBS canonical application plus native frontier-parallel QuickHull, with monotone-chain oracle and partition depth | Native variant uses extended-precision predicates and breadth-wise partitions; it is not a verbatim PBBS port or a floating-point reproduction of its circle survivor count |
| Deduplication | Scalar atomic hash table, native string hash table, synthetic word triples; canonical PBBS trigram cases through application adapter | Native word triples are not PBBS's language model; canonical generator remains in PBBS |
| Radix sort | Stable 32-bit and 64-bit scalar/pair kernels; untimed per-pass samples; full-output oracle | Tuning pass width remains experimental work |
| Sample sort | Native scalar sampling/bucketing and recursive refinement of large buckets; canonical PBBS types/generators via adapter | Native string and record variants remain supplied by PBBS |
| Secondary applications | All 12 PBBS application families compile through both scheduler and OOX task adapters, including suffix array, octree, triangulation/refinement, ray casting and forests | Full dataset runs and independent serial baselines for every secondary algorithm |
| Synthetic experiments | Nine cost distributions, concurrent caller sweeps, temporary worker occupation/release, serial/parallel first-touch cases, NUMA placement and perf wrapper | Hardware-specific remote-access measurements and LIKWID/PAPI integrations |
| Metrics | Actual executing pool's task/steal/sleep counters; workload descriptors, preflight radix pass samples, graph SHA-256; Heartbeat comparison export | Exact utilization and Heartbeat promotion semantics are unmeasured, explicitly null or documented in the export |
| Historical comparison | Pinned Eigen/PBBS reference available | Runnable pinned Heartbeat, SPTL and PASL comparison environments |

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
