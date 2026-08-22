# Persistent Quotient Session And In-Place Frontier Growth

**Status: Gate 6 accepted by owner adjustment on 2026-08-22; Gate 7 is
active.** The persistent-session behavior, progress, value, time, and
responsiveness requirements passed. After reviewing the checked 419,316,840
byte conservative ownership estimate, Oliver replaced the original 150 MB
milestone ceiling with 512 MiB (536,870,912 bytes). This is an acceptance
boundary adjustment only; no engine resource cap or proof behavior changes.
See [result.md](result.md).

Owner: Oliver

Starting commit: `65d4d76ccd98a08b1eb3ce4265fc59fd4c6c8ab0`

Parent: [stopped strict-frontier replay result](../2026-08-21-persistent-strict-frontier-growth/result.md)

## Objective

Extend one live strict quotient session when a competitive carrier appears
instead of throwing away and reconstructing the strict oracle, locator set,
partition, Bellman graph, proof store, alternative obligations, and policy
state.

The change must preserve complete exact carrier coverage, collision-checked
identity, deterministic ordering, split-only partition authority, current-Q
alternative proof, proper executable publication, independent compiled-policy
evaluation, resource caps, cancellation, and cooperative stepping.

The checked target is the Allflame four-natural-T1 Conquest Lamellar case. Its
accepted baseline reaches a verified `3745.7295960574743` upper at 38.94
seconds, rebuilds strict passes near 5,820, 5,924, and 5,961 states, and stops
after 300 seconds with lower zero and 17,584 automatic alternatives open. The
rejected carrier-row cache proved that completed mechanic-row calculation is
not the controlling cost; generation reconstruction is the selected owner.

## Source-Confirmed Boundary

At the starting commit:

- `lift_policy_quotient_pass_task` owns nearly the whole production generation
  in one coroutine. A competitive successor throws
  `QuotientFrontierExpansion`; `PolicyExactLiftWork::Impl` merges only frontier
  seeds and cumulative telemetry, then constructs a new pass.
- Production builds two batch `ClosedPartitionResult` values, assigns final
  cell IDs from zero, assigns every generation `1`, and installs them into a
  new `QuotientBellmanGraph`.
- `QuotientPartitionState` already defines stable IDs and split-only previous-
  state refinement. `QuotientBellmanGraph` and `ProofStore` already support
  append, supersession, source/target invalidation, generations, and reverse
  proof dependencies.
- The unused materialized quotient scaffold exercises part of that persistent
  API, but production duplicates its own streamed assembly path. It is a
  reference and cleanup candidate, not production authority.
- Strict locators, locator ordinals, exact stable keys, closed-partition class
  IDs, quotient cell IDs, Bellman state IDs, and sparse row IDs are distinct
  identity domains even where their storage types coincide.

This is therefore an integration and lifetime milestone, not another
cross-pass cache and not a new quotient algorithm by assumption.

## Gate 0 — Reference Oracle And Forced-Frontier Fixture

- Add a deterministic native fixture that forces at least two competitive
  frontier insertions, including one source-cell split, one target-cell split,
  one preserved cell, and one conditional alternative proof that must be
  revoked and re-evaluated.
- Keep a development/test-only from-scratch path as the reference oracle. At
  every completed frontier prefix, compare canonical carrier and cell
  semantics, exact row costs and probabilities, action identities, Q values
  under existing tolerances, public values/status, selected actions, and any
  compiled JSON/hash. Pass-local numeric IDs need not match.
- Add live internal diagnostics for initial constructions, forbidden full
  restarts, frontier insertions, retained/new/superseded cells, source/target
  invalidations, rows appended/reprojected, obligations revoked/reproved,
  retained/scratch peak bytes, and update wall time.
- Make interrupted or watchdog-stopped diagnostics retain the active strict
  session counters. Do not add public ABI or TypeScript fields unless the
  existing internal/report surface cannot carry the evidence.
- Freeze the accepted 300-second primary measurements from the parent result.
  Do not rerun the primary merely to establish another identical baseline.

## Gate 1 — Durable Session Ownership Without Behavior Change

- Extract a durable production-session owner from coroutine locals. It owns
  the `ProductionPolicyOracle`, exact locator/ordinal mappings, selected raw
  closure, stable partition state, Bellman graph and proof store, admitted and
  completed action accounting, alternative obligations, current/published
  solve state, compiled incumbent, and monotone resource consumption.
