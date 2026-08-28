# Native Solver Lab v0 Result

**Status: complete.** Completed 2026-08-28 on `main`, local-only. Final
implementation checkpoint before archival documentation: `41a316a`.

Parent: [Native Solver Lab v0](README.md)

## Outcome

The repository now has a usable local Windows-native solver experiment
workbench around the existing benchmark. The Lab does not create a second
solver: every attempt remains one isolated `poecraft_solver_benchmark.exe`
process using the shared corpus-worker adapter, while native code retains all
mechanics, planning, proof, compilation, and exact-evaluation authority.

The delivered surface includes:

- a versioned `native_allflame_no_imprint_v1` profile and five-case frozen
  qualification corpus;
- a WAL-backed SQLite catalog with durable experiments, jobs, immutable
  attempts, commands, events, leases, sessions, and hashed artifacts;
- bounded multi-process supervision with deterministic dispatch, conservative
  host-memory admission, verified cancel/escalation, retry, clone, priority,
  queue pause, and stale-session/orphan recovery;
- a PySide6 GUI with Queue & Run, Compare, Strategy, and Matrix tabs;
- a JSON CLI and local stdio MCP adapter with 21 finite typed tools over the
  same application service;
- controlled investigation bundles and a reusable direct-versus-Lab semantic
  qualification command; and
- stable operator documentation in
  [Native Solver Lab](../../foundation/solver-lab.md).

GUI, CLI, and MCP mutations are dry-runnable and idempotent. Matrix expansion
is sorted case ID × replicate ordinal with explicit include/role/exclude rules.
The UI and artifacts disclose automatic Imprint off, voluntary/economic
Restart off, and native mechanic-owned paid Fracture miss replacement retained.

## Qualification

Gate 6 first found and fixed a profile binding defect: the supervisor had
hardcoded the explicit goal-progress-gating worker override false while the
profile declared true. The native Calculator profile default had nevertheless
resolved the option true in prior reports, so the repair corrected command and
provenance identity without changing native behavior.

Both qualification paths then ran from clean `14ebf42`, one process at a time:

| Path | Cases | Survivors | Wall |
| --- | ---: | ---: | ---: |
| Direct corpus runner | 5 | 0 | 439.722325 s |
| Native Solver Lab | 5 | 0 | 440.980810 s |

The one observed Lab-minus-direct delta was 1.258485 seconds / 0.2862%. This
single sequential observation is instrumentation evidence, not solver
semantics.

All five cases matched on immutable source/executable/artifact/corpus/case/
economy/profile/action/cap/watchdog/measurement identity, runner
classification, final bounds and termination, exact evaluated cost and proof
authority, final states and stable work, policy/transition hashes, strategy
hash, exact success/properness/pricing/reconciliation/off-policy evidence, and
distinct lower/upper/incumbent milestones.

| Case | Result | Certified lower | Evaluated upper |
| --- | --- | ---: | ---: |
| Three prefixes | exact / exact closed | 1618.2138946963837 | 1618.2138946963837 |
| Three suffixes | exact / exact closed | 1101.15648683309 | 1101.15648683309 |
| Four-mod PDR | solver-owned resource cap | 21.772459401271156 | 7866.432124027084 |
| Partial 4-to-5 | requested bounded finish | 36.48853172876641 | 7896721.254200992 |
| Four-goal Bow | state cap | 212.38564294509226 | 223349.0000393144 |

Native trace samples are clock-positioned, so sample counts and intermediate
work positions can differ while bound milestones match. The partial
wall-time-stopped case differed by 17,405 cumulative `outcome_entries`
(0.0051%) while all protected endpoints and stable work matched. Its corpus
contract explicitly says fixed work identity is not required; a separate
direct repeat varied more counters while reaching the same result. The
qualification artifact records the difference instead of hiding it.

Raw ignored evidence is under:

- `build/solver-lab/gate6-direct-14ebf42/`;
- `build/solver-lab/gate6-lab-14ebf42/`; and
- `build/solver-lab/gate6-qualification-14ebf42.json`.

## Simulator Controls

Both exact same-side strategies were rerun with the native
`--verification-runs 10000` override. Each completed 10,000/10,000 successes
with zero failures, stops, action/cost/step-limit events, inapplicable actions,
missing edges, missing prices, or off-policy events. Independent exact
evaluation still matched the solver values with success probability one and
zero off-policy mass.

The earlier runner-level `--run-verification` probe correctly removed the skip
flag but selected zero runs because these frozen cases declare `runs: 0`.
That probe is retained separately; it is not counted as the required control.
Explicit 10,000-run evidence is under ignored
`build/solver-lab/gate7-simulator-explicit-41a316a/`.

## Acceptance

- Native Solver Lab service/catalog/supervisor/GUI/MCP/contracts/parity and
  legacy runner suite: 39 passed in 8.29 seconds.
- Real stdio MCP initialize, tool listing, and bounded `list_cases` call:
  passed.
- Direct-versus-Lab five-case semantic qualification: passed 5/5.
- Explicit Simulator controls: 10,000/10,000 success for each exact strategy.
- Full `powershell -File scripts/test.ps1`: passed once after the selected
  boundary, including ingest, DB/artifact validation, 3,417,290 native checks
  with zero failures, 12 benchmark specifications, Python bindings, 28/28
  release-WASM worker checks, and all web suites.
- `npx tsc --noEmit`: passed.
- Documentation relative-link and root-reachability audit: passed after
  archival.
- `git diff --check`: passed.

No C++, C ABI, WASM, TypeScript, canonical SQLite, or compiled-artifact
behavior change was required by the Lab. The full pipeline regenerated and
validated the derived artifact through the repository-owned path.

## Limitations And Successors

v0 deliberately does not implement running-solve pause/resume, live solver
checkpoint continuation, remote workers, cloud/authentication, full graph
rendering, learned guidance, cap tuning, or another mechanics/evaluation
backend. Closing the GUI stops new dispatch and allows an owned worker to
drain; live work must be canceled explicitly.

The PDR control remains bounded/resource-refused. The completed Lab makes the
following separate future boundaries easier to measure but does not authorize
one automatically:

1. verified executable option/subgoal fragments as shadow incumbent generators;
2. fresh-run PDR proof-memory attribution, with scheduler-aware replay only if
   identical-prefix continuation proves necessary;
3. small action-specific retention/capacity lower-bound patterns with measured
   consumers; or
4. deterministic scheduling baselines and feature logging before learned
   guidance.

Oliver must select the next implementation boundary.
