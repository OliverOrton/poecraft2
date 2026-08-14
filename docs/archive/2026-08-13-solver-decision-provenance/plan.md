# Solver Decision Provenance And Result-Truth Hardening

**Status: complete and archived on 2026-08-14.** Oliver selected this audited
replacement on 2026-08-13 in hardening-only mode. The unavailable historical
Fossil-to-Chaos request may not be claimed as localized; the controlled current
Calculator request-scope witness is the Gate 0 authority.

Parent: [Active work](../README.md)

Frozen source baseline:
`9914b84f2c075e84d932936a14fa0d2ac5f03156` on
`codex/solver-goal-realignment`.

The plan was activated from the reviewed deferred proposal without changing
the source baseline. `HANDOFF.md` owns the exact live gate and must remain
current at every checkpoint.

## Why This Replaces The Attached Proposal

The attachment was written for a branch still at `8b67d1a`. That premise is
obsolete. Goal Realignment Gates 0 through 8 are complete and archived in the
[milestone report](../../archive/2026-08-09-solver-goal-realignment/report.md) and
[Gate 8 acceptance record](../../archive/2026-08-09-solver-goal-realignment/evidence/gate8-final-acceptance.md).
In particular:

- the primary closes exactly at `3745.7309340083884` chaos in native and
  release WASM inside the five-minute boundary;
- Warlord, Eldritch, Imprint, and the primary compile and pass their selected
  10,000-run verification;
- all 979 ordinary bases pass the lightweight matrix; and
- the one native/release-WASM 49-case portfolio publishes all policies and
  passes 4,072 semantic comparisons.

Those results are frozen evidence, not recovery work to repeat. The replacement
therefore removes the attachment's R0 recovery and carried Gates 5 through 12,
does not reopen the price-bounded finite Imprint-program proof, and does not
assume that an old reported Fossil-to-Chaos transition still reproduces.

The source audit does support a smaller hardening milestone:

| Classification | Current finding |
| --- | --- |
| Source-confirmed | Sparse policy preference in `solver_sparse_policy.hpp` uses an epsilon when comparing finite action costs. A genuine smaller finite cost can therefore lose to stable order. |
| Source-confirmed | `solver_solve_incremental.cpp` owns an independent sparse-row Q calculation and uses local numerical cutoffs. This is a duplicated decision authority even if current fixtures agree. |
| Contract-confirmed, not yet a defect | `successful_refined_publication_termination()` can produce `termination = ExactClosed` with `policy_status = BoundedFeasible`. Existing native and web tests intentionally support that pair: it means coarse discovery closed while exact refinement produced only a bounded executable policy. `stop_cause` and cap masks separately preserve resource stops. The narrower audit is whether every synthesized `ExactClosed` from coarse `None`/`NoExecutablePolicy` has evidence that coarse discovery really closed. |
| Source-confirmed | Calculator selected-odds construction reaches Solve one step before the final request: `solverGoal(undefined, true)` injects a selected Fossil into the envelope request, `solverActions()` turns that envelope into the priced candidate IDs, and those IDs scope the final Solve goal. Testing only a final goal built from fixed candidate IDs could miss the leak. |
| Already protected | `certified_global_lower_bound()` already returns zero while the incremental envelope is open and can retain a positive certified lower after closure. A blanket rule that every bounded lower is zero would discard valid proof. |
| Partly present | Incremental telemetry already exposes fallback sources and restart/chaos completion. It does not yet provide a durable per-choice distinction between construction history and final proof authority. |
| Unconfirmed report | No exact current serialized item, goal, economy snapshot, selected-odds action, and solver options were supplied for the reported Fossil-to-Chaos transition. Its current cause and even its reproducibility remain unknown. |

## Objective

Make the action chosen by Solve explainable and make every public exact,
bounded, lower-bound, and termination claim mean one thing across native,
WASM, and the web application.

The milestone must:

1. reproduce and localize the reported mixed-action transition if the exact
   current request is available;
2. distinguish how a policy choice was constructed from the authority that
   finally selected or certified it;
3. use one authoritative Q evaluator for policy decisions;
4. make minimum-cost ordering strict for finite values while retaining
   tolerances only where numerical convergence or proof reconciliation needs
   them;
5. verify and preserve the existing separation of search termination, precise
   stop cause, and publication status, repairing only unsupported mappings;
6. isolate Calculator odds queries from ordinary Solve action scope; and
7. expose whether a lower bound is globally certified without weakening the
   existing open-envelope fail-safe.

