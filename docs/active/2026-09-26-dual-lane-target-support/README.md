# Dual-lane original-goal support (2026-09-26)

Oliver selected execution of the attached U0–U5 programme. The verified input
packet is retained under `research-inputs/dual-lane-adb5210/` (ZIP SHA-256
`8a9c86df53248e92df7622ccf8285bd2cc1eef5b008f12dfb44a305278171df8`).
Its proposed steps are research input, not engine authority.

## Source finding and scope

`derive_model` in `solver_eval_helpers.hpp` collects every graph condition into
the evaluator's observation universe. `solver_eval.cpp` accounts success from
the graph's executed terminal routes. The earlier E audit's wording that the
evaluator reconstructs a clean requested goal was overbroad: the evaluator's
derived `GoalSpec` is an observation/layout device. It does not itself certify
that a graph solves the original Calculator request.

The retained shared check extracts Finder's exact native-goal success-ingress
contract into the compiler owner and invokes it from Current's actual
compiled-policy assertion, including paired and reused graphs. It preserves
generic authored strategy evaluation. The checker is intentionally
conservative: every edge into success must have the exact compiler-emitted
native request predicate, and no default edge may lead to success. Finder
continues to check its native control object, original root, action scope and
reached programme entries. Current checks its parsed exact root and binds
cached evaluation to a full goal/root/data/operator/price identity. The
compiler now emits exact explicit implicit modifiers, including Eldritch and
Synth flags and an explicit empty list, because the previous `with_implicits`
boolean could rebuild a different starting item. Unrepresented root fields
cause a refusal.

The focused fixture reaches an R terminal on a covered item with one extra
explicit affix and zero cost; the same graph is refused for L. A nonterminal
observation of B does not alter the requested A target. An any-two-of-four
request is checked against its own threshold and refuses a graph guarded for a
different target. Existing default-edge and false-success negatives remain.
Finder's actual `PolicyFinderWork` discovers a positive-cost nontrivial R
Alteration controller from an empty Magic root, and the clean request refuses
that R graph. The C API's synchronous and stepped Finder paths return a
checked zero-cost policy for an already-covered dirty Rare root; Current still
refuses R at both API entries and direct `SolveWork` construction.

## Current proof-profile disposition

The proposed `TargetNeutralZero` profile is **not admitted**. Zero is a valid
global lower under nonnegative prices, but a displayed zero would not undo
earlier proof-dependent decisions. The live paths include:

- `solver_solve_carrier_pattern.cpp::completion_proof_lower_value` selecting
  clean, carrier, terminal-debt, strict, envelope and retention components;
- `solver_solve_incremental.cpp::incremental_classification_certified_lower`
  using restricted `result.values` when the envelope is closed and retained
  positive snapshots otherwise; `advance_incremental_classification` can mark
  an alternative `NonImproving` and record incumbent domination;
- `solver_solve_finish.cpp::run_publication_pipeline` forcing goal-cover setup
  during finalization, and the same file's exact-closed branch assigning an
  exact policy and equal lower/upper from solved values;
- direct envelope and operator consumers of completion lowers in
  `solver_solve_envelope_proof.cpp` and `solver_solve_operator_proof.cpp`.

The selected profile needs an explicit capability bit at producer preparation,
every direct lower/retirement consumer and final status normalization. Tests
must inject positive clean potentials and show they cannot retire rows or
promote exactness. That isolation is unfinished, so no Current R solve, common
L/R Current profile, or eight-case matrix is claimed. The existing ordinary L
proof profile remains available. Finder's R capability is independent of this
refusal.

## Actual A4/A5 controls and A5 Finder target contrast

The frozen native benchmark executable SHA-256 is
`99e03218c6274413374bdd9c21f79237cf88e27f3a2c17bbb26147218956a121`.
Both batches use the original role corpus SHA-256
`404c0ee632b199b85a5fce76a62f9d75ca59839849b2b9efd1bcf4c174125b20`,
compiled artifact manifest SHA-256
`852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`,
case ID `cb01-cross-base-product8-long240` for A5 and
`cb02-cross-base-product8-long240` for A4. The root is empty Rare Conquest
Lamellar level 86, with the pinned goal, price snapshot and goal-relevant
product action scope. Timed cases ran serially with the case's 240-second
Finish, 300-second native watchdog, 315-second host deadline and 1-GiB solver
cap. Strategies were independently exact-evaluated; no new Simulator run was
selected.

