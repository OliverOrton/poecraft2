# Generated Planner-Envelope Qualification And Ladder-Service Repair v1

**Status: selected and active.** Selected by Oliver on 2026-08-28 from source
checkpoint `28c4c30c75cef6ff8e1b38fa218f9b5a98d70203` after a controlled Native
Solver Lab investigation and independent review of its evidence.

Parent: [Active work](../README.md)

Progress and exact evidence belong in the [execution log](execution-log.md).

## Objective

Qualify the complete generated planner/action envelope and restore useful,
bounded service to the existing exact-goal carrier ladder without silently
restricting the problem. The selected subject is not the permanent action
registry. It is the generated `FixedOptionSpec` and `PlannerOperator`
vocabulary, its primitive dependencies, canonicalization, carrier-local
admission, row construction, cooperative scheduling, and joint-policy
consumption.

The boundary first repairs two correctness defects exposed by the starting
investigation: the falsely named Solver Lab disabled-family identity component
and the native access violation at a 256-state discovery cap. It then adds
bounded operator-lineage and phase-owner evidence, separates primitive cost
from generated-automatic cost, and permits a behavior change only after a
specific exact owner is measured.

The completed Verified Executable Graph-Fragment Core v1 remains parked and
unchanged. This boundary neither expands fragments nor adopts another planner.

## Starting Evidence

The source and diagnostic starting point is recorded in the execution log.
The selected diagnosis is:

- the permanent registry remained 186 actions in the historical and later
  five-T1 evidence;
- generated automatic planner operators grew from 242 to 370 while carrier
  ladder service fell from 35 epochs and 615 goal subsets to three epochs and
  63 goal subsets;
- current-HEAD unrestricted, no-Temporary-Bench, and no-Metamod five-goal
  controls all exceeded a 180-second host watchdog without a terminal native
  report;
- 256-state control, no-Temporary-Bench, no-Metamod, and no-full-evidence
  controls exceeded 120 seconds;
- a normal-cap currency-only control exactly closed in 36.90 seconds with nine
  supported actions, zero automatic operators, and 603 expanded states;
- a 256-state currency-only control reached exactly 256 discovered and zero
  expanded states, then terminated with Windows access violation
  `0xC0000005`; and
- Solver Lab hashes `action_scope.explicit` under the component name
  `disabled_families`, so the component is unrelated to the native goal's
  effective disabled-family list.

These observations localize the problem to broad envelope execution economics
and expose two definite bugs. They do not prove that the additional generated
operators are duplicates, unnecessary, or mechanically invalid.

## Dirty-Tree Exception

The selected source checkpoint is on branch `main` synchronized with
`origin/main`, but the worktree contains one untracked file named `0`. It is
three bytes and contains the line `0`. The file appeared during the diagnostic
window after the initial clean check.

This plan treats `0` as preserved external/user state:

- do not delete, clean, restore, stage, edit, rename, or commit it;
- exclude it explicitly from every commit and status assertion;
- do not claim the whole worktree is clean while it exists; and
- stop if any other unexpected dirty path appears.

Oliver may remove it manually later if he confirms it is accidental. Its
presence is not permission to ignore any other dirty state.

## Locked Boundaries

- Preserve exact engine-owned mechanics, transition probabilities, legality,
  costs, and terminal-goal semantics.
- Preserve the complete required action envelope. A faster restricted-family
  solve is attribution evidence, not product qualification.
- Do not target an arbitrary reduction from 370 operators to 242.
- Every required operator must remain exactly represented as canonical,
  state-locally inapplicable, admitted, scheduled, completed, interrupted, or
  explicitly open in the action-envelope ledger.
- Do not prune by label similarity, carrier projection, historical selection,
  arbitrary top-k, depth, time, or count caps.
- Exact cost dominance is valid only when legality, complete physical and
  choice kernels, hard execution state, exit behavior, and continuation
  semantics are equal under the pinned economy.
- Do not inject, seed, or replay the historical 87k strategy as an incumbent.
- Preserve and park the archived fragment implementation and evidence. Do not
  add fragment integration, meta-policy search, RCASSP, learned guidance, or a
  new planner.
- Preserve native solver authority. The Solver Lab remains an additive typed
  control plane around one native benchmark process; it gains no mechanic,
  probability, arbitrary shell, SQL, or path-write authority.