This is a truth and decision-integrity plan. It is not a new solver
architecture or a performance project.

## Non-Goals

- Do not rerun or rewrite completed Goal Realignment Gates 0 through 8.
- Do not reopen Imprint closure, Warlord recovery, action-family coverage, or
  the accepted compiler and evaluator work without a new failing witness.
- Do not change Path of Exile mechanic rules. Any ambiguity goes to Oliver.
- Do not add a new action family, scheduler, quotient, state abstraction, or
  search cap.
- Do not raise caps to conceal a result or timing regression.
- Do not force every bounded lower to zero. Preserve certified positive
  closed-envelope lowers.
- Do not categorically exclude a choice merely because it originated in a
  fallback constructor. A later complete Bellman or strict proof may validly
  reselect it.
- Do not make detailed provenance a permanent public ABI or UI feature unless
  compact diagnostic evidence proves that necessary.
- Do not treat `termination = ExactClosed` plus
  `policy_status = BoundedFeasible` as inherently contradictory. It is an
  existing documented coarse-closure/bounded-refinement result.
- Do not perform rendered or visual UI review; Oliver owns it.
- Do not claim to have localized the Fossil-to-Chaos report from a synthetic
  substitute request.

## Evidence Vocabulary

Every gate record must label important claims with one of:

- **source-confirmed** - the behavior is directly present in the selected
  baseline source;
- **reproduced-current** - a frozen current binary and request demonstrate it;
- **archived-qualified** - accepted prior evidence still applies under its
  recorded inputs;
- **not-reproduced** - the exact current request was run and did not show it;
- **hypothesis** - a possible explanation that has not yet been isolated.

Do not promote an archived observation or source suspicion into a current
runtime defect without a current witness.

## Required Reproduction Input

The preferred path requires the exact suspicious product request, including:

- start item and base/item level;
- complete goal and any selected modifier IDs;
- league/economy snapshot identity and all manual overrides;
- the Calculator action whose odds were selected;
- `requested_fossil_actions`, if present;
- solver mode, caps, timeouts, and other non-default options;
- the returned result and compiled strategy, if either was saved; and
- the first policy state where Fossil was expected and Chaos or Restart was
  observed.

At activation Oliver chooses one of two modes:

1. **Localization required (recommended when the request exists).** Gate 0
   must reproduce or precisely classify that request before behavior repair.
2. **Hardening only.** The source-confirmed integrity work may proceed without
   the request, but the milestone must say the mixed-action report was not
   localized and must not present a speculative repair as its cause.

## Core Invariants

### Construction history and selection authority are different

A policy choice needs two independent concepts. Exact names may follow current
types, but the semantics must be equivalent to:

- **construction origin:** Bellman seed, incremental overlay, retained
  incumbent, new-state Restart completion, constructive witness, direct-root
  overlay, strict-generated choice, or unknown; and
- **selection authority:** closed Bellman comparison, strict exact
  reoptimization, local incumbent improvement, verified executable upper, or
  unknown.

Construction origin answers where the candidate came from. Selection
authority answers why the final choice is permitted to carry its published
status. An exact policy may retain a fallback-origin action only if every
reachable final choice is subsequently selected or revalidated by complete
exact authority.

### Strict cost order is not convergence tolerance

For two finite expected costs, any smaller representable `double` wins. Stable
action identity breaks only exact equality. The canonical strict argmin and the
decision to replace a policy during a numerically solved iteration are separate
operations: the former defines the objective and published choice, while the
latter may use a named convergence tolerance to prevent near-equal numerical
cycling. Epsilon comparisons may also remain for probability mass
reconciliation, interval closure, or explicit proof tolerances, but they may
not silently redefine the optimization objective. Exact publication must
reconcile any tolerance-suppressed iterative choice with the final canonical
strict winner; otherwise the result remains bounded.

### Search stop and publication status are orthogonal

The existing contract intentionally permits coarse discovery to close while
exact state refinement publishes only a bounded feasible policy. Therefore
`termination = ExactClosed` does not by itself mean `policy_status = Exact`.
The solver must preserve at least:

- the legacy/coarse termination classification;
- the precise stop cause and every cap mask;
- whether a proper executable policy was published; and
- whether its value is globally exact or only a feasible bounded upper.

