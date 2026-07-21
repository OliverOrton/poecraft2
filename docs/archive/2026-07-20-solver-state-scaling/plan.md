# Exact Solver State Scaling Plan

**Status: completed and archived on 2026-07-20.** The archive
[README](README.md) records the result and evidence.

Parent: [Exact solver state scaling archive](README.md)

## Outcome

Make ordinary two- and three-T1 product solves tractable without weakening
crafting behavior or hiding structural growth behind larger caps.

This boundary has three implementation levers:

1. finish incremental selected-allocation accounting for the outer
   `SolveWork`, so cap enforcement is not itself a graph-sized scan per state;
2. reuse exact action kernels when an action cannot observe the differences
   between otherwise distinct states; and
3. quotient outer solver states only when every admitted action proves those
   states behaviorally equivalent.

The current `max_discovered_states`, owned-byte, compiled-node, and strategy
JSON caps remain unchanged until the structural work is measured. A later cap
change is allowed only at the capacity checkpoint below and must use the
smallest measured value that closes the accepted cases within all downstream
budgets.

The first motivating case is an empty rare item-level-86 Dire Pelt targeting
T1 item rarity plus T1 maximum life under the Calculator's complete
`goal_relevant` product envelope. On the 2026-07-20 native reproduction it
reached the 100,000 discovered-state cap after expanding 23,548 states, with
76,452 states still in the frontier and no Bellman sweep. About 22.47 seconds
of its 30.92-second expansion were attributed to per-state preparation byte
accounting. A `chaos` plus `restart` control converged at 57,722 strict states
but exceeded the 64 MiB compiled-strategy JSON cap. These observations are
diagnostic baselines, not acceptance claims.

## Exactness Contract

Smaller must not mean less accurate. Two strict states may share an outer
solver state only when all of the following are identical under the proposed
successor projection:

- goal and terminal status;
- automatic candidate admission;
- action legality and availability;
- state-dependent resource quantities and price identity;
- every exact primitive or option transition probability; and
- every successor behavioral class, including observation-owned choices.

Equivalence is taken across **all admitted actions**, not merely the current
policy. Policy-dependent merging could delete a cheaper action and is
forbidden. Crafted, fractured, metamod, occupied-group, influence, Veiled,
Eldritch, and other identities remain distinct whenever any admitted action
can observe them.

The strict `CalcContext` representation remains the correctness oracle. A
coarse signature is only a proposed equivalence class until exact comparison
has found no witness that splits it. Unknown or mismatched cases fall back to
strict behavior; they are never merged optimistically.

No Path of Exile mechanic rule changes in this plan. If implementation exposes
an ambiguous mechanic observation, Oliver decides it before work continues.

## Invariants

1. The native engine remains the only legality, pool, weight, and transition
   authority. TypeScript receives results and diagnostics only.
2. Minimum expected cost and the complete admitted action set remain
   unchanged.
3. Literal `AbstractState` interning remains collision-safe and equality-
   authoritative. This plan measures and removes proven behavioral
   distinctions; it does not replace exact equality with hash equality.
4. Selected-allocation cap enforcement remains conservative at every
   allocation boundary. A fast ledger may overestimate temporarily but may
   never undercount an audited live allocation.
5. Kernel reuse must leave strict state IDs, transition probabilities, solve
   values, policies, and diagnostics semantically unchanged.
6. Quotienting must be exact under the contract above. The existing compact
   junk-layout switch is an experiment, not evidence that a global compact
   mode is safe.
7. Resource limits remain visible failures. No phase raises a cap merely to
   obtain a green result.
8. Testing follows the repository milestone cadence: narrow checks only when
   needed during implementation, then one complete affected acceptance gate
   at the end.
9. No in-app browser or rendered UI review is part of this boundary. Oliver
   owns any later visual review; final browser-path checks are non-visual
   Node/WASM tests.

## Change Surface

Primary implementation authority:

- `engine/src/solver_solve.cpp` for `SolveWork` ownership accounting, sparse
  graph storage, cap checks, and solve telemetry;
- `engine/src/solver_calc.cpp` and `engine/src/solver_internal.hpp` for state
  interning, exact transition caches, signatures, and strict-oracle support;
- `engine/src/solver_abstract.cpp` for any certified state projection or junk
  partition refinement;
