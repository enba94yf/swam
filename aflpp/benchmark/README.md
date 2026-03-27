# American Fuzzy Lop plus plus (AFL++)

## benchmarking

This directory describes how AFL++ benchmarking works in a full source
distribution.

This artifact does not ship the benchmarking scripts and sample data files
(for example `benchmark.py`, `benchmark-results.jsonl`, and `benchmark.ipynb`).
Only `COMPARISON.md` is kept.

To achieve this, we use a sample program ("test-instr.c") where each path is
equally likely, supply it a single seed, and tell AFL to exit after one run of
deterministic mutations against that seed.

**Note that this is not a real-world scenario!**
Because the target does basically nothing this is rather a stress test on
Kernel I/O / context switching.
For this reason you will not see a difference if you run the multicore test
with 20 or 40 threads - or even see the performance decline the more threads
(`-f` parameter) you use. In a real-world scenario you can expect to gain
exec/s until 40-60 threads (if you have that many available on your CPU).

To run the benchmark suite, use a full AFL++ source distribution that includes
the benchmarking scripts.