Recovering a verified policy after a cap stop may change "no executable
policy" into a useful bounded result, but may not erase the cap in the precise
stop cause. Global policy exactness still requires the lower to close to the
proper evaluated policy value under the exact proof contract. When a helper
synthesizes coarse `ExactClosed` from `None` or `NoExecutablePolicy`, the plan
must prove that broad/coarse discovery actually completed; otherwise that
mapping, not the legal bounded/exact-closed pair, is the defect.

### Bounds need proof provenance

A public lower is globally certified only when its producing envelope is
globally valid for the reported search state. Open incremental work keeps the
existing zero fail-safe. A positive lower from a closed valid envelope remains
legal and must carry explicit provenance rather than being zeroed because the
upper policy is bounded.

### UI inspection is not solver authority

Selecting an action to inspect its odds must not silently restrict or widen
the next ordinary Solve request. A restricted-action solve is allowed only as
an explicit mode with an explicit request contract and presentation.

## Gate 0 - Selection, Freeze, And Current Reproduction

**Status: complete.** Hardening-only mode freezes `9914b84`; the historical
request is unavailable. A controlled current odds toggle reproduced the
Calculator envelope leak without claiming the historical policy symptom.
Evidence: [Gate 0/1 Calculator request scope](evidence/gate0-gate1-calculator-request-scope.md).

On activation:

1. freeze the actual source commit, native executable, release WASM module,
   compiled-data identity, economy identity, and request identity;
2. link the completed Gate 8 evidence rather than re-running its acceptance;
3. capture separately, before and after Calculator odds selection, the ordinary
   odds handle goal, the product envelope goal, `solverActions()` result, priced
   candidate IDs, and final Solve goal;
4. run a controlled odds-toggle witness even when the original report is
   unavailable: keep item, goal, economy, and options fixed, switch only the
   inspected action between a Fossil and a non-Fossil, and classify every
   changed envelope/candidate/final-request field. This proves or rejects the
   source-level request leak but does not by itself localize the old reported
   policy transition;
5. when the exact suspicious request exists, run it once natively, compile and
   exact-evaluate any published policy, and capture existing upper-policy,
   publication-candidate, fallback-source, obligation, and completeness
   telemetry;
6. identify the first state where the expected Fossil differs from the
   published action and record the start-state/action Q values available from
   current telemetry; and
7. classify the report as reproduced-current, not-reproduced, or unavailable.

Do not create an "equivalent" mixed-action case and call it the original
report. A small deterministic oracle may be added later for a source invariant,
but it is not reproduction evidence.

Gate 0 is accepted when the mode and frozen identities are recorded, the
request-scope comparison is complete, and the report's evidence classification
is honest. Under localization-required mode, unavailable input pauses the plan
for Oliver; under hardening-only mode it records the limitation and continues.

## Gate 1 - Calculator Envelope And Solve Isolation

**Status: complete.** The odds, product-envelope, and scoped-Solve goal
contracts are separate. The whole-flow regression fails on the frozen behavior
and passes after the correction while selected-Fossil odds remain materialized.
Evidence: [Gate 0/1 Calculator request scope](evidence/gate0-gate1-calculator-request-scope.md).

Correct the source-confirmed request leak before adding solver-hot-path
instrumentation:

1. split the goal builder for exact selected-action odds from the builder for
   the product action envelope;
2. retain selected-Fossil materialization on the odds handle so a Fossil
   outside the automatic beam can still show exact odds;
3. make the product envelope independent of the currently inspected action;
4. derive priced candidate IDs only from that independent envelope; and
5. admit `requested_fossil_actions` to a Solve request only through a distinct,
   explicit restricted-action mode.

The regression must exercise the whole two-stage product flow. With item,
goal, economy, and solver options fixed, toggle only the inspected odds action
and assert that:

- the odds request may change as designed;
- the product envelope request does not change;
- the envelope's returned action IDs and priced candidate IDs do not change;
- the final serialized Solve goal does not change; and
- the result, admitted-action telemetry, value, policy, and compiled strategy
  do not change.

A focused request-builder test with preselected fixed candidate IDs is
insufficient. The pre-fix witness must use or simulate a Fossil that the
automatic envelope omits unless it is explicitly requested, so an already
admitted Fossil cannot make the regression pass accidentally. A controlled
failure before the fix establishes this product
request-scope defect even without the original report, but the milestone still
must not claim it caused the reported Fossil-to-Chaos policy without that
request or an exact reproduction.

## Gate 2 - Decision Provenance And Independent Truth Corrections

