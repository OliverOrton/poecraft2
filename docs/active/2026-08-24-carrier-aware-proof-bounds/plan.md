# Carrier-Aware Proof Bounds And Consumers

**Status: Gate 3 passed; Gate 4 active (2026-08-24).** Oliver selected this successor
after checkpoint `bd80522`. It inherits Gate 0 attribution, Gate 1's proof /
ordering ownership split, and the current-semantics control corpus. Failed
Gate 2 ordering is not an entry condition and must not be retried; legacy
focused and incremental ordering remains unchanged.

Parent: [Active work](../README.md)

Predecessors:

- [Carrier-Aware Bounds And Prioritization](../2026-08-24-carrier-aware-bounds-and-prioritization/plan.md)
- [Carrier-Aware Control Authority Repair](../2026-08-24-carrier-aware-control-authority/plan.md)

Activated from checkpoint: `bd80522`

## Objective

Extend the existing clean carrier MDP into a locally conservative carrier-
aware admissible completion bound, prove it against complete materialized
rows, and retain it only if a real proof consumer fires. If Gate 3 produces a
useful continuation lower, re-evaluate lower-only unresolved-action
descriptors. Finally route only proved, correctly covered authority into
pruning, focused gaps, and public lower/gap termination.

## Inherited authority and fixed boundaries

- Gate 0 telemetry and Gate 1's `ProofLowerValue` / `CarrierOrderingScore`
  separation are retained from `bd80522`.
- `FocusedLegacy` and `IncrementalLegacy` remain the only carrier-ordering
  modes. No carrier/action ordering experiment is authorized here.
- Exact terminal semantics remain unchanged: every explicit affix at success
  satisfies a requested goal slot; empty slots and implicits remain allowed.
- No mechanics, prices, goal scope, action admission, caps, strategy
  vocabulary, frontend rule authority, or compiled data changes.
- Proof-bearing values cover every relevant admitted alternative or fall back
  locally to the existing universal cover.
- A restricted optimum never becomes a global lower while the requested
  action envelope remains uncovered.
- Preserve exact obligations and executable-policy properness. A lower-only
  descriptor is never an executable row, constructive upper, or closure
  certificate.
- Do not run the full repository acceptance pipeline. Final acceptance is the
  proportional native / release-WASM / nonvisual web boundary specified in
  Gate 5.

## Preservation checkpoint — passed

Before activation, the complete prior working tree was reviewed and retained
in local commit `bd80522` with the required Codex co-author trailer. It
contains Gate 0 telemetry, Gate 1 source/type ownership, current-semantics
controls, failed-ordering evidence, and the restored legacy ordering.

`git diff --check` and the native build passed. A fresh separately named
1,000-expansion report reproduced Gate 1 exactly: lower `0.01165`, upper
`59810.9537769745`, 51 / 149 graph, transition hash
`fb8dc170b29920df`, and final policy hash `1b98ca41e69ad1b1`.

## Gate 3 — Carrier-aware admissible bounds

**Result: passed.** The rarity × satisfied-mask carrier MDP, state-local
terminal-action debt, local fallback, proof fixtures, and attribution are
retained. On the 1,000-expansion four-of-five witness the certified global
lower rises from `0.01165` to `3.47245`; the exact-evaluated upper, 51 / 149
graph, transition hash, and final policy hash are unchanged. Solve wall is
`41083.6708` ms versus the `39310.6266` ms control. See the
[Gate 3 result](evidence/gate3-result.md).

Extend the existing clean carrier MDP rather than introducing an unrelated
heuristic. Prioritize the measured fallback holes:

- rare carriers and exact satisfied masks;
- goal-plus-junk and goal-plus-temporary/metamod nonterminals;
- fractured goal preservation;
- blocked prefix/suffix capacity; and
- the influence/Eldritch identity and automatic side options required for a
  valid relaxation.

The abstraction may make the real problem easier through favorable identity,
free setup, blocker removal, target side, or goal preservation. It may not
omit a cheaper executable action, reduce real success probability, charge
avoidable preliminary cleanup, or destroy progress that a real outcome can
retain. In particular, a protected reforge that replaces junk directly must
not be charged an unnecessary cleanup step merely because junk exists.

Unknown, unpriced, or unproved carrier/action shapes fall back locally to the
existing universal cover. Keep the strict Eldritch guard until every admitted
automatic side option is represented by a no-stronger relaxation.

Add focused proof tests for exact goal zero, positive mandatory cleanup debt,
goal-plus-junk/metamod nonterminal behavior, protected direct replacement,
paired obstructions, rare carriers, Eldritch and non-Eldritch sessions,
action-set monotonicity, and Bellman inequalities
`h(s) <= price(a) + E[h(S')]` for every materialized row in small complete
fixtures.

Integrate the result only as a maximum with existing admissible components.
Measure finite operator separation, incumbent pruning, focused direction, and
certified lower/work movement on the current four-of-five and five-T1
witnesses. Stop and restore Gate 3 if proof fails or the machinery only
changes a displayed number.

## Gate 4 — Unresolved-action lower descriptors

**Status: active.** Gate 3 supplies a useful proved continuation lower, so
the archived mandatory-first-price descriptor architecture is being
re-evaluated under the stop conditions below.

Enter only if Gate 3 supplies useful continuation lowers. Re-evaluate the
archived mandatory-setup-price prototype against today's graphs. For each
unmaterialized obligation, use guaranteed immediate price plus a proved
optimistic continuation only where all possible exact successors have no-
stronger carrier coverage; otherwise retain immediate-price-only authority.

Descriptors are price-scoped proof records. They participate only in lower
minimization, remain explicit until an exact row exists or their lower is
strictly separated from an independently verified incumbent, and must keep
materialized closure distinct from proof closure.

If cheap bench-first descriptors again pin the minimum, a broad reforge hits
the state/work cap, no descriptor separates, or lower/work does not improve,
remove only Gate 4's experiment, record the precise failure, and continue
Gate 5 with the valid Gate 3 bound.

## Gate 5 — Consumers and proportional acceptance

Route only matching proof authority into:

- `optimistic_operator_lower` expansion pruning;
- preservation pruning where successor coverage is identical;
- focused fringe gaps and fixed-time scheduling; and
- public lower/gap termination only with full action-envelope coverage.

Keep independent pattern, restricted-search, unresolved-descriptor, and
exact-closure provenance separate in telemetry and result truth.

Exercise:

1. current four-of-five and clean five-T1 Conquest Lamellar;
2. non-armour partial-five and tri-elemental Spine Bow;
3. bounded current-semantics Warlord;
4. automatic Eldritch and Imprint controls; and
5. protected/dirty paired proof fixtures.

Preserve action coverage and executable-policy properness. Diagnose
regressions without weakening terminal semantics or silently dropping
actions. Exact-evaluate every changed compiled strategy and use 10,000
simulator runs whenever final strategy verification is required.

At final acceptance run the affected native suites, native build, release
WASM rebuild/parity, applicable nonvisual web tests, `npx tsc --noEmit`, and
`git diff --check`. Do not run `scripts/test.ps1` unless Oliver separately
requests the full repository pipeline.

## Commits and handoff

Keep this plan, its evidence, the active indexes, and `HANDOFF.md` current.
Create a local checkpoint after every coherent successful gate. Do not push.
Every commit ends with:

`Co-authored-by: Codex <codex@openai.com>`
