# Continuous integration

One workflow, `.github/workflows/ci.yml`. The `lint` and `test-ref`
jobs run on GitHub-hosted runners for every push to main and every pull
request, and both are intended as required checks.

`lint` runs `scripts/lint_check.py` (clang-format 18, pinned by the
ubuntu-24.04 image).

`test-ref` builds without CUDA, so only the ref backend and its tests
are compiled, then runs the ref test binaries against a pattern file in
the build directory and the Python binding tests under `python/tests`.

The cufile and aisio backends need CUDA, the pinned dependency stack
and a machine with an NVMe device and a GPU with dma-buf P2P support,
so the hosted jobs do not cover them. Their suite runs on a self-hosted
runner behind the `test-full` label, and the benchmarks behind the
`bench` labels; see ci-hw.md. Without a label those checks are skipped,
and the manual flow in the README (`rsync.py`, `build.py`,
`run_tests.py` against a configured target) remains available.
