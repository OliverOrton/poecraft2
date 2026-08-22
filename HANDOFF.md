# Handoff

**Status: no implementation boundary is active.** Recovery-scoped Restart and
successor-aware bounds are complete. Oliver must select the next chunk before
implementation resumes.

## Current checkpoint

- Branch: `main`.
- Local `origin/main` remains at `c95e6e1`; no push was issued.
- Starting checkpoint: `40eeb87`.
- Native implementation checkpoint: `1e21260`.
- Release-WASM checkpoint: `cfd8904`.
- Completed plan:
  [docs/active/2026-08-22-recovery-scoped-restart-successor-bounds/plan.md](docs/active/2026-08-22-recovery-scoped-restart-successor-bounds/plan.md)
- Accepted result:
  [docs/active/2026-08-22-recovery-scoped-restart-successor-bounds/result.md](docs/active/2026-08-22-recovery-scoped-restart-successor-bounds/result.md)

## Completed boundary

Calculator-default Solve scope now excludes voluntary discard-and-buy-new-base.
The UI has an unchecked explicit opt-in, while native/explicit callers retain
the historical unrestricted default. No ordinary Bellman state receives a
Restart row in restricted mode, and unmatched bounded compilation fails
closed.

Product-local Fracture replacement is independent of that action scope. A
miss still pays `base`, reaches a fresh Normal carrier, and compiles the
dedicated `product_fracture_restart` retry operation. Do not generalize this
authority to Influence Exalt or another miss: the Warlord control continues on
the influenced carrier with Harvest Reforge Fire and requires no replacement.

Operator lowers now carry only source goal slots retained by at least one
proved runtime execution path, then add all possibly reached slots. Sequential
refinement contracts are the authority. Exact reset removes progress;
incomplete semantics and may-destroy selectors retain it. The universal and
proved shape-aware state covers combine by maximum, never replacement. The
strict rare-carrier cover remains excluded on Eldritch-eligible sessions until
it models automatic side options. The reported concrete-rare ordering bug was
stale and current source already counts both sides correctly.

Restricted proper-policy initialization now permits stochastic retry SCCs when
no executable incumbent exists. The deterministic seed is evaluated by the
existing exact SCC solver and never gains publication authority by itself.
Historical unrestricted initialization remains unchanged.

## Measured result

The checked native Allflame four-natural-T1 Conquest Lamellar primary stopped
at the pre-existing `max_imprint_program_work` cap after 47.789 seconds, not at
the watchdog. It retained a certified `21.772459401332767` lower and an
independently evaluated `3759.9763122101763` upper. The 87-node / 241-edge
strategy passed exact evaluation and 10,000/10,000 simulations with empirical
mean `3737.4451776349074`. It has zero ordinary Restart rows and one Restart
operation, used only for Product Fracture miss recovery.

The Warlord control closes exactly at `224.1238588972487` in 1.930 seconds. Its
8-node / 13-edge strategy uses Influence Exalt plus Harvest Reforge Fire,
contains no Restart, exact-evaluates at the solver value, and passed
10,000/10,000 simulations with empirical mean `223.95892804999892`.

## Acceptance

- `powershell -File scripts/build.ps1`: pass.
- Native solver suite: 96,543 checks, zero failures.
- Native compiler suite: 840 checks, zero failures.
- Native API suite: 2,978 checks, zero failures.
- Native Calculator suite: 436,308 checks, zero failures.
- Native refinement suite: 362 checks, zero failures.
- Release `powershell -File scripts/build-wasm.ps1`: pass.
- `npx tsc --noEmit`: pass.
- Complete web tests and 28/28 release-WASM engine smoke checks: pass.
- Full repository pipeline: deliberately not run.
- Rendered/visual review: deliberately not run; it remains Oliver's.

## Possible successors

No successor is selected. The closest evidence-backed choices are:

1. **Finite Imprint-program closure.** The primary now completes well inside
   its watchdog and publishes a verified bounded policy, but exact closure
   stops at `max_imprint_program_work`.
2. **Exact alternative/refinement performance.** Continue the retained
   persistent quotient owner after a wider Imprint closure if that closure
   exposes a downstream plateau.
3. **Executable-upper scheduling.** The earlier Essence/Harvest/Fossil early-
   incumbent issue remains separate from lower-bound proof strength.
4. **Goal-slot row equivalence.** The measured temporary-bench duplicate rows
   remain a separate behavior-preserving size/proof opportunity.

## Retained architecture

Production strict refinement owns one durable quotient session across
competitive frontier growth. The strict oracle, split-only partition, Bellman
graph, proof store, alternative obligations, published rows, and independently
evaluated incumbent grow in place. Source/target splits invalidate dependent
proof authority through stable generations and reverse indexes. Do not replace
this with the rejected cross-generation carrier-row cache or another unbounded
replay cache.

The retained implementation spans the local checkpoints `18e4640` through
`bb29378`, with release qualification through `9d447f5`. The prior accepted
persistent-quotient result remains at
[docs/active/2026-08-21-persistent-quotient-session/result.md](docs/active/2026-08-21-persistent-quotient-session/result.md).

Five-T1 recovery, broader compiler/router work, mechanics, prices, and action
admission remain separate unless Oliver explicitly selects them.
