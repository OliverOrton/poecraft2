# High-Impact Automatic Admission Closure Result

**Status: accepted on 2026-08-21.**

Owner: Oliver

Starting commit: `c955d028e4e85a220cf33d6d01517b64dd06906d`

Plan: [High-Impact Automatic Admission Closure](plan.md)

## Result

The Calculator's high-impact incremental scheduler now prepares the existing
carrier-local automatic action envelope. It no longer evaluates only delayed
Fossil, Harvest, and Essence rows while silently skipping automatic bench,
Eldritch, Cannot Roll, Veiled, Imprint, and related state-local programs.

High-impact scheduling owns a separate automatic-carrier cursor because its
delayed primitives retain their operator-major cursor. Retaining a carrier now
creates one explicit automatic-preparation obligation. Successful preparation
discharges that obligation and creates one obligation per materialized
operator; resource refusal leaves the preparation obligation and the named
unresolved count open. Exact closure additionally requires the automatic
cursor to be caught up and no preparation frame or completed batch to remain.

No product/action-family filter changed. The real four-goal case remains 24
direct candidates, 158 automatic dependencies, and 102 filtered primitives,
identical to the stored pre-repair result. This work changes when the retained
automatic dependencies are evaluated, not which dependencies or ordinary
families the product envelope retains.

## Reproduced Failure And Regression

Before the repair, the combined native control enabled both Calculator flags,
included a delayed Harvest reforge, and priced Veiled Exalt as the cheaper
exact route. It reported zero automatic carriers, zero Veiled candidates, and
no policy. The ordinary control selected Veiled Exalt at exactly 1.25 Chaos
and passed 10,000/10,000 simulator runs.

After the repair, the high-impact control reports a carrier, two eligible
Veiled candidates, and an explicitly open three-action obligation instead of
claiming closure. The ordinary exact-selection and 10,000-run control remains
unchanged. Release WASM now runs the existing forced-winning product Eldritch
case with high-impact scheduling enabled and asserts both nonzero automatic
carriers and Eldritch candidates before compiling and executing its winning
policy.

## Real-Case Characterization

Two deliberately capped native probes stopped at 512 discovered states. They
are admission/fail-closed diagnostics, not new optimality measurements:

| Case | Result | Automatic carriers | Candidate dispositions | Automatic preparation evidence | Open obligation |
| --- | --- | ---: | ---: | --- | ---: |
| Four natural T1 | `refused_state_cap` | 1 | 77 considered / 75 eligible / 1 rejected / 1 deferred | 479 temporary-bench variants; nonzero synthesis; parent Eldritch generation deferred by the named cap | 937 |
| Five natural T1 | `refused_state_cap` | 1 | 1 deferred | 598 temporary-bench variants; nonzero synthesis | 17 |

Both diagnostic benchmark commands return nonzero because the artificial cap
deliberately violates each fixture's full-solution acceptance contract. Their
solver status is the named state-cap refusal above; neither contains a harness
or internal engine error after the finalizer repair.

The first four-goal capped probe exposed an observational finalizer defect:
Fracture Q diagnostics attempted to evaluate a retained row against a partial
value table and threw an out-of-range exception. Finalization now treats any
row with an unavailable successor value as unresolved. Repeating the same
probe cleanly reports `refused_state_cap`, preserves the open envelope, and
contains no harness error.

The earlier stored four-goal value `3745.7309340083884` remains a proved
executable policy upper, but its old `exact_closed` label depended on zero
automatic carriers and is not an optimality authority after this defect was
confirmed. This focused repair did not run an uncapped replacement proof.

## Acceptance

The following passed:

- native build;
- focused S8.3 automatic suite: 531 checks;
- focused solver suite: 96,120 checks;
- the capped four- and five-goal admission probes completed with the intended
  named resource stops, open automatic obligations, and no harness error;
- release WASM rebuild;
- release-WASM engine smoke, including the high-impact forced Eldritch winner:
  28/28 checks;
- the complete non-visual web test suite;
- `npx tsc --noEmit`; and
- `git diff --check`.

The full repository acceptance pipeline and an uncapped primary solve were not
run. No browser visual review was performed. Nothing was pushed.

## Deferred Boundary

The next solver-quality decision is whether to run and optimize an uncapped
four-goal replacement proof now that automatic work is honestly in scope, or
to measure automatic-family scheduling cost first. Five-goal exact recovery,
the already-approved narrow Essence/Harvest/Fossil product scope, and broader
action-family changes remain outside this completed repair.