**Status: complete.** Existing bounded-incumbent and publication telemetry is
sufficient in hardening-only mode, so 2A added no hot-path construction-origin
trace. Shared Q authority, strict objective/stable iteration separation,
explicit coarse-closure preconditions, and lower-bound proof provenance are
implemented and covered by focused native/API tests. Evidence:
[Gate 2 decision and result truth](evidence/gate2-decision-and-result-truth.md).

These corrections have independent tests and commits so one cannot hide a
failure in another.

### 2A - Minimal decision-provenance instrumentation

After Gate 1, determine whether the request fix plus existing
bounded-incumbent choice sources and full diagnostic telemetry answer the
remaining Gate 0 question. If they do, skip instrumentation changes and record
the existing fields used.

Only if evidence is insufficient:

1. add the smallest internal representation of construction origin and final
   selection authority to bounded incumbents and policy choices;
2. propagate it through incumbent capture, retention, overlays, strict mapping,
   final publication, and any compiler mapping that can change the selected
   choice;
3. emit compact counts and the first conflicting choice in existing diagnostic
   output;
4. permit a byte-capped per-state trace only under an explicit full-diagnostic
   option; and
5. keep the new detail out of stable public product output unless a later gate
   proves a compact user-facing contract is necessary.

Instrumentation must be behavior-neutral. Against each deterministic Gate 0
control, values, bounds, termination, deterministic work, policy hash,
compiled strategy hash, and selected actions must match the uninstrumented
baseline exactly. Repeat runs must emit identical compact provenance. Gate 0
must also freeze a default-mode hot-path timing control before instrumentation;
the median of at least three matched instrumented runs may regress by no more
than 5 percent when provenance output is disabled. Full diagnostic tracing may
cost more, but its wall time and byte cap must be recorded.

### 2B - One Q-value authority

Remove independent decision math from `sparse_row_q_for_values()` or reduce it
to a thin call into the same authoritative row evaluator and value accessor
used by the exact solver. Centralize probability/dead-mass/denominator handling
and observed-choice value lookup. Add deterministic tests for terminal mass,
failure mass, self-loop normalization, zero/near-zero continuation
denominators, observed choices, and non-finite rejection.

Any baseline mismatch is evidence to diagnose. Do not add another epsilon or
choose whichever implementation preserves the expected action.

### 2C - Strict finite minimum ordering

Inventory epsilon-based comparisons in action preference, incumbent
preference, overlays, and final selection. Replace objective-order comparisons
with strict finite order and exact-equality stable tie-breaking. Split the
shared interface into a canonical strict row winner and a separately named
policy-replacement/stability decision. The latter may retain an explicit
numerical tolerance; direct-replacement call sites, including quotient and
strict-policy improvement, must not accidentally inherit objective semantics
or discard convergence protection.

Add tests proving that:

- a one-ULP cheaper finite action is the canonical winner regardless of
  insertion order;
- exactly equal values retain deterministic identity;
- a near-equal cyclic/SCC policy does not alternate until `max_sweeps`; and
- a tolerance-suppressed policy change is reconciled against the canonical
  strict winner before exact publication, or the result remains bounded.

### 2D - Result-authority contract audit and conditional correction

Begin from the durable contract rather than assuming a status pair is invalid:

- `termination = ExactClosed`, `policy_status = Exact` means global exact
  closure with a proper executable policy;
- `termination = ExactClosed`, `policy_status = BoundedFeasible` is legal when
  broad/coarse discovery closed but exact refinement retained only a bounded
  executable policy;
- a cap remains visible in `stop_cause` and `cap_hit_mask` even when a verified
  bounded artifact is recovered; and
- exact policy status requires a proper evaluated policy, a globally certified
  lower, and closure under the existing proof contract.

Trace every call to `successful_refined_publication_termination()`. For coarse
`None` or `NoExecutablePolicy`, require an explicit invariant or recorded fact
showing that coarse discovery completed before the helper synthesizes
`ExactClosed`. If that evidence already exists, retain the behavior and add a
focused regression. If it does not, preserve the actual stop classification
or add the minimum orthogonal field needed; do not rewrite the legal status
matrix merely because the enum name contains "Exact".

Map any changed contract consistently through native JSON, `stop_cause`, cap
masks, the C/WASM facade, TypeScript protocol types, worker transport, and
result presentation. Required controls include global exact closure, legal
coarse-close/bounded-refinement publication, cap stop with no policy, cap stop
with a verified executable upper, target-gap publication, solver error, and a
recovered policy whose lower does not close.

### 2E - Explicit lower-bound provenance

