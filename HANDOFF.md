# Session Handoff

**Status: implementation checkpoint complete; product qualification and final
evidence are pending.**

Oliver selected
[Policy-Guided Exact State Refinement](docs/active/policy-guided-exact-refinement.md)
instead of the deferred executable-anchor library. The active plan remains
open. Do not archive it or claim milestone completion until the portfolio,
WASM, and final acceptance gates below pass.

The working branch is `codex/policy-guided-exact-refinement`, created from the
required clean source
`71e1ad05e07949aafe6312fe0e50f49fb685dba3` on
`codex/cross-base-strategy-reliability`. Boundary/plan commit
`69d7a0abda4ddd8809439c09f3c8803fc978c4f1` was made separately. `main` was
not merged or fast-forwarded. The implementation checkpoint is the HEAD commit
containing this handoff; it is local only.

## Implemented Boundary

The native engine now has one versioned, engine-owned action
observation/preservation/destruction contract and one shared refinement
engine. Contract admission occurs before solving. Policy publication:

1. lazily enumerates strict refinements only from the concrete start and
   policy-reachable coarse states;
2. derives minimal observation signatures from admitted action contracts and
   downstream routers;
3. proves selected cost and successor-class probability lumpability to a
   deterministic fixed point;
4. compiles exact class routers when lifting is sound;
5. schedules witness-local exact rows and reuses the existing sparse
   Bellman/policy-improvement machinery when subclasses need different
   decisions; and
6. exact-evaluates and cost-reconciles the retained compiled strategy before
   publication remains available.

Destructive operations collapse unobservable identity; preserving operations
retain only their declared survivor scope. Equal exclusion-effect signatures
remain mergeable even when modifier IDs differ. Fixed-program observed choices
retain their pre-choice observation carrier, so equal offers from different
carriers cannot alias. Compiler routing, exact evaluation, and the final
compatibility assertion consume the same native authority; the frontend only
round-trips engine-authored observation programs.

The exact evaluator also uses shared typed operation resolution for ordinary,
Restart, and Bestiary nodes. Imprint creation, ordinary checkpoint
preservation, restore/consume, Restart clearing, Bestiary prices, and simulator
accounting now reconcile without aliasing an ordinary action descriptor.
Clean destructive-renewal graphs select their pre-discovery carrier from the
generic contract selectors: semantic-strict junk identity is restored as soon
as a router or operation can preserve and observe a fresh explicit's exclusion
identity.

Gate 0 evidence is frozen in
[`policy-guided-exact-refinement-baseline.json`](fixtures/solver-reliability/v1/evidence/policy-guided-exact-refinement-baseline.json).
It records 49 cases, four published policies, 45 without policy, and 36 exact
exclusion refusals: 28 Regal, seven Exalt, and one Restart. It also freezes the
named Regal/Exalt witnesses and the qualified Fracture hashes. The full-four
qualification input is tracked under
[`fixtures/solver-reliability/v1/qualification/fracture-full-four`](fixtures/solver-reliability/v1/qualification/fracture-full-four/README.md).

## Focused Verification Already Run

The optimized native checkpoint binary is
`build/engine/poecraft_engine_refinement_tests.exe`. It was rebuilt after the
last source edit. These final slices are green:

- shared refinement: `296` checks, `0` failures;
- policy lifting/local re-optimization: `4,829` checks, `0` failures;
- Calculator contracts/kernels: `122,041` checks, `0` failures (synthetic
  slice; artifact sub-suite was intentionally not requested);
- compiler/publication, including renewal and Imprint: `750` checks, `0`
  failures;
- focused Imprint: `59` checks, `0` failures; and
- exact evaluator with `data/compiled/current`: `16,783` checks, `0` failures.

`git diff --check` was clean. A read-only architecture audit found no action
name switch, representative modifier selection, incompatible-refinement value
mixing, or incomplete-contract admission in the shared refinement, lifting,
re-optimization, compiler, or exact-evaluation paths.

Not run at this checkpoint: the full solver slice, the real-artifact abstract
slice, native product corpora, the qualified Fracture case, release WASM, web
acceptance, or `scripts/test.ps1`. That work is intentionally handed off below.

## Qualification Handoff

Use the checked-out implementation HEAD. Preserve run output under
`build/acceptance/policy-guided-exact-refinement/`; do not commit transient
build artifacts.

### 1. Remaining native implementation slices

Rebuild through the official path first:

```powershell
powershell -File scripts/build.ps1
```

Then run the remaining focused slices with the produced engine test binary.
The local optimized checkpoint binary may be used directly in this workspace:

```powershell
$Tests = ".\build\engine\poecraft_engine_refinement_tests.exe"
& $Tests --solver-abstract-only data\compiled\current
& $Tests --solver-solve-only
& $Tests --solver-api-only data\compiled\current
& $Tests --solver-s8-3-only
```

`--solver-solve-only` owns the mixed-side rare-cap regression. Do not run the
routine complete test pipeline yet.

### 2. Build the native runtime artifacts

If the local incremental make helper remains available, this is the quickest
equivalent build:

```powershell
C:\msys64\ucrt64\bin\mingw32-make.exe `
  -f build/engine/policy-refinement.mk -j12 runtime-artifacts
