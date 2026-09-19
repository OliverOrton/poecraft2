# Reproduction sources

Run from the repository root. These are thin task adapters over existing native
and supervision owners, not another solver or supervisor. Completed results must
be reused under their recorded identities; reproduction requires new output names
and fresh host admission. Do not rerun the long batch merely to inspect this commit.

- `run-worker-A7.py` declares the final serial manual/default/default/manual
  Conquest pair, Conquest-four, Regalia and actual Calculator Finish/setup-Cancel.
  It uses `run_isolated_process`, with 315-second host watchdog per case.
- `run-native.py` resolves declared jobs through `resolve_case_execution`, then
  uses `run_corpus` serially. `../A7-native-jobs.json` and
  `../conquest-four-comparison-jobs.json` preserve exact treatments, prices,
  capacities and executable hashes. A6 jobs were prepared, not timed.
- `summarize-qualification.py` projects existing bulk reports and checks actual
  Calculator graph/goal/price correspondence. It launches no solver.
- `observation-probe.cpp` directly calls the existing checked-model and native
  observation fixed-point owners on two saved Ring controllers. No rows are built.
- `response-cost-probe.cpp` independently cold-evaluates those same complete
  graphs, preserving original-price rewards and separate primitive counts.
  The extra 400M work guard is conservative; standalone public evaluation has
  no finite work limit. `run-response-cost.py` resolves the A7 Ring prices,
  admits the host and calls the existing supervisor with a 60-second watchdog.
- `summarize-diagnostics.py` projects both results and compares exact control
  syntax. It grants no native row/member correspondence or reuse authority.

The C++ probes were built with `C:/msys64/ucrt64/bin/g++.exe -std=c++20 -O2`,
includes `engine/include` and `engine/src`, and
`build/engine/libpoecraft_engine.a`. Executables and bulk output live under
`out/verified-delivery`; source/library/executable/graph hashes are in the receipts.
Python execution uses `PYTHONPATH=tools/ingest;bindings/python` and `py -3`.

The public ABI check reuses the preceding programme's
[old-header source](../../2026-09-14-progress-and-delivery/probes/legacy-progress-probe.cpp)
and header, linked against A7. Focused test/build commands and log hashes are in
[validation](../validation.json). Actual DOM probe source is
`apps/web/test/calculator-delivery-probe.ts`; rendered UI review remains with
Oliver. Imported synthetic reference checks were not rerun.