- Keep the unattended-hardening qualification wording exact: its six-hour soak
  was owner-waived, not passed, and is not overnight qualification.

## Gate 0 — Maintenance And Active Boundary

Before solver diagnosis:

1. Activate this plan in `docs/README.md`, `docs/active/README.md`, and
   `HANDOFF.md`; record the exact starting state and diagnostic identities.
2. Remove the hardcoded Visual Studio 2022 CMake/Ninja dependency from the
   portable CMake preset. The PowerShell build owner must discover tools from
   `PATH`, explicit task-specific environment overrides, or bounded local
   fallback paths and pass resolved paths to CMake.
3. Provision a reproducible UCRT64 GCC/CMake/Ninja toolchain in hosted Windows
   CI so the workflow can configure and build instead of falling through or
   binding to a developer-specific Visual Studio installation.
4. Correct stale handoff wording that says the completed checkpoint was not
   pushed when `main` is synchronized with `origin/main`.
5. Preserve the archived fragment boundary byte-for-byte except for links from
   current indexes. Preserve the untracked `0` file as described above.
6. Run proportional build-script/configuration checks. Hosted success may be
   claimed only from an actual hosted run after push; local-only work records
   the workflow/configuration evidence without inventing a hosted result.

Gate 0 changes no solver behavior.

## Gate 1 — Identity And Low-Cap Correctness

### Effective family identity

Replace the false `disabled_families` component with distinct, truthfully
named canonical identity components:

- `explicit_imprint_scope`;
- `effective_disabled_action_families`;
- `allowed_mechanic_families`;
- `product_action_envelope`; and
- `goal_action_list`.

The effective family component must hash validated canonical native family
names, independent of input order. Preserve complete-case and complete-goal
identity. Add service/CLI/MCP comparison regressions for:

- unrestricted control;
- `temporary_bench` disabled;
- `metamod` disabled; and
- currency-only.

They must have distinct effective-disabled-family and appropriate
action-vocabulary/core/full identities, stable replay identities, and an
unchanged Imprint-scope identity when Imprint settings are unchanged.

### Native low-cap finalization

Reproduce and repair the currency-only `0xC0000005` without changing the
requested envelope. Sweep `max_discovered_states` values 1, 2, 255, 256, 257,
and 512, covering:

- a cap before the first complete row;
- zero expanded states;
- one completed row;
- no executable policy;
- an existing executable fallback; and
- full evidence enabled and disabled.

Every outcome must be a valid named resource-cap or another documented safe
terminal result. No access violation, invalid strategy publication, fabricated
closure, or unclassified termination is permitted.

No action-envelope behavior change belongs in Gate 1.

## Gate 2 — Operator Lineage And Phase Ownership

Add a stable diagnostic lineage for every generated candidate:

```text
permanent registry action
  -> primitive candidate or dependency
  -> generated FixedOptionSpec
  -> pre-canonical candidate variant
  -> canonical effect/template class
  -> PlannerOperator
  -> priced/supported operator
  -> state-local applicability
  -> admitted carrier/operator pair
  -> scheduled row
  -> begun/completed/interrupted row
  -> joint-policy consumption
```

Reuse existing Temporary-Bench, automatic-candidate, ledger, scheduler, and
row telemetry owners. Do not build a parallel accounting system.

For each solver action family and native `AutomaticCandidateKind`, report
bounded totals for generated variants, canonical groups, collapsed variants,
planner operators, dependencies, carrier-local checks/admissions, retained
bytes, synthesis work/time, row starts/completions/interruptions, scheduler
offers/services/waits, carrier epochs, joint-policy attempts, and complete
missing-frontier lifecycle.

Long operations must publish bounded phase-owner evidence for setup, planner
construction, temporary-effect precompile, primitive rows, state-local
automatic synthesis, dependency preparation, ladder scheduling, policy
assembly, compilation, and exact evaluation. Phase evidence must not claim a
solve step or row completed when it did not.

Telemetry must remain capped, deterministic for fixed work, and excluded from
mechanic or solve authority.

## Gate 3 — Two-Layer Add-Back Attribution

Run every matrix through immutable Native Solver Lab revisions under the
versioned profile and normal state caps after Gate 1 passes.

### Primitive envelope

