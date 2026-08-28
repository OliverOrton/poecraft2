# Handoff

**Status: Gates 0–5 of the selected Native Solver Lab Unattended Execution and
Identity Hardening boundary passed; the Gate 6 harness and accelerated suite
passed, and the required six-plus-hour soak is next.** The configured MCP
registration uses the combined version `0.2.0` launcher.

## Checkpoint

- Branch: `main`, local-only; nothing was pushed.
- Gates 0–3 unattended-hardening checkpoint: local commit
  `9fb0e083ebdbce6d0cf1cf720df550f7a6810cca` (`Harden solver lab unattended
  execution identity`), with 72 focused tests passing.
- Gate 4 combined-launch checkpoint: local commit
  `Run solver lab MCP with singleton dispatcher` (the commit containing this
  handoff), with 81 focused tests passing.
- Unattended-hardening stable source base:
  `978b200e8d7993a49ee7991f303cc0823f60914b`.
- Unattended-hardening planning activation: the current HEAD is the single
  documentation-only commit whose parent is the source base and whose subject
  is `Activate solver lab unattended hardening`. The fresh task must record its
  full hash before editing.
- GUI-stabilization selection base: `ee18cc2`.
- GUI-stabilization plan activation: `b7b383e`.
- GUI-stabilization implementation: `60bd13f`.
- Case-authoring selection: `c1dd9d7`.
- Case-authoring implementation: `88d65a9`.
- Native Solver Lab checkpoints through typed LLM controls:
  `d893db9`, `b047f02`, `91d44dd`, `7bf0f9d`, `cde2ccb`, and `1d5350a`.
- Gate 5 practical GUI checkpoint: `a601a3a`.
- Profile-to-worker binding checkpoint: `14ebf42`.
- Gate 6 semantic qualification checkpoint: `5255998`.
- Stable operator docs and stdio MCP integration: `41a316a`.
- Completion result and archive checkpoint: `ea9e0f5`.
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

## Selected Native Solver Lab Unattended Hardening

Gates 0–3 now add complete canonical mutation-request binding, full immutable
dispatch identity revalidation, exact submitted-watchdog enforcement, separate
solver cap/per-worker headroom/global reserve accounting, crash-atomic hashed
terminal publication, integrity-checked terminal reads, valid-final recovery,
and possible-live orphan quarantine. Catalog schema v4 is additive and does not
rewrite legacy rows. The focused contract/service/supervisor/CLI/MCP/nonvisual
GUI/runner/parity suite passed 72 tests; `git diff --check` passed. No native,
mechanic, ABI, generated-data, strategy-vocabulary, WASM, browser, or rendered
GUI change was made.

Gate 4 added catalog schema v5 singleton dispatcher ownership, a combined MCP
plus supervisor entry point, conservative live-legacy-session migration,
catalog-wide reservation admission, typed bounded launch controls, and normal
draining shutdown. Real stdio dispatch, dual server/control-only behavior,
simultaneous ownership race, normal release, and queued/running/finalizing
forced-death recovery passed. The user-local registration now includes
`--with-supervisor --max-workers 1`; no user-specific path is committed.

The existing GUI is still intentionally open with verified-live supervisor
`supervisor-0f222afe-f8f1-46dd-915a-48b22b8d37c0` (PID `46580`). The exact
registered-command probe migrated it as the catalog owner and correctly stayed
control-only. Oliver should normally close that GUI before Gate 5 so its
supervisor drains and releases ownership. If it remains open, the fresh MCP
server must remain control-only and report it; do not bypass or force-clear the
owner.

The fresh Gate 5 task passed the MCP-only revision → submit → live partial →
cancel → verified termination/release → retry ordinal 2 → compare → bundle
workflow without opening the GUI. The accepted job is
`job-4d55bb58-097e-4882-8a01-831a078ef550`; exact attempt, artifact, command,
request, comparison, and bundle identities are retained in the execution log.

Gate 6 now has a deterministic qualification module, 22 passing accelerated
fault cases, and a passing 56.3-second real-native rehearsal. The next exact
action is to start its immutable ignored six-hour soak from the clean harness
checkpoint, using 600-second audits and the retained accelerated result. Do not
edit source during the soak because periodic provenance revalidation must stay
exact.

The fresh implementation task verified clean-tree/source ancestry, confirmed
the configured Solver Lab MCP tools, and used MCP to inventory profiles, cases,
revisions, supervisor state, jobs, and attempts before editing. MCP remains the
operator acceptance surface; normal repository tools remain the only
source-editing and test surface.

The selected scope is limited to complete dispatch-time identity revalidation,
actual immutable-watchdog enforcement, canonical-payload idempotency, explicit
per-worker host headroom, crash-atomic hashed terminal publication, artifact
integrity verification, valid-final recovery, possible-live orphan quarantine,
one combined MCP/supervisor launch path, a fresh-task MCP workflow, and
overnight recovery qualification. Solver mechanics, search order, proof
bounds, verified options, RCASSP, learned guidance, Imprint, native caps, and
WASM behavior remain out of scope.

Gates 0–3 are retained together as the first coherent checkpoint. The exact
startup, tests, state decisions, and continuation are in the
[execution log](docs/active/2026-08-28-native-solver-lab-unattended-hardening/execution-log.md);
the remaining hard stops and retained-state rules remain authoritative in the
[plan](docs/active/2026-08-28-native-solver-lab-unattended-hardening/plan.md).

## Completed Native Solver Lab GUI Stabilization

