# Session Handoff - S8.4R.3F Implemented, Normal-Cap Gate Still Open

Updated 2026-07-19 after implementing S8.4R.3F, rebuilding release WASM,
reproducing the linear-retention defect, and stopping the long-running pinned
normal-cap gate at Oliver's direction before Bellman entry. Read
[AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4, S8.0-S8.4, and S8.4R.1-R3 are complete. The S8.4R.3F code,
focused native/web evidence, Calculator feedback, and release-WASM rebuild are
complete, but **R3F's pinned normal-cap Bellman-entry gate is not passed.** It
remains the sole active acceptance boundary. Do not begin carrier-relative
template sharing (R3A), browser transfer/lifetime work (R4), verification-truth
work (R5), integrated acceptance (R6), S8.5, or later work from the stopped
gate.

Historical S8.0-S8.4 evidence remains immutable. R1-R3 regression evidence is
separate under
[fixtures/solver-regressions/s8.4r/v1](fixtures/solver-regressions/s8.4r/v1/).
No completed real product solve, full acceptance suite, 10,000-run
verification, or rendered UI review was performed in R3F. The Calculator now
omits the dedicated Imprint explanation, reports actual native cap/status
detail for incomplete solves, and blocks priced Fracture without an actionable
fresh-base price.

## What R3 Delivered

### Correct Imprint semantics

Magic rarity is enforced only when native checkpoint creation is attempted.
The final solver goal may be rare. An Imprint attempt exits when its discovered
intermediate predicate matches, preserving that actual successor for ordinary
Bellman continuation. Every other exact outcome restores the bound checkpoint
and retries. Checkpoint creation/restoration, one Craicic Chimeral plus three
rare beasts per attempt, retry occupancy, and primitive compilation retain the
existing exact engine paths.

The JSON goal parser now rejects user-authored `imprint_retry` programs and
exits. C ABI metadata, Python/WASM bindings, TypeScript types, Calculator draft
persistence, eligibility, controls, pricing text, tests, and docs describe
automatic state-local discovery instead of a final-rarity or complete-goal
restriction. Missing beast prices defer candidates and are never zero cost.

### Bounded state-local discovery on R2

At each reachable carrier, the transient strict R2 context first asks the
native Bestiary action whether checkpoint creation is legal. It enumerates a
bounded set of goal-relevant primitive programs, executes each exact kernel,
and derives useful exits from goal slots missing at the carrier and satisfied
by actual positive-probability outcomes. Complete create/attempt/restore/exit
kernels and exact resource vectors deduplicate before admission. Only a unique
admitted program's structural primitives are added to the shared solve.

The default program depth/work ceilings are solver search resources, not
mechanic limits. Exhaustion appears in bounded automatic-candidate diagnostics
as `max_imprint_program_depth` or `max_imprint_program_work`. R1 sample, output,
owned-byte, and solve-work caps remain in force; automatic evidence strings are
included in the selected owned-byte estimate.

### Focused rare-final fixture

The fixture
[automatic-imprint-to-rare-focused.json](fixtures/solver-regressions/s8.4r/v1/cases/automatic-imprint-to-rare-focused.json)
starts from a legal magic Vaal Regalia carrier and has a rare two-slot final
goal. The solver automatically selects an exact Augment Imprint stage, exits on
the actual useful magic successor, and continues through ordinary Regal value.
The compiled graph uses the existing create/Augment/route/restore/retry/Regal
primitives.

Its intentionally small deterministic simulation completed 64/64 successes
with zero failure, limit, unapplied, or unmatched routes: 2,230 checkpoint
creates, 2,166 restores, 2,230 Augments, 64 Regals, 2,230 Chimerals, and 6,690
rare beasts. The compact record is
[r3-imprint-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r3-imprint-summary.json).
The required 10,000-run verification belongs to R6.

## Post-R3 Product Scaling Finding

The normal-cap pinned Conquest/Mirage diagnostic did not fail because primitive
Fossil roll DPs were repeatedly rebuilt. It expanded 223 states, admitted
exactly 223 fixed options, retained 2,891 state/action rows and 9,168,904 sparse
transitions, reached 433,238,148 selected owned bytes, and performed zero
Bellman sweeps. Reforge telemetry recorded 2,001 requests, 1,992 hits, and only
9 builds.

The one-fixed-option-per-state signature comes from automatic Fracture
preparation. R2 compares complete option kernels only within one carrier's
transient local admission batch. Every admitted option then receives a new
operator and a mapped kernel keyed by absolute `(state_id, operator_index)`;
kernel equality includes absolute exit, retry, and continuation state IDs. The
preparation retry returns to its own entry carrier, so otherwise-equivalent
closures cannot share retained transitions across carriers. An owner-supplied
follow-up report observed the same linear slope at an 8x transition budget:
1,344 expanded states, 1,344 fixed options, 34 million transitions, then a clean
1.48 GiB selected-byte refusal. Reproduce bounded evidence before relying on
those follow-up numbers.

This is not the resolved eager global S8.3 cross product: construction remains
state-local and the shared abstract layout remains narrow. Owner mechanic
direction 2026-07-18 resolves the Fracture half structurally: fracturing is an
early-craft technique planned over ordinary primitives, so R3F removes
product-path Fracture-preparation closures entirely by making goal-relevant
primitive Fracture an ordinary selectable candidate. R3A then owns the
carrier-relative retained-kernel representation and cross-carrier exact
template sharing for the remaining automatic kinds and for explicit-envelope
Fracture preparation.

## What R3F Delivered

In `goal_relevant` automatic product mode, `fracture` is now a normal priced
primitive candidate. Its native per-state distribution is retained only when a
legal carrier has a satisfied, unfractured goal slot; irrelevant legal carriers
are refused before outcome construction, so rejected rows do not intern
unreachable fractured states. Bounded witnesses distinguish native illegality,
carrier irrelevance, exact primitive inclusion, and missing prices.

Automatic product synthesis no longer creates per-state `fracture_prepare`
closures. Explicit user-authored `fracture_prepare` options still use the
unchanged S7 contract. A priced automatic Fracture solve without `base` is
refused natively because Restart is the miss-recovery route, and Calculator
readiness surfaces the same requirement before solve launch.

The focused S8.3 test now proves the automatic operator is primitive, no
automatic Fracture preparation operator exists, the exact distribution has
four outcomes summing to one, and the price flip remains exactly `23.75`:
Fracture at price `23` has value `124.25`; Restart at price `24` has value
`125`. An irrelevant four-mod carrier discovers only itself plus Restart's
fresh base, with no leaked Fracture successors. Release WASM was rebuilt from
the final native source; no C ABI or strategy vocabulary changed.

The reproduced pre-fix normal-cap record is pinned in
[r3f-linear-retention-before.json](fixtures/solver-regressions/s8.4r/v1/evidence/r3f-linear-retention-before.json):
243 expanded states retained 243 Fracture preparation operators, reached
9,989,904 transitions and 452,827,288 selected bytes, performed zero Bellman
sweeps, then refused `max_transitions`.

## Focused Validation Completed

No full acceptance suite was run.

- `powershell -File scripts/build.ps1` passed after the locked benchmark image
  was stopped.
- Native `--solver-s8-3-only`: 150 checks, 0 failures.
- `npx tsx test/solve-workspace.test.ts`: 4 checks, 0 failures.
- `npx tsc --noEmit` passed.
- `powershell -File scripts/build-wasm.ps1` rebuilt the release module from the
  final source. The R3F WASM picker subtest exposed primitive Fracture directly.
- The broader `engine-smoke.test.ts` run continued past the R3F picker subtest
  but later stopped at an automatic permanent-bench group-goal solve, which did
  not converge. Do not report the full smoke as passing or attribute that
  boundary to R3F without a separate diagnosis.
- No C ABI or strategy vocabulary changed in R3F.

## Exact Next Boundary: S8.4R.3F Normal-Cap Gate Only

Do not add more R3F mechanic behavior and do not start R3A. The implementation
is complete; the remaining boundary is evidence that the pinned
`conquest-lamellar-mirage-r3f-product` request completes expansion and enters
outer Bellman optimization under the checked-in normal caps. The case uses an
explicit 1-chaos `base` override solely to activate Restart; it is not a Mirage
market quote.

Reproduce with:

```powershell
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case conquest-lamellar-mirage-r3f-product --output build\s8.4r3f-after.json --skip-verification
```

Oliver directed the 2026-07-19 final attempt to stop before report emission.
An earlier batched attempt was also stopped to unlock the benchmark executable
for the requested final native/WASM rebuild after about 27,094 CPU seconds and
about 810 MB private process memory; those process samples are diagnostic only,
not solver-owned telemetry or a cap result. No attempt reached a reported
Bellman boundary, and no cap was raised. The exact stopped-attempt record is in
[r3f-implementation-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r3f-implementation-summary.json).

If Oliver keeps the gate, let the exact command finish and pin its emitted
report/compact after-summary before marking R3F complete. If Oliver explicitly
changes or waives the gate, record that decision in the plan and evidence first.

## After R3F: S8.4R.3A Carrier-Relative Kernel Scaling

Repair carrier-relative automatic-kernel retention for the remaining automatic
kinds (Imprint, renewal, protected-side, temporary bench) and for
explicit-envelope Fracture preparation before browser transfer work:

- normalize an option's entry-relative retry/self mass instead of treating the
  absolute entry state ID as part of an otherwise shared kernel;
- cache and reuse exact preparation/renewal templates across carriers with the
  same preparation program, target goal slot, preserved-base signature,
  non-self exits, continuation classification, primitive routes, and resource
  vector;
- retain only the carrier-local Bellman self reference/template ID per state,
  admit dependencies once per unique template, and keep distinct fractured or
  otherwise mechanic-relevant preserved substrates separate;
- add per-automatic-kind and per-primitive-family candidate/template/row/
  outcome/transition/time/byte telemetry;
- tighten Essence relevance to guaranteed modifiers that actually satisfy a
  requested slot, add bounded exact Harvest/Fossil/Essence relevance and
  deduplication where proven, and report search bounds as resources rather than
  mechanic invalidity;
- prove physical affix ordering and junk ordering collapse in abstract-state
  projection; preserve goal-slot identity only where an admitted action can
  observe a real difference; and
- retain R3F's pinned normal-cap Conquest result and prove bounded per-carrier
  retention for the remaining automatic kinds in the new telemetry.

Do not silently hard-prune every destructive raw renewal by solve depth or a UI
stage. If Oliver chooses a narrower Fossil/Essence product scope, first pin the
specific below-exit retry rule and label the result optimal only within that
scope. A general focused/custom action scope is a fallback only if the exact
sharing/relevance/state repair still cannot produce a usable product policy.
Oliver owns rendered and visual review.

## Deferred Boundaries And Gotchas

- R5 owns terminal/off-policy verification truth, confidence, cost semantics,
  and exact evaluator vocabulary such as `mod_count`.
- R4 owns giant strategy transfer, clone removal, compile-size alignment,
  solved-handle/transition-closure release, rebuild-on-reprice, and browser
  live-byte lifecycle checks after R3A passes.
- R6 alone runs exact real product solves, required 10,000-run compiled-policy
  verifications, and the complete non-visual acceptance/evidence pass.
- B1.5 remains waived/deferred, not complete. Do not silently backfill it.
- Prefix-to-Suffix and Suffix-to-Prefix beastcrafts remain parked and absent.
- R1 caps/telemetry and R2 lazy state-local generation remain settled. R3F
  changes only Fracture's product planning shape (primitive candidate, no
  product-path preparation closures); R3A then changes only cross-carrier
  retained-kernel normalization/sharing and measured product usability for the
  remaining kinds; do not recreate or reopen the resolved eager global
  cross-product/`bad_alloc` design.
- Large S8.0 strategies and projections are immutable historical evidence, not
  normal product inputs.