- Give each identity domain an explicit mapping boundary or narrow named
  wrapper. No raw locator, partition class, quotient cell, Bellman state, or
  sparse row ID may cross its owner's lifetime accidentally.
- Separate retained session allocations from update scratch allocations. The
  memory ledger must count both at their real simultaneous peak and release
  scratch storage after each update.
- Initially preserve the existing restart behavior behind the extracted owner
  and prove the forced fixture and fast controls are unchanged. This is an
  implementation checkpoint, not a routine suite gate.

## Gate 2 — Grow The Strict Oracle And Selected Closure In Place

- Replace `QuotientFrontierExpansion` as the production restart boundary with
  an explicit cooperative phase transition inside the persistent session.
- Collision-check and intern each new physical `AbstractState` into the same
  strict context. Extend exact discovery and selected closure only for newly
  inserted or newly reachable carriers.
- Append canonical rows and successors under the existing exact mechanics
  authority. Do not retain or replay cross-generation Bellman verdicts, and do
  not create another unbounded carrier/action cache.
- Preserve stable ordering, tie breaks, floating-point evaluation order, work
  limits, transition limits, `max_reforge_work`, and the one-GiB owned-memory
  cap. Resource consumption becomes monotone session state rather than a sum
  reconciled after thrown restarts.
- After the initial construction, production full-restart count must remain
  zero. A failure to extend safely must report a named refinement/resource
  stop; it must not silently fall back to repeated reconstruction.

## Gate 3 — Stable Split-Only Partition Updates

- Bridge production's streamed replay partition to
  `QuotientPartitionState(previous=...)`. Unchanged semantic cells retain
  stable IDs and generations; only actual split children receive deterministic
  new IDs/generations.
- It is acceptable initially to recompute partition scratch against the
  previous stable state. It is not acceptable to reconstruct the oracle,
  Bellman graph, proof store, or exact mechanic universe merely because the
  partition implementation still performs a batch refinement round.
- Grow coverage descriptors for inserted carriers and validate complete,
  disjoint live coverage before any new row becomes proof authority.
- Supersede split cells in the live Bellman graph. Invalidate rows through the
  existing source and reverse-target dependency indexes, then append or
  reproject only rows whose source generation, target generation, action
  generation, admission generation, price generation, or exact payload
  actually changed.
- Prove on the forced fixture that an unchanged cell and row remain valid, a
  source split revokes its rows, a target split revokes predecessors, and no
  stale proof validates after a generation change.

## Gate 4 — Alternative Proof And Publication Continuity

- Persist the automatic-alternative scheduler, attempted set, carrier/cell
  mapping, admitted/completed accounting, and obligation store across frontier
  growth.
- Revoke conditional noncompetitive proofs whenever source coverage, target
  coverage, admission, action identity, price, or current Q invalidates them.
  Requeue only the affected obligations and audit the complete action envelope
  before exact closure.
- Retain an independently evaluated proper compiled incumbent as a monotone
  upper across mutations. A frontier insertion may withdraw current lower or
  exactness authority, but it must not discard a still-valid executable upper
  or expose an unverified one.
- Resume alternative work from its live cursor after localized reproof. Do not
  restart already completed unrelated carrier/action work.

## Gate 5 — Cooperative Lifetime And Cleanup

- Express the persistent workflow as explicit cooperative phases: insert
  carriers, extend closure, refine partition, invalidate, append/reproject,
  solve/publish, and resume alternatives.
- Preserve cancellation and progress publication between phases and within
  existing cooperative helpers. Split newly exposed atomic work only when
  measured public-step evidence requires it.
- Release retired partition scratch, superseded projection payloads, obsolete
  indexes, and temporary solve buffers promptly while retaining everything
  required for current proof.
- Refactor the current oversized production owner when doing so makes state
  lifetime, phase boundaries, or dependency direction clearer. New private
  headers or translation units are authorized; update `engine-sources.txt`
  once and preserve the one-way solver header dependencies.
- Consolidate duplicate partition, cell-construction, row-projection,
  publication, memory-accounting, or failure-classification helpers when the
  persistent path would otherwise create a third implementation. Keep a
  single named authority for each lifecycle operation.
- Once reference parity is established, extract the useful contracts from the
  unused materialized scaffold and remove the dead scaffold in a separate
  mechanical checkpoint. Remove dead fields, stale restart-only bookkeeping,
  obsolete exception control flow, redundant adapters, and unreachable
  branches exposed directly by the new session architecture.