Explicit-null report sections now normalize to bounded service summaries, and
one malformed attempt cannot abort the job list. Cached aggregation and
selected-detail reads run off Qt; terminal reports are not reopened, refresh is
single-flight, unchanged rows avoid model resets, and selection is stable by
job ID. Every visible action has explicit valid/busy state and persistent
accepted/rejected feedback. Complete tracebacks and identity context append to
the Activity & Errors dock and controlled GUI log.

Cancellation was qualified through direct service, JSON CLI, stdio MCP, and
the real GUI button using actual Windows parent/grandchild process trees. All
four paths moved `running -> canceling -> canceled`, terminated the tree,
released the lease and host reservation, and acknowledged in under 0.5 seconds
using `graceful_then_process_tree_termination`. The reported Cancel problem was
owned by synchronous GUI polling/model reset and silent no-selection handlers,
not by the supervisor lifecycle.

The final focused suite passed 34 tests, the rendered PySide smoke passed, and
the one final full pipeline passed 3,417,290 native checks, all 12 benchmark
specifications, 28/28 release-WASM checks, and every other layer. No native
solver, mechanic, ABI, generated-data, strategy-vocabulary, or WASM change was
made. Stable usage is in [Native Solver Lab](docs/foundation/solver-lab.md),
and complete evidence is in the archived [result](docs/archive/2026-08-28-native-solver-lab-gui-stabilization/result.md).

## Completed Native Solver Lab Case Authoring

The Lab now supports editable local drafts, native `--validate-only`, and
content-addressed immutable revisions. Submitted jobs record the revision ID,
canonical case digest, immutable case/corpus paths, profile identity, and
effective native command, so later draft edits cannot alter queued or completed
work. Frozen fixtures remain read-only, and deleting a draft retains saved
revisions.

The PySide6 GUI now has a fifth **Cases** surface. Calculator owns graphical
item/goal authoring and can **Copy Lab case**; the Lab imports that envelope,
allows JSON plus common watchdog/bounded-finish/memory edits, validates, saves,
exports, and submits revisions. The bridge preserves concrete affixes,
crafted/fractured/veiled flags, influence/Eldritch state, the product goal,
diagnostic family exclusions, and pinned Allflame prices. It refuses active
Imprint checkpoints and special item flags that the native benchmark start
format cannot preserve.

CLI and MCP expose the same lifecycle. The closed adapter now has 31 typed
tools and still has no arbitrary shell, SQL, path-write, mechanic override, or
native argument-bag authority. The installed server is registered in the
user-local Codex configuration as `poecraft2-native-solver-lab`, rooted at this
checkout; a newly started task or Codex restart is required to load it.

Acceptance passed 26 focused Lab tests, the complete web suite and TypeScript,
a real native case validation/revision/submit probe, and the installed MCP
stdio handshake. The final full repository pipeline passed 3,417,290 native
checks, all 12 solver benchmark specifications, 28/28 release-WASM worker
checks, and all remaining layers. The first full-pipeline attempt hit a
timing-only existing WASM finalization-cancel assertion; that exact test and
the complete pipeline both passed on rerun. No solver, mechanic, ABI, strategy
vocabulary, compiled data, or WASM module changed.

Stable usage is in [Native Solver Lab](docs/foundation/solver-lab.md), and the
exact outcome is in the archived [result](docs/archive/2026-08-28-native-solver-lab-case-authoring/result.md).

## Completed Native Solver Lab v0

Native Solver Lab v0 extends the existing `solver_corpus_runner.py` and
`poecraft_solver_benchmark`; it does not create another solver backend. GUI,
CLI, and MCP share one typed application service. One isolated OS process owns
each solve, attempts and artifacts are immutable, the catalog is persistent,
and partial/crash/watchdog/resource outcomes remain distinct.

The original v0 Gates 0-7 provided persistent immutable attempts,
resource-aware native supervision, JSON and a closed 21-tool MCP adapter,
investigation bundles, and
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

Both exact same-side strategies completed explicit 10,000-run Simulator
controls with 10,000/10,000 success and no failure, stop, limit, inapplicable-
action, missing-edge/price, or off-policy event. The final Lab/runner suite
passed 39 tests, stdio MCP integration passed, TypeScript type-check passed,
and the one selected full repository pipeline passed 3,417,290 native checks,
all 12 benchmark specifications, 28/28 release-WASM worker checks, and all
remaining layers.

Stable usage, recovery, shutdown, profile, GUI, CLI, MCP, artifact, and parity
documentation is in
[Native Solver Lab](docs/foundation/solver-lab.md). No native solver,
mechanics, ABI, WASM, or product behavior changed in this boundary.

The locked research profile is `native_allflame_no_imprint_v1`: fixed resolved
Allflame economy, Calculator product profile, automatic Imprints off,
voluntary economic Restart off, mechanic-owned paid Fracture miss replacement
retained, goal-progress gating on, and exact junk-free success.

Explicit non-goals include scheduler-aware replay, PDR memory repair, option
behavior, RCASSP, learned guidance, solver ordering changes, release-WASM
redesign, and remote/cloud execution. The supporting read-only reports are
archived under
[Solver Research Architecture Audits](docs/archive/2026-08-27-solver-research-audits/README.md).

## Candidate Successors — Not Selected

The completed research audits and Lab result preserve four independent tracks:

1. verified executable option/subgoal fragments as shadow incumbent generators;
2. fresh-run PDR proof-memory attribution, adding scheduler-aware replay only
   if identical-prefix continuation proves materially useful;
3. small action-specific retention/capacity proof patterns with measured
   lower-bound consumers; and
4. deterministic scheduling baselines and feature logging before learned
   guidance.

These are choices for Oliver, not an active sequence.

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