```

The helper is ignored build state, not part of the commit. If it is absent,
use `scripts/build.ps1`. Confirm that
`build/engine/poecraft_solver_benchmark.exe` and the engine DLL are from the
implementation HEAD before starting corpora.

### 3. Gate 4 focused product cases

```powershell
$Repo = (Get-Location).Path
$env:PYTHONPATH = "$Repo\tools\ingest;$Repo\bindings\python"
$Exe = "$Repo\build\engine\poecraft_solver_benchmark.exe"
$Artifact = "$Repo\data\compiled\current"
$Corpus = "$Repo\fixtures\solver-reliability\v1\manifest.json"
$Out = "$Repo\build\acceptance\policy-guided-exact-refinement"

py -3 tools/ingest/benchmark_solver_corpus.py `
  --root $Repo --executable $Exe --artifact $Artifact --corpus $Corpus `
  --output "$Out\gate4-focused" --max-workers 1 `
  --watchdog-ceiling-seconds 900 --goal-progress-gated-reforges `
  --case reliability-class-belt `
  --case reliability-class-abyssjewel `
  --case natural-t1-breadth-two-4e65dda9c53b `
  --case reliability-selected-ring-10k `
  --case reliability-selected-gloves-10k
```

`reliability-class-belt` is the frozen one-goal prefix Regal refusal.
`natural-t1-breadth-two-4e65dda9c53b` is the frozen suffix-oriented Exalt
refusal. The frozen final corpus contains no one-goal Exalt refusal: its
one-goal suffix exclusion refusals selected Regal, so
`reliability-class-abyssjewel` covers that product shape separately. Do not
invent or hard-code an Exalt recipe to merge those two qualifications.

Run the qualified Fracture fixture separately:

```powershell
py -3 tools/ingest/benchmark_solver_corpus.py `
  --root $Repo --executable $Exe --artifact $Artifact `
  --corpus "$Repo\fixtures\solver-reliability\v1\qualification\fracture-full-four\manifest.json" `
  --output "$Out\gate4-fracture" --max-workers 1 `
  --watchdog-ceiling-seconds 900 --goal-progress-gated-reforges
```

The focused gate is red if a structurally feasible one-goal case lacks an
exact or bounded executable policy because of coarse exclusion identity. Every
published policy must compile, exact-evaluate, reconcile, and simulate without
an off-policy action.

The Fracture result must retain all frozen values: six parent junk classes,
217 root Chaos successors, 927 discovered/expanded states, zero completed-row
recomputations, transition hash `04a66ba6c6dfcabf`, policy hash
`3e5d7530e7aed5fb`, and compiled strategy SHA-256
`e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`.
Any change is a hard stop: preserve evidence and report instead of forcing it.

### 4. Gate 5 portfolio

Run the 27-class smoke before the complete native portfolio:

```powershell
py -3 tools/ingest/benchmark_solver_corpus.py `
  --root $Repo --executable $Exe --artifact $Artifact --corpus $Corpus `
  --output "$Out\gate5-smoke" --tier smoke --max-workers 1 `
  --watchdog-ceiling-seconds 900 --goal-progress-gated-reforges

py -3 tools/ingest/benchmark_solver_corpus.py `
  --root $Repo --executable $Exe --artifact $Artifact --corpus $Corpus `
  --output "$Out\gate5-native-49" --max-workers 1 `
  --watchdog-ceiling-seconds 900 --goal-progress-gated-reforges
```

Require zero `coarse_parent_requires_exact_exclusion_identity` refusals and an
exact or bounded executable policy for every structurally feasible one-goal
case within the declared limits. The runner performs exact compilation,
evaluation, reconciliation, and ordinary simulation for every published
policy; treat any compile/evaluation/simulation/mismatch classification as a
failure.

Run the selected 10,000-simulation verification cases only after the native
portfolio is otherwise accepted:

```powershell
py -3 tools/ingest/benchmark_solver_corpus.py `
  --root $Repo --executable $Exe --artifact $Artifact --corpus $Corpus `
  --output "$Out\gate5-selected-10k" --max-workers 1 `
  --watchdog-ceiling-seconds 900 --goal-progress-gated-reforges `
  --run-verification `
  --case reliability-selected-ring-10k `
  --case reliability-selected-gloves-10k
```

### 5. Release and final acceptance

Only after the native gates pass:

```powershell
powershell -File scripts/build-wasm.ps1
powershell -File scripts/test.ps1
```

Run `scripts/test.ps1` once, at the end. Codex performs no rendered or visual
review.

## Final Evidence And Closeout

After all qualification passes:

1. record before/after policy availability and refusal counts, including the
   zero exact-exclusion-refusal assertion;
2. use `policy_refinement` telemetry to distinguish cases solved by
   compiler-only lifting from cases that scheduled local rows or changed
   policy/value decisions;
3. preserve all native/WASM report paths and hashes and the 10,000-run results;
4. archive the active plan with a full dated report and update stable evidence
   indexes;
5. update `HANDOFF.md` to a no-active-boundary state; and
6. create the final local qualification/evidence/documentation commit with
   `Co-authored-by: Codex <codex@openai.com>`.

Do not push. Do not start the executable-anchor library or add mechanics.