- Include other low-risk debt discovered in files already touched when it is
  behavior-neutral, locally provable, and reduces duplication, ambiguous
  identity, hidden ownership, or unnecessary allocation. Record broader or
  behavior-changing discoveries instead of silently expanding the milestone.

### Refactoring guardrails

- Structural edits may be large, but each must serve persistent-session
  ownership, proof clarity, cooperative lifetime, or removal of directly
  adjacent dead/duplicate code.
- Preserve stable public and private contracts unless a private contract is
  deliberately replaced and all callers move in the same checkpoint.
- Keep mechanical moves separable from semantic changes in the diff and local
  commit history wherever practical.
- Do not retain old and new production implementations indefinitely. The
  from-scratch path remains test/reference authority only; dead production
  scaffolding is removed after parity is proved.

## Gate 6 — Native Qualification

- Run one final native build and the focused quotient partition, Bellman,
  proof, policy-refinement, and solver suites after the complete implementation.
  Intermediate narrow tests are permitted only to diagnose failures.
- Run the forced-frontier fixture in persistent and from-scratch reference
  modes and require semantic/public parity after every insertion.
- Run the fast automatic-Eldritch and Warlord controls to confirm that
  unrelated automatic and influenced action authority, values, selections,
  accounting, and compiled execution remain unchanged.
- Run one checked 300-second native four-T1 primary with verification disabled.
  Compare first upper, final upper/lower, frontier insertions, full restarts,
  preserved/superseded cells, invalidations, row and obligation progress,
  open alternatives, owned memory, wall time, and largest public step.

Native acceptance requires all of the following:

- exactly one initial strict-session construction and zero production full
  restarts after frontier growth;
- at least two measured in-place frontier insertions on the real primary;
- final live upper no greater than `3745.7295960574743`, no upper increase,
  and no unsupported lower or exactness promotion;
- first verified strict upper no later than 42.84 seconds (10% over the
  accepted 38.94-second baseline);
- either completion or at least a 10% reduction from 17,584 open automatic
  alternatives at the 300-second stop;
- estimated native peak ownership at or below 512 MiB (536,870,912 bytes),
  with no hidden or unledgered retained payload; and
- largest public step at or below 1.65 seconds, with cancellation and progress
  still advancing.

If exact closure occurs, the result must independently compile and evaluate to
success one, zero off-policy mass, reconciled cost, and complete action
accounting before it is accepted.

## Gate 7 — Conditional Release Acceptance

- Only after Gate 6 passes, rebuild the tracked release WASM module.
- Run TypeScript type-checking, the complete non-visual web/WASM tests, and a
  release-WASM forced-frontier/reference control.
- Run the release-WASM four-T1 primary with the same request and five-minute
  boundary. Require the native proof/publication invariants, zero full
  restarts, monotone verified upper, bounded memory, responsive cancellation,
  and materially improved alternative progress to survive the browser build.
- Run 10,000 Simulator executions only if a final compiled primary strategy is
  emitted. Do not verify an unfinished progress-only incumbent.
- If native and release WASM acceptance pass and the result is merge-ready,
  run the full repository pipeline once, update the result/handoff documents,
  and create coherent local checkpoint commits with
  `Co-authored-by: Codex <codex@openai.com>`. Do not push.

## Non-Goals

- No mechanic, price, goal, action-family admission, automatic-option grammar,
  or Bellman-objective change.
- No cap-only increase, heuristic action removal, weakened lower bound,
  approximate proof, or new fallback publication.
- No state-928 Imprint grammar rewrite or five-T1 recovery.
- No strategy compiler/router/Fracture compaction, strategy-schema change,
  frontend presentation work, or unrelated repository-wide cleanup.
- No cross-request, cross-price, cross-goal, serialized, or product replay
  cache.

## Stop Conditions

- Any pass-local ID survives past its owning generation without explicit
  collision-checked rebinding.
- Any row, obligation, lower bound, exactness claim, or policy verdict remains
  authoritative after one of its source, target, action, admission, price, or
  Q generations changes.
- Frontier insertion reconstructs the complete production strict pass after
  the initial session, or an apparent optimization merely hides that rebuild
  behind another cache.
- Coverage becomes incomplete/overlapping, partition evolution is not split-
  only, deterministic public output diverges from the reference without a
  proved reason, or action accounting changes.
- Peak memory, public-step latency, or time to the first verified upper exceeds
  the declared native boundary.
- The real primary performs in-place growth but does not materially reduce the
  open-alternative stop. Record the newly measured owner instead of layering
  further speculative lifetime state.