Starting from the successful currency-only control, add one primitive family
at a time while suppressing generated automatic candidates through a validated
benchmark-private diagnostic control. This separates raw primitive row cost
from generated work.

### Generated envelope

Using a fixed primitive envelope, add native automatic candidate kinds or
dependency bundles incrementally. Enumerate the actual current native enum;
the expected set includes permanent goal Bench, constructive renewal,
Temporary Bench blocker, Cannot Roll, protected Metamod, Eldritch side intent,
Fracture preparation/recovery, Veiled work, Harvest-related work, and any other
implemented kind.

Run:

- each mechanically meaningful generated class alone;
- cumulative add-back in a recorded order; and
- pairwise interactions for classes that finish separately but stall jointly.

Compare deterministic work as well as wall time. For each run record complete
request/core/component identities, case revision, job, attempt, termination,
phase owner, action/operator counts, lineage, ladder service, bounds, memory,
and artifact identities.

Reduced state caps remain safety regressions, not performance substitutes.

## Gate 4 — Narrow Qualified Repair

Retain a solver behavior change only when Gate 3 identifies one measured
owner. Qualified repair classes are limited to:

- exact semantic interning of proved duplicate generated operators;
- exact dominance removal under the full locked equivalence contract;
- reuse of an already identical generated template or dependency result;
- avoiding repeated canonicalization/preparation whose identity is unchanged;
- carrier-local admission that avoids irrelevant global scans while retaining
  the open obligation;
- cooperative yielding inside a measured long automatic-synthesis or row
  operation;
- preservation of missing-frontier priority through every scheduling stage;
  or
- bounded fair ladder service when automatic preparation is proved to starve
  it.

Every retained repair needs a failing regression, a precise authority proof,
before/after lineage and deterministic-work evidence, and a demonstration that
no required operator vanished from the ledger. Revert speculative changes that
do not qualify. Diagnosis-only completion is valid.

## Gate 5 — Ladder Requalification

Use fresh current-source immutable runs for:

- clean five-T1 Conquest Lamellar;
- partial four-to-five;
- a non-armour four-goal case;
- same-side exact controls; and
- PDR as a secondary safety/control case.

Measure time and deterministic work to the first carrier epoch, first completed
useful carrier row, every joint-policy assembly, first proper independently
verified upper, and selected upper milestones. Also compare epochs, unique goal
subsets, row offers/admissions/completions, completed rows per expanded state,
automatic generation work, missing-frontier completion, joint-policy attempts,
failure reasons, bounds, memory, and terminal classification.

The historical 35 epochs, 615 subsets, and 87,361 upper are reference evidence,
not literal pass thresholds and not incumbent authority. Mechanics and the
complete required envelope may differ.

If a fresh proper policy is published or behaviorally changed, compile it and
independently exact-evaluate it. Run 10,000 Simulator trials when the final
acceptance requires compiled-strategy verification.

## Gate 6 — Acceptance And Handoff

Run the appropriate complete acceptance once after the final retained change:

- Python Solver Lab service/CLI/MCP tests;
- fresh native build and the affected complete solver/API/compile/eval suites;
- benchmark manifest validation;
- release WASM rebuild and web tests/typecheck if browser-visible native
  behavior or telemetry changed;
- full `powershell -File scripts/test.ps1` when the retained scope crosses the
  repository pipeline;
- `git diff --check`; and
- a one-off documentation link/reachability audit.

Use MCP for final immutable case submission, monitoring, comparison, and
investigation-bundle export. Do not substitute ad hoc catalog SQL or repository
CLI execution for operator-level Lab evidence.

Archive the completed plan only after durable identity, cap, telemetry, and
solver contracts are extracted into stable documentation.

## Valid Completion Outcomes

### Diagnosis-only completion

Passes when both definite bugs are fixed, operator lineage is complete, the
matrix identifies the current generation/admission/row/scheduling/continuation
owner, no sound behavior repair qualifies, and every speculative change has
been removed.

### Repair completion

Additionally requires a narrowly proved repair, improved ladder service
attributable to it, a freshly generated proper independently exact-evaluated
upper improvement at matched work or horizon, no material partial/non-armour/
same-side regression, no silent operator removal, and no historical strategy
injection.

Only after this boundary establishes that operators are correctly canonical,
admitted, and serviced—and that global continuation coverage still dominates—
may online continuation completion through verified fragments be reconsidered.

