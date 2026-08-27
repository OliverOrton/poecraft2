# Exact Same-Side Closure Result

**Status: complete.** Completed 2026-08-27 on `main`, local-only. Engine
behavior checkpoint: `7ed9f17`.

Parent: [Exact Same-Side Closure](README.md)

## Outcome

Current Calculator-product solves now close exactly for clean items requesting
three prefixes or three suffixes under the junk-free terminal contract. Both
directions publish `exact / exact_closed`; the certified lower, solver value,
and independently evaluated compiled-graph cost are equal.

| Witness | Exact cost | Solve wall | Graph | Simulator |
| --- | ---: | ---: | ---: | ---: |
| Conquest Lamellar, three prefixes | `1618.2138946963837` | 78.553 s | 154 nodes / 432 edges | 10,000 / 10,000 success |
| Conquest Lamellar, three suffixes | `1101.15648683309` | 48.836 s | 78 nodes / 219 edges | 10,000 / 10,000 success |

The final repetitions used seed `20260827`. Neither recorded a Simulator
failure, stop, action-limit event, inapplicable action, missing edge, missing
price, or off-policy event. Exact graph evaluation proved success probability
one, zero off-policy mass, complete pricing, and exact cost reconciliation.
The preceding final-source repetitions had identical values, policy hashes,
graph hashes, node counts, and edge counts.

## Repairs

- `b8c0c36` treats a newly discovered state without a materialized sparse row
  as an ordinary frontier condition during reachable-incumbent lift instead
  of indexing beyond the transition cache.
- `42af57f` closes a fully finite coarse policy through the strict quotient
  without requiring an unrelated speculative upper first.
- `7918379` prioritizes closed exact proof over redundant fallback assertion.
- `7ed9f17` uses an independently evaluated fallback as rollback-upper
  authority during strict alternative accounting, defers the redundant
  interim compiled assertion, and includes downstream observation features in
  persistent policy-collapse identity. The latter removes the PDR frontier's
  false `collision-free strict state identity changed` failure.

No lower, restricted value, or unverified policy was promoted to exactness.
The rollback seed remains executable-upper authority only; final exact
publication still compiles and independently evaluates the selected strict
policy.

## Gate 4 Boundary

The PDR four-mod probe no longer throws the original vector range exception or
the later policy-collapse collision. It reaches a second strict frontier, but
then consumes the 50,000,000 logical reforge-work allowance while certifying
broad alternative rows. The measured first frontier had 13,933 quotient
classes and discovered 4,907 additional strict carriers. A retained direct
compiled policy exact-evaluated at `7866.432124027084`, but the bounded probe's
external 300-second watchdog expired before publication.

A solve-scoped experiment transferred the coarse operator lower into all
299,394 strict obligations. It produced zero noncompetitive obligations and
still consumed the full reforge-work allowance, so it was removed. The next
exactness owner is therefore not the coarse/strict plumbing or the existing
goal-cover lower. Four-mod cross-side closure needs either a materially
stronger successor-aware carrier/action lower with an admissibility proof, or
cheaper resumable proof for broad destructive rows. It should not repeat the
discarded immediate/current-cover transfer.

## Acceptance

- fresh native build: pass;
- policy-refinement tests: 2,083 checks, zero failures;
- solver solve: 100,169 checks, zero failures;
- exact evaluator: 18,086 checks, zero failures;
- solver API: 2,629 checks, zero failures;
- solver calc: 436,640 checks, zero failures;
- strategy compiler: 583 checks, zero failures;
- release WASM rebuild: pass;
- nonvisual web/WASM suite: pass, including 28/28 worker smoke tests;
- `npx tsc --noEmit`: pass;
- final fixture expectations and exact graph evaluation: pass;
- `git diff --check`: pass.

The plan explicitly excluded the full repository pipeline, broad benchmark
matrices, five-goal tuning, and rendered browser review; none was run or
claimed.

Raw local reports are under `build/performance/` with prefixes
`native-solver-exact-same-side-current-`,
`native-solver-exact-same-side-final-repeat2-`, and
`native-solver-exact-same-side-feature-key-pdr-`. Build outputs are local
evidence and are not committed.