- `engine/src/solver_options.cpp` for exact option-kernel observation and
  reuse; and
- `engine/src/solver_compile.cpp` only if the exact quotient requires lifted
  state lookup or exposes a remaining policy-output limit.

Downstream surfaces to inspect under the
[change-impact map](../../foundation/change-impact.md): the native solver tests
and benchmark harness, public solver telemetry if fields change, WASM facade
and generated module, worker parsing/types, Calculator diagnostics, evidence,
and stable solver documentation. No C ABI change is planned; if one becomes
necessary, bindings and protocol work expand according to the change-impact
map before that edit lands.

## Phase Q0 - Freeze Baselines And Add Equivalence Measurement

Promote reproducible native cases into a versioned solver-scaling fixture:

- Dire Pelt, empty rare, T1 rarity plus T1 life, complete product envelope;
- the same goal with explicit `chaos` plus `restart` as the narrow control;
- a representative three-T1 ordinary rare goal selected from currently
  supported modifiers; and
- the existing ordinary, advanced, Conquest, and Mirage scaling cases needed
  to guard against optimizing only one base.

Pin artifact and economy identities. Record discovered/expanded/frontier/goal
states, junk classes, action/operator counts, rows, retained transitions,
reforge work, selected live/peak bytes, phase timings, compile size, and cap
reason.

Add full-evidence-only shadow telemetry that groups strict states by a coarse
candidate signature and reports:

- strict states per proposed class;
- per-action observation-signature cardinality;
- exact kernels reused in shadow comparison;
- the first bounded witness that splits each false equivalence; and
- projected successor-class mismatches.

The shadow path does not change interning, transition storage, or policy
selection. Diagnostic samples are bounded; aggregate counters remain exact.

**Checkpoint Q0:** the baseline is reproducible and the report distinguishes
literal duplicates, proposed behavioral equivalents, and witnessed
non-equivalents. No state-count reduction is claimed yet.

## Phase Q1 - Complete The Outer Owned-Byte Ledger

Replace `SolveWork::fast_estimated_owned_bytes()`'s repeated traversal of
prices, operator resource vectors, transition caches, kernel-row buckets,
automatic-admission maps, result arrays, and nested preference vectors with an
incremental selected-allocation ledger.

Each owner records capacity changes, nested payload insertion/release, shared
storage ownership, temporary peak allowances, and final release exactly once.
Keep the full estimator as an audit path. Reconcile periodically and at phase
boundaries rather than once per prepared state. An audited undercount is a
hard implementation error with an owner-category breakdown.

Preserve checks before allocations that can cross `max_solver_owned_bytes` and
preserve the current peak/live distinction. Do not turn the selected-
allocation estimate into a claim about process or browser heap.

**Checkpoint Q1:** on the pinned Dire case, solve status, cap point, state/action
counts, transition and policy hashes where available, and all non-timing
diagnostics are unchanged. Reconciliations report zero undercount. Preparation
byte-accounting time falls by at least 10x and is no more than 5% of expansion
time on the same native evidence machine.

## Phase Q2 - Exact Action-Observation Signatures And Kernel Reuse

Define an internal observation signature for each primitive and option
operator. It contains exactly the state facts used by admission, legality,
resource accounting, and transition generation for that operator. Signatures
are versioned by layout/operator identity and never shared across incompatible
sessions or goals.

Run signature reuse in shadow mode first: calculate the authoritative strict
kernel, compare it with the proposed cached template after successor mapping,
and retain a bounded mismatch witness. Refine the signature or disable reuse
for that operator family whenever a mismatch occurs.

After a family has an exact comparison path, cache its kernel/template by the
observation signature and map successors through ordinary strict interning.
This phase does **not** merge solver states. It removes repeated calculation
and retained payloads only.

**Checkpoint Q2:** discovered and expanded state counts remain identical to Q1;
start values, selected operators, exact probabilities, resource quantities,
transition hashes, and policy hashes match the strict baseline. There are zero
unresolved signature mismatches, and the report quantifies calculation calls,
payload bytes, and time saved per operator family.

## Phase Q3 - Exact Outer-State Quotient

Construct the global candidate equivalence as the intersection of the proven
observation equivalences for every action that can be admitted at either
state. Automatic admission is itself observable and participates in the key.

Use partition refinement:

