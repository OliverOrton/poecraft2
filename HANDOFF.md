# Handoff

**Status: active implementation boundary.** Oliver approved
[Native Solver Lab v0](docs/active/2026-08-27-native-solver-lab-v0/plan.md)
from clean checkpoint `bd86b46` on 2026-08-27. Gates 0-6 are complete; Gate 7
is next.

## Checkpoint

- Branch: `main`, local-only; nothing was pushed.
- Native Solver Lab checkpoints through typed LLM controls:
  `d893db9`, `b047f02`, `91d44dd`, `7bf0f9d`, `cde2ccb`, and `1d5350a`.
- Gate 5 practical GUI checkpoint: `a601a3a`.
- Profile-to-worker binding checkpoint: `14ebf42`.
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

Gates 0-5 now provide persistent immutable attempts, resource-aware native
supervision, JSON and a closed 21-tool MCP adapter, investigation bundles, and
Queue & Run / Compare / Strategy / Matrix GUI surfaces. Matrix expansion is
canonical and idempotent unless the explicit new-batch action is used. Oliver
still owns rendered visual/usability review.

Gate 6 behavior-neutral qualification passed all five frozen cases from clean
`14ebf42`. Direct and Lab requests, runner classifications, endpoint semantics,
strategy hashes, exact-evaluation results, and distinct bound milestones all
matched. Direct wall was 439.722325 seconds; Lab wall was 440.980810 seconds,
a single observed delta of 1.258485 seconds / 0.2862%.

Qualification fixed one provenance defect first: the supervisor had hardcoded
the explicit goal-progress-gating worker override false despite the profile
declaring true. The native Calculator profile default had still resolved the
prior solve option true, so prior native behavior was not changed. The Lab now
derives exact evaluation, Simulator verification, and gating from the typed
profile and records an explicit matching command.

The partial wall-time-stopped case disclosed a 0.0051% `outcome_entries`
difference while all protected endpoints and stable work counters matched.
This is retained as non-fixed-work observation evidence; the corpus explicitly
sets `fixed_work_identity_required: false`, and direct repeats varied more.

The next boundary is Gate 7 only: run final acceptance once, including the
required 10,000-run Simulator control, finish operator/MCP/recovery docs,
archive the plan/result, and update this handoff. No native source changed in
the Lab boundary, so no native rebuild is required solely for Gates 0-6.

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