Keep the existing open-envelope zero fail-safe and valid closed-envelope
positive lower. Add the minimum machine-readable distinction needed to tell
whether the public lower is globally certified and what proof family produced
it. Detailed envelope state remains diagnostic.

Tests must prove:

- open incremental envelope -> safe zero, not globally certified;
- closed valid envelope -> its certified lower, with provenance;
- bounded upper plus closed positive lower -> bounded interval, not forced
  zero and not exact unless it closes; and
- no finite upper -> lower provenance remains truthful and cannot imply an
  executable policy.

Gate 2 is accepted when each required correction or retained audit conclusion
has its focused native or web regression and the cross-layer result contract
is documented.

## Gate 3 - Replay And Classification

**Status: complete.** Replay the frozen current controls and classify the source
hardening without assigning a cause to the unavailable historical request.

Replay the frozen cases against:

1. the selected source baseline;
2. the Gate 1 request-isolated build;
3. the behavior-neutral provenance build, if Gate 2A changed code; and
4. the remaining Gate 2 corrected build.

Record request bytes or canonical hash, result status, search termination,
bound provenance, lower/upper/value, policy and compiled hashes, first
different choice, construction origin, selection authority, row Q values, and
proper exact evaluation.

For a suspicious action, interpret the pair rather than the origin alone:

| Final evidence | Interpretation |
| --- | --- |
| Fallback construction + closed Bellman or strict exact authority | The history is harmless; audit Q/order if the action is unexpected. |
| Fallback construction + local/verified-upper authority | Valid bounded incumbent only; exact publication would be a result-truth defect. |
| Bellman construction + closed Bellman authority | Audit shared row/value inputs, envelope completeness, and price inputs. |
| Strict-generated + strict exact authority | Audit strict graph mapping and compiler identity if runtime differs. |
| Any construction + unknown authority | Provenance gap; exact publication is forbidden until revalidated. |

If the current report does not reproduce or its input is unavailable, Gate 3
may accept the source-hardening results but must not assign the old symptom a
cause.

The controlled Calculator request defect is fixed at Gate 1. Frozen/current
Imprint input, exact value, compiled strategy bytes, exact evaluation, and
10,000-run behavior match. The historical request remains unavailable, so no
cause is assigned to that report. See the
[Gate 3/4 replay record](evidence/gate3-gate4-replay-classification.md).

## Gate 4 - Evidence-Selected Repair Only

**Status: complete after one evidence-selected repair.** Gate 3 produced no
new witness, but the first Gate 5 native breadth run did: automatic Eldritch
Exalt lost exact strict-lift publication. Policy-change gates now retain their
named numerical stability tolerances while objective selection stays strict,
and streamed compilation unions proof coverage with materialized equivalent
strict carriers. The repaired case is exact, independently evaluated, and
byte-identical to its frozen compiled strategy. See the
[Gate 4 repair record](evidence/gate4-eldritch-strict-lift-repair.md).

Make a behavioral solver repair only if Gate 3 identifies a remaining defect
not already corrected by Gate 2:

- **retained incumbent or Restart completion:** preserve it as an executable
  upper, require every completed row to be an actual improvement, retain
  unresolved authority explicitly, and require complete final revalidation
  for exact publication;
- **Bellman/Q source:** correct the authoritative price, state, probability,
  value, or envelope input and add the smallest exact oracle that fails before
  the fix;
- **strict reoptimization:** preserve strict selection authority across state
  mapping and compilation and reject an unmapped reachable choice;
- **compiler/runtime mapping:** reject an unexpected default action and prove
  every reachable policy action retains identity through exact evaluation; or
- **action is actually optimal:** document the proof and make no behavior
  change merely to match the reported expectation.

Stop and propose a separately selected milestone if evidence instead requires
a scheduler redesign, new state abstraction, cap increase, mechanic ruling,
or action-family expansion.

Gate 4 is accepted only with a pre-fix failing witness, a post-fix passing
witness, proper exact evaluation where exactness is claimed, and no unexplained
change in the controls.

## Gate 5 - Proportional Final Acceptance

**Status: complete.** The selected row was Q, ordering, or policy-selection
change, together with the crossed Calculator/result contract. The final
post-fix repository pipeline passed; the first attempted invocation exposed
one stale S8.3 exact-tie expectation, which was corrected and passed its
focused suite before the accepted final invocation.

Run acceptance once, after all selected implementation gates are complete.
Choose the row matching the actual semantic reach:

| Change class | Required acceptance |
| --- | --- |
| Diagnostic-only provenance | Fresh native build; focused solver tests; deterministic before/after work, policy, value, and hash comparison; matched default-mode timing control within the Gate 2A budget. No portfolio rerun. |
| Result-contract or Calculator request change without policy/value change | Fresh native build; focused solver/API/web tests; `npx tsc --noEmit`; release WASM rebuild and focused browser-protocol semantic checks; one final full repository pipeline because the contract crosses layers. |
| Q, ordering, or policy-selection change | All of the preceding checks; focused exact/bounded controls; lightweight every-base matrix; selected native/release-WASM semantic cases; one current 49-case native/release-WASM portfolio comparison; and one final full repository pipeline. |

For any emitted strategy whose graph or policy changes and requires
verification, use the repository-standard 10,000 simulator runs. Do not repeat
verification for bit-identical strategies merely to recreate Gate 8. The
current suspicious case, if reproduced and changed, is required. The primary
is required when solver policy/value semantics change and must remain exact,
cross-runtime equal, and inside five minutes. Select Warlord, Imprint, or the
representative bounded five-mod control only when the changed code path reaches
them; record why each control was selected.

If provenance touches the primary hot path, default-mode native and release
WASM primary wall times must each remain below five minutes and no more than 5
percent above their frozen same-machine baseline. A first violation may be
rerun twice in alternating baseline/instrumented order to reject transient
machine noise; the median must then meet both limits. Full diagnostic mode is
not held to the 5-percent overhead limit, but remains byte-capped and must
finish within the selected diagnostic timeout.

The concrete end sequence is:

1. audit the staged diff and generated/contract impact using
   [change-impact guidance](../../foundation/change-impact.md);
2. run `powershell -File scripts/build.ps1`;
3. run focused `--solver-solve-only`, `--solver-refinement-only`,
   `--solver-eval-only`, and `--solver-api-only` native suites as applicable;
4. in `apps/web`, run focused Calculator/result tests and
   `npx tsc --noEmit`;
5. rebuild release WASM with `powershell -File scripts/build-wasm.ps1` whenever
   engine ABI, policy vocabulary, result semantics, or browser-visible solver
   behavior changed;
6. run the selected semantic/performance/verification matrix above; and
7. run `powershell -File scripts/test.ps1` exactly once at the end when the
   selected row requires the full cross-layer pipeline.

No rendered UI smoke is part of agent acceptance.

## Stop Conditions

Pause and hand off precisely when:

- localization-required mode lacks the exact suspicious request;
- a Path of Exile behavior is ambiguous;
- provenance instrumentation changes a deterministic value, action, bound,
  result, or strategy hash;
- the two Q implementations disagree and the authoritative inputs have not
  been isolated;
- a reachable exact choice has unknown final selection authority;
- native and release WASM disagree semantically;
- the primary loses exact closure or the five-minute product boundary after a
  policy/value change;
- passing would require a cap increase or weakening exactness; or
- the required repair expands into an architecture or mechanic milestone.

## Checkpoints And Documentation

Recommended local checkpoints, each ending with the required agent co-author
line:

1. activate plan and freeze reproduction evidence;
2. isolate Calculator envelope and Solve requests;
3. add behavior-neutral provenance, if needed, and unify Q authority;
4. correct strict ordering, result authority, and bound provenance;
5. apply an evidence-selected behavior repair, if needed; and
6. complete proportional acceptance, archive the milestone, and return
   `HANDOFF.md` to no active boundary.

Single larger commits are acceptable if the diff remains coherent. Commits are
local-only unless Oliver explicitly requests a push.

## Completion Contract

The milestone is complete when:

- the current suspicious request is either localized, not reproduced, or
  explicitly unavailable under the chosen hardening-only mode;
- odds selection cannot mutate ordinary Solve action scope;
- all decision paths use the shared authoritative Q semantics;
- every smaller finite expected cost wins and exact ties remain deterministic;
- search termination, executable-policy publication, global exactness, and
  lower-bound provenance are independently truthful across native/WASM/web;
- every reachable choice in the focused case has known final selection
  authority, regardless of construction origin, or exact publication is
  refused;
- the proportional acceptance row passes with no unexplained regression; and
- the accepted evidence is archived, durable docs are updated, and no active
  implementation boundary remains.

## Oliver Decisions At Activation

1. Provide the exact suspicious serialized request if it exists, then select
   localization-required mode. Otherwise explicitly select hardening-only mode.
2. Keep any new per-choice provenance diagnostic-only initially (recommended),
   or request a separate product-visible explanation contract.