1. begin from immutable core facts such as goal status, rarity, affix counts,
   carrier flags, and other always-observable mechanic state;
2. add action-observation signatures for the complete product action set;
3. compare exact strict kernels after mapping successors into proposed
   classes; and
4. split a class whenever legality, cost, admission, probability, observation
   choice, or successor class differs.

The implementation may keep strict local mechanic contexts beneath a smaller
outer DP. Every strict successor must have one deterministic quotient mapping,
and materialization/compiler lookup must retain enough representative data to
emit the same executable primitive strategy. Speculative online merging that
would require silently changing an already-issued state ID is forbidden.

Retain a diagnostic strict mode. On bounded fixtures it provides a full
strict-versus-quotient comparison; on larger cases it may stop at a configured
diagnostic boundary after recording class and witness counts.

**Checkpoint Q3:** all bounded strict graphs satisfy the exactness contract and
produce equal start values and equivalent selected actions after lifting the
quotient policy. The Dire and three-T1 reports show the measured strict-to-
quotient ratio. If exact refinement produces negligible reduction, record that
result and do not substitute approximate compact mode; return to Oliver with
the remaining structural witnesses before any cap change.

## Phase Q4 - Product Closure, Compilation, And Capacity Decision

Run the complete product envelope with Q1-Q3 enabled under the unchanged
normal caps. The required product result is a converged solve that enters
Bellman optimization and compiles an ordinary strategy for both the Dire
two-T1 case and the selected three-T1 case.

If a solve still reaches `max_discovered_states`, perform native diagnostic
sweeps at 100k, 200k, and 400k without changing semantics. Record closure,
growth, wall time, selected live/peak bytes, state/action rows, transitions,
compiled nodes/edges, and serialized bytes. Do not ship the larger setting
during the sweep.

A production cap may increase only when:

- exact quotienting and kernel reuse are already active and measured;
- the graph reaches a known closure rather than merely a later cap;
- selected-memory accounting and non-visual WASM measurements have adequate
  headroom;
- compilation fits the existing node, edge, and JSON limits; and
- the smallest sufficient cap is recorded with the evidence.

If compilation remains the limiting step after a converged exact quotient,
first use exact region/router sharing already expressible in the v1 strategy
vocabulary. A new strategy format, browser transfer redesign, or weakened
compiler limit is outside this boundary and requires a new Oliver-selected
plan.

**Checkpoint Q4:** either the accepted product cases converge and compile under
normal caps, or a measured smallest cap satisfies every condition above. A
cap increase without known closure is a failure of this checkpoint.

## Phase Q5 - Final Acceptance And Documentation

Run one final affected acceptance gate:

1. native build and solver/engine tests;
2. strict-versus-quotient oracle comparisons for the bounded corpus;
3. complete product solves and compilation for the two- and three-T1 cases;
4. exact compiled-strategy evaluation plus 10,000 simulator runs per required
   verification case;
5. rebuild the tracked WASM module;
6. non-visual Node worker/WASM solver tests, `npm test`, and
   `npx tsc --noEmit` in `apps/web`; and
7. the full `scripts/test.ps1` pipeline once after the milestone is otherwise
   ready.

For quotient-changing cases, raw graph hashes need not equal the strict graph
because state IDs and graph shape intentionally change. Acceptance instead
requires equal strict/quotient start values within the existing numeric
tolerance, action-by-action lifted kernel equality, exact evaluator agreement,
and sampled verification within its declared confidence tolerance. Cases
unchanged by quotienting retain hash-equality gates.

Record the final evidence under a versioned solver-scaling fixture, update the
stable solver and evidence indexes, and disclose native versus WASM
measurement boundaries. Archive this plan only after lasting contracts have
been promoted and `HANDOFF.md` names the next actual boundary or returns to no
active work.

## Completion Criteria

This plan is complete only when:

- outer cap bookkeeping is incremental, audited, and no longer a dominant
  expansion cost;
- kernel sharing is exact and reports no unresolved observation mismatch;
- every merged state satisfies the all-actions equivalence contract;
- the accepted two- and three-T1 product solves converge, compile, and verify;
- any cap change is backed by measured closure and downstream native/WASM and
  compiler headroom; and
- final evidence and stable documentation describe the implemented boundary
  without claiming that native measurements prove rendered-browser behavior.