| Lane / target | Checked upper | Lower | Result and graph |
|---|---:|---:|---|
| Current ordinary L, A4 | C3,746.13194094858 | C198.83349967477 | Bounded Finish; graph SHA-256 `a7a4a54bad1dfb8c4e9e85ac0060a2cc73943648d0bd69541b47eaa36eb3fe02`, identical to E audit |
| Current ordinary L, A5 | C85,558.7061856044 | C405.36940210634 | Bounded Finish with `refused_unsupported_action` reliability class; graph SHA-256 `7df8b8be00a5c4513d6d5fb761605b5c399957c714079d8c39a103add036dc02`, identical to E audit |
| Finder conditional-retention L, A5 | C524,079.6986482172 | unavailable | Complete bounded search; 8 checked, 6 accepted, 2 refused; graph SHA-256 `92e70dd2776d0ec038158e1a90ce221d96db58fee13e335f36a8eb0c63b8230d` |
| Finder conditional-retention R, A5 | C133,226.02816593435 | unavailable | Complete bounded search; 8 checked, 6 accepted, 2 refused; graph SHA-256 `25557a358d6820794747f45cd3937edb137696d9b65806a0695669b971b8a15d` |

All four emitted policies had complete exact graph evaluation, success
probability one, zero off-policy mass and matching checked cost. Finder's L and
R runs used the same heuristic ranking and conditional-retention grammar;
native logical reforge work was 5,310,355 in each. Their host wall times were
34.84 and 33.46 seconds, native checker time 27.46 and 26.61 seconds, and
finder peak owned bytes 812,027,241 and 793,085,726 respectively. These are
one-run observations, not robust timing claims. The corpus's saved Current
reliability expectation marks both Finder cases `expectation_met=false` and
returns exit code 2, although each native case was `bounded_feasible` with a
checked policy and no report errors. The standard same-problem comparator
cannot compare L against R; these rows are a labelled semantic contrast.
The R cost is a different-target upper, not a clean-policy gain. The Finder
clean A5 controller remains costlier than Current's clean A5 controller.

Evidence: [`Current controls`](../../../out/dual-lane-target-support/U4/current-ordinary-L/ledger.json),
[`Finder L`](../../../out/dual-lane-target-support/U4/finder-L/ledger.json),
[`Finder R`](../../../out/dual-lane-target-support/U4/finder-R/ledger.json).

## Capability and validation

| Consumer / target | Original-target graph check | Positive MDP lower | Exact closure | Native result | Public/UI result |
|---|---|---|---|---|---|
| Current ordinary L/E normal form | Yes | Existing clean profile | Available, not reached on A4/A5 | Qualified clean controls | Existing default Current clean path |
| Current target-neutral-zero L/R | No admitted profile | Would be zero only | Withheld by proposed profile | Refused before work | Unavailable |
| Finder L | Yes | None | None | Qualified nontrivial synthetic and A5 | Existing experimental clean path |
| Finder R | Yes | None | None | Qualified synthetic, dirty zero root and A5 | Native-private only |
| Other noncanonical terminal shapes | No | None | None | Not qualified | Unavailable |

The source-matched native build passed. Focused Compile (1,498), Evaluator
(16,927), Solve (87,667) and API (2,986) checks passed; Compile and API were
rerun after the final implicit-list repair. The final release WASM
rebuild passed with three existing unrelated comparison warnings. The clean
Eldritch browser fixture initially exposed missing implicit identity and
flag serialization; it passes after the repair. Full `npm test` and
`npx tsc --noEmit` pass. The Python corpus-runner suite passes 31 tests after
admitting the private Finder terminal selector. Knowledge lint against
`adb5210` reports zero errors and 18 existing open-claim warnings; lint is a
traceability check, not a proof. No rendered UI review was
requested. Current R profile and its four target-neutral timing arms were not
run because the proof-consumer isolation gate did not pass.

The next supported same-clean question is still candidate quality: Finder's
clean A5 policy remains above Current's. A Current R proof-profile project
would first need the explicit producer, retirement and publication isolation
tests named above. Neither intervention is automatically selected here.
