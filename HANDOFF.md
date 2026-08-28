# Handoff

**Status: active implementation boundary.** Oliver approved
[Native Solver Lab v0](docs/active/2026-08-27-native-solver-lab-v0/plan.md)
from clean checkpoint `bd86b46` on 2026-08-27. Gates 0-1 are complete; Gate 2
is next.

## Checkpoint

- Branch: `main`, local-only; nothing was pushed.
- Active-boundary selection commit: `25737c3` (`Activate PDR strict proof
  memory boundary`).
- Native checkpoint/replay implementation: `f15f590` (`Add native solver
  graph checkpoint replay`).
- Final cache ownership: `952524b` (`Keep replay evidence outside coroutine
  state`).
- Release-WASM compiler workaround/rebuild: `5f49b4e` (`Stabilize and rebuild
  release WASM`).
- Calculator product scope remains `calculator_product_v1`: generated
  automatic Imprint programs off, voluntary economic Restart off,
  goal-progress-gated reforges on, junk-free exact terminal success, and no
  disabled action families unless a diagnostic control is selected.

## Proposed Next Boundary

Native Solver Lab v0 extends the existing `solver_corpus_runner.py` and
`poecraft_solver_benchmark`; it does not create another solver backend. GUI,
CLI, and MCP share one typed application service. One isolated OS process owns
each solve, attempts and artifacts are immutable, the catalog is persistent,
and partial/crash/watchdog/resource outcomes remain distinct.

The first usable checkpoint is Gate 2: a persistent single-worker vertical
slice that submits and monitors a small native case through both the GUI and
JSON operations. Later gates add robust resource-aware supervision, LLM tools,
comparison, strategy summary, matrices, and final current-semantic parity.

The locked research profile is `native_allflame_no_imprint_v1`: fixed resolved
Allflame economy, Calculator product profile, automatic Imprints off,
voluntary economic Restart off, mechanic-owned paid Fracture miss replacement
retained, goal-progress gating on, and exact junk-free success.

Explicit non-goals include scheduler-aware replay, PDR memory repair, option
behavior, RCASSP, learned guidance, solver ordering changes, release-WASM
redesign, and remote/cloud execution. The supporting read-only reports are
archived under
[Solver Research Architecture Audits](docs/archive/2026-08-27-solver-research-audits/README.md).

## Stopped PDR Replay Result

The current coarse-graph checkpoint cannot faithfully resume
`conquest-lamellar-allflame-clean-4-pdr-product8`. The last prepared Bellman
graph contained `1207` states, while delayed incremental generation had grown
the calculator to `7242` carrier states and `61476` rows. That additional
scheduler state is behavior-bearing and is not part of the coarse checkpoint
contract.

A stable-prefix replay with the complete dynamic vocabulary priced all `310`
scanned actions, but published upper `9844.962286897467` and stopped on
`numerical_stability` with `18451` unresolved obligations. The matched
ordinary diagnostic published upper `7866.432124027084` and stopped on memory.
A broader calculator-closure replay also solved a different graph. The
mismatch fired Gate 1's explicit stop condition before memory attribution.

All experimental source edits were removed. No solver behavior, ABI, fixture,
or release-WASM change was retained. The authoritative 1 GiB PDR boundary is
unchanged: bounded upper `7866.432124027084`, certified lower
`21.772459401271156`, `3507568` strict reforge work, `846846750` retained
proof/quotient bytes, and `max_solver_owned_bytes` at a native peak of
`1179431999` bytes.

## Prior PDR Successor Requirement

Do not use the existing coarse replay as a live PDR scheduler snapshot. If a
future PDR boundary requires identical-prefix continuation, it must implement
either:

1. a scheduler-aware checkpoint that atomically preserves incremental carrier
   order/generations/cursors, delayed rows and status, focused/support
   frontiers, restricted values/incumbent/properness, complete action-ledger
   scheduling counters, and graph generations; or
2. a first-strict-partition checkpoint that additionally preserves the
   persistent oracle, partition/dependency generations, obligations, kernels,
   cursors, and incumbent.

That replay boundary's first acceptance gate is an ordinary/save/replay
triplet on the fixed 1 GiB PDR witness with identical request/action scope,
upper/evaluation, lower, strict frontier/work, open obligations, and resource
stop. Only after that parity may replay evidence be used for retained
proof/quotient attribution. Fresh ordinary-run behavior-neutral memory
telemetry remains a separately valid future measurement route.

## Stable Baseline

The completed native checkpoint milestone remains valid for completed coarse
graphs and its dynamic Eldritch control. It passed the full repository pipeline
and release-WASM acceptance before this stopped probe. The probe did not rerun
the full pipeline, broad benchmarks, or browser review because it retained no
source change.
