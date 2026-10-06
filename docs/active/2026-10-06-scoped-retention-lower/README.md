# Current scoped retention lower: implementation checkpoint

Oliver resumed the completed probability-aware lower research lane as an
implementation at 2026-10-06 02:50 UTC. This is the one living implementation
record. The complete argument and investigation remain in the
[published report](https://github.com/OliverOrton/poecraft2/blob/6d1c237b54dfc9052ff5b1d377dc6e6b7a3706ce/docs/research/2026-10-05-probability-lower/report.md).
That research branch/report is unchanged. This record supplies source and
validation handoff; it does not replace the report or issue runtime evidence.

## Frozen scope and evidence state

- Base: full remote main `7252027c80856628ed16734583bfc9d6e166458b`, verified
  before implementation and again at the source checkpoint. The normal checkout
  remains on `7eb16ac3d63834fd5d3256ab42483f47d264b764`.
- Local branch: `dot/sol61-scoped-lower-20261006`, in this task's isolated
  implementation checkout. The source-freeze commit is recorded in the external
  task-local delivery receipt, avoiding a self-referential commit identifier.
- Local source and finite fixtures are written. **No build, test, solver or
  Simulator run has been executed.** Assertions below are intended acceptance
  conditions, not passed results. No remote push, merge or deployment occurred.
- The active causal checkout was observed at
  `ce4efa33471eb55756445e2ec367326cef6f9e57` during review. Its code and dirty
  canonical documentation were left intact. Qualification is not transferred
  from that checkout, an earlier executable or a similarly named component.
- No protected path `0`, frozen game/economy files or release artifacts were
  inspected or modified. No new dependencies or experiment infrastructure.
- Builds/tests are pending the parent's coordination of shared LOCAL and CI
  ownership. The previous programme's spent allowances remain spent; this
  checkpoint grants no Conquest, Bow or economic run allowance.

## Selected construction and theorem obligations

Reuse the checked native `PreparedPhasePotential` issuer as a stopped,
current-progress relaxation. Retain clean Rare/all-required goals, the complete
native registry and generated-family grammar, all-member domain projection,
current goal-mask/rarity/occupancy/crafted categories, native Annul retention,
pool-envelope laws, cooperative preparation and existing checked arithmetic.
Do not enable the legacy `OrdinaryClean` profile on neutral scope.

For a supported physical state, let `L(s)` be the installed checked potential;
for a goal or an unsupported member, use zero. Every legal priced native
primitive must have an optimistic relation proving
`L(s) <= c_lower(s,a) + E[L(S')]`. Every positive-probability outcome and every
observed-choice branch is covered. Static descriptor legality remains the
native authority. The auxiliary registry is a superset of the requested
registry scope, so extra optimistic rows can weaken this component but cannot
strengthen it unsoundly. Uncovered generated families refuse preparation.

Newer primitive laws and companion-state descriptors receive a paid stopped
exit with zero continuation. Their support witness grants every missing goal
simultaneously; it does not ask the older reach/pool law to describe Foulborn,
Dominance or restore semantics. For such an exit, `L(s) <= c_lower(s,a)` is the
entire obligation. Thus any cheap or free applicable exit imposes a real
ceiling; a free outside exit can collapse the component to zero. An optimistic
paid exit is a proof relaxation, not an executable upper policy.

For covered generated controllers, unfold the already audited primitive/control
coverage and telescope the statewise inequalities through their complete paid
steps and observations. Unsupported physical members and compressed retry
markers stop at zero. Imprint restore scope and authored ImprintRetry remain
refusals. No ever-acquired ledger is substituted for current progress. The
registry-wide support fallback does not itself authorize a restore-sensitive
retention relation.

With nonnegative paid costs, a proper native policy reaching a terminal almost
surely, and the finite bounded installed potential, finite-horizon telescoping
followed by bounded convergence yields `L(s) <= E[total policy cost]`. Taking
the infimum over proper native policies gives the desired lower. Numeric
iteration only proposes values; the existing value-specific directed checker
issues the potential after complete coverage. Cancellation, a cap or a scope
refusal installs no partial vector. Existing claims CLM-0006, CLM-0012,
CLM-0014 and CLM-0018 retain their original premises.

## Activation and useful consumer

Three permissions remain separate:

1. Native-private Current selection may request preparation using
   `native_retention_lower` plus `current_scoped_retention`. C ABI defaults and
   public option layout are unchanged. The existing private diagnostic maps
   the latter bit only for native Current. Finder clears it even for direct
   C++ callers; Emscripten builds cannot enable the new neutral issuer.
2. A full checked component must be installed, and
   `native_retention_consume` must be true, before neutral lower lookup can
   use it. Neutral preparation supplies zero candidates and never prepares
   or reads the legacy goal-cover clean tables or working/focused value vectors.
3. The new local retirement permission needs a complete materialized row,
   compatible statewise values from the output policy incumbent, and the
   incumbent's independent certification/evaluation, properness and executable
   flags. Immediately before an attempted new retirement, the existing
   `certified_incumbent_invalid_reason` owner must also return null for the
   current goal, economy, action prefix, caller scope, artifact, monotone graph
   generations, retained graph prefix and materialization/provenance.
   Root-only/rejected values and working result values do not qualify.

The first consumer is delayed row classification. For a row with no explicit,
embedded or observed-choice source return, transport the lower once through
the complete native row: `q_L = c + E[L(S')]`, with the existing observed-choice
minimum where appropriate. The new consumer rounds each nonnegative price,
coefficient product and accumulation downward in the native stored-coefficient
scope, independently of the existing nearest-rounded proposal Q. This does
not claim a different rational-law or probability-coefficient certificate.
If `q_L >= U(s)` for that checked compatible
incumbent, the row cannot improve the incumbent and may be retired locally.
The existing evaluator eliminates self returns, so the new permission refuses
every self-containing row rather than claiming its repeated-row quantity as a
first-action lower. This is a conservative limit on the new consumer only.

Unmaterialized-family retirement keeps its existing profile guard. Global
exact closure, global/public progress lower and TargetNeutralZero permissions
remain unchanged. The existing policy exact-lift completion oracle also sees
the prepared native lower; it retains its own verified-artifact requirements.
No whole-scope exact-closure claim follows from either local consumer.

Current defaults remain off. Finder has no new activation; authored/public
controls and WASM are unqualified. The existing OrdinaryClean issuer remains
available under its earlier gate. No strategy vocabulary or mechanics changed.

The extra row guard and directed transport cost linear work in that row's
transitions and observed-choice successors, with constant extra scratch.
Neutral lower-vector construction remains linear in discovered states, using
the existing vector allocation. Proof preparation retains its existing labelled
32/64 MiB cap and aggregate accounting; no larger allowance is introduced.
The final compatibility check is called only after the lower comparison
passes. Its existing owner hashes the retained graph/action prefixes and uses
an allocation-free quadratic economy-key scan. This cost is additional to row
transport and may matter economically; no new cache or framework is introduced.

### CI source-review correction before runtime validation

Review of initial freeze `4015c2beb0b930b8dc1f9616d6444c6a1718ade2`
identified that assignment plus certification flags did not establish current
compatibility. `commit_output_incumbent` does not validate on assignment, so no
lifetime invariant is asserted. The corrected consumer calls the existing
`certified_incumbent_invalid_reason` immediately before granting its new
retirement permission. That owner delegates to `retained_incumbent_invalid_reason`
and the existing retained-fallback contract; its identity/generation/provenance
checks precede certification, properness, executable and evaluated-cost checks.

New consumer countertests hold certificate flags and values fixed while
changing goal/economy/action/caller/artifact identities, actual caller restart
scope, source/target generations, retained prefix and materialization/payload
provenance. They also mutate an actual retained-prefix probability. Each must
return the precise owner's invalidation reason and leave Scour unresolved.
The positive control retains a separately captured native Bench prefix,
appends Scour outside it and remains compatible; self/choice probes therefore
exercise their own vetoes rather than being hidden by stale-prefix rejection.
These are structural consumer/compatibility tests with the separately derived
native Bench value oracle, not graph-issuance qualification. All remain unrun.

## Finite independent witnesses and counterexamples

`run_solver_scoped_lower_tests` is selectable alone with
`--solver-scoped-lower-only` and included in `--solver-phase-lower-only`.
It uses the existing ten-mod native fixture, with explicit prices and registry
descriptors, and does not use its auxiliary model as the value oracle.

- Deterministic case: empty Rare can Bench the clean goal for 1. Other useful
  operations cost 100; Scour costs 1/2 but produces Normal and needs an
  expensive rarity change before completion. The proper native optimum is 1.
  Native kernels independently witness the Bench terminal and Scour successor.
  Consumed lower evidence should retire that Scour row; prepared-unconsumed
  evidence should leave it unresolved with the same checked preparation.
- Stochastic retention case: one natural goal plus one junk, Annul price 1,
  Bench price 1. Annul removes either affix with probability 1/2. Empty Rare
  has value 1; lone junk has value 2 by Annul then Bench; the root therefore
  has value `1 + (1/2)*0 + (1/2)*2 = 2`. Every other operation costs 100,
  except Scour to Normal. The native kernels check both probabilities and
  the proper miss recovery. A favorable-deletion oracle would incorrectly
  give 1. The new lower is expected above 1.9 and at most 2.
- Cheaper same-law Bench: a distinct actual registry descriptor costing 1/4
  changes the native optimum to 1/4 and must cap the new component accordingly.
  This is a native mechanics fixture, not public vocabulary qualification.
- New-law paid exit at 1/8, including an unmaterialized dependency: support
  must cover the entire registry and retention must not exceed 1/8. A free
  companion-state exit must reduce the root lower to zero. No newer law is
  executed or inferred for these stopped exits.
- Wrong price/registry identity, extras allowed, any-k scope, an
  unknown generated-family bit, Imprint restore scope, influence and compressed
  retry markers, unchecked/improper or root-only/rejected uppers, self returns,
  missing Current gate, an unsafe numeric proposal, cancellation, abandoned
  partial setup and an insufficient proof reservation retain refusal/zero or
  unresolved status as appropriate.

The manual incumbent in the consumer fixture is a stand-in at the existing
consumer seam. Its flags do not test graph admission. The separately enumerated
native Bench kernel and paid case analysis establish its value; production
incumbents must still come through the checked issuer.

## Owner map and coordinated integration

| Owner | Change in this isolated branch |
|---|---|
| `solver_phase_lower.hpp/.cpp` | Explicit full-goal paid-exit support witness; terminal semantics in exact reuse identity; support version bump |
| `solver_phase_probability.cpp` | New/companion primitive first-exit floor; clean semantics guard; potential version bump |
| `solver_solve_contracts.hpp`, `solver_solve_types.hpp` | Native-private Current issuer gate and installed/consumed readiness |
| `solver_api.cpp`, `solver_finder.cpp` | Native Current mapping; Finder and WASM isolation |
| `solver_solve.cpp`, `solver_solve_bounds.cpp` | Neutral cooperative preparation from zero with legacy setup disabled |
| `solver_solve_carrier_pattern.cpp`, `solver_solve_incremental.cpp` | Independent neutral lookup and conservative checked-policy local retirement |
| `solver_solve_telemetry.cpp` | Preserve neutral global progress/closure permissions |
| `test_solver_calc.cpp`, `test_main.cpp`, `tests.hpp` | Finite correctness/counterexamples and focused selector |

The active solver owner's representation-independent eligibility work may
overlap phase preparation/projection, solve contracts/types, incremental
classification, API diagnostics and telemetry. Integrate by reviewing the
precise issuer/consumer gates against that owner's final revision. Do not
cherry-pick blindly or transfer any of its receipts to this source. Upper
repair remains a separate owner; this branch does not upgrade a root cost into
a statewise table. No shared canonical documentation was edited.

Doc-ready integration map, after validated coordinated adoption:

- `docs/solver/lower-pruning.md`: independent neutral producer/consumer gates,
  complete registry fallback, Current-only activation and self-row refusal.
- `docs/solver/mathematics/lower-bounds.md`: stopped paid-exit theorem and full
  controller/properness premises; deterministic and stochastic counterexamples.
- `docs/solver/claims.md`: correspondence/history under CLM-0006/0012/0014;
  retain CLM-0018's cheap proper relaxed-policy ceiling distinction.
- `docs/solver/current-status.md` and `HANDOFF.md`: one short validated
  implementation entry with frozen source/build/batch receipt and next owner.
  Until then label this source-written/unrun; do not duplicate this record.
- `docs/foundation/change-impact.md`: no new map entry is needed; this follows
  the existing solver-algorithm/native-private path. No data, public ABI or
  strategy-vocabulary migration is proposed.

## Prepared validation batch and stopping decision

CI owns build orchestration on the exact frozen source. The existing narrow
Windows target is `scripts/dev-engine.ps1 -Task Tests -Jobs 1` (CMake
`tests-only`: engine tests plus header smoke). This task has not invoked it.
The isolated sparse source checkout includes the tracked
`fixtures/economy/harvest-recipes-v1.json` from that exact base; its local Git
blob was verified as `e020ae74ff5bee9b127de82b1bf52524d6ec3d9b`.
Do not copy or refresh frozen prices or compiled data. These finite selectors
require no artifact argument.

After the parent coordinates shared LOCAL and receives the source/build
identity, execute one serial finite batch against that exact test executable:

```text
poecraft_engine_tests.exe --solver-phase-lower-only
poecraft_engine_tests.exe --solver-proof-pattern-only
```

The first includes the new selector's fixtures and existing phase/probability
regressions; the second checks the proof-pattern manager touched by optional
issuer setup. Both selected entry points were inspected and contain no
Simulator invocation. Use the new standalone scoped selector only to isolate
a diagnosed failure, not as an extra routine duplicate run. Record full source
SHA, executable hash, selectors, return codes, checks/failures and refusal
details using existing CI/run receipt owners. No tests run concurrently with
other owners' native work. The morning gaming window stays quiet.

Stop before any economic experiment if coverage, admissibility, interruption,
identity or the independently derived finite bounds fail. Fix the failed
premise or retire the construction; do not weaken the oracle. If the finite
consumer witness passes, separately inspect current paid-exit ceilings and
compatible-upper hit rates before requesting any runtime allowance. Cheap
outside exits or unavailable statewise uppers may make this sound component
economically useless. Older 405/36 observations, matched Conquest equality and
the rejected/root-only upper diagnostics remain negative evidence, not proof
that this implementation improves the present gap.

Remaining questions for a later Pro pass are only whether a stronger
restore/control abstraction can avoid the paid-exit ceiling with complete
native coverage, and whether compatible local uppers occur often enough for
this bounded consumer to matter. Those questions are not prerequisites to the
finite implementation validation and do not authorize new experiments.
