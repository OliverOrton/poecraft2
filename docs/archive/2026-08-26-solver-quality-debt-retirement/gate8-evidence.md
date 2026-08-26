# Gate 8 — Acceptance and handoff

**Status: complete on 2026-08-26.** Final source is `f883a6a` plus the
documentation/archive checkpoint containing this record. Oliver-owned rendered
Calculator review remains explicitly unclaimed.

## Current product-quality boundary

The complete 18-case Calculator-profile ladder is recorded under
`build/performance/solver-quality-gate8-product-profile/`; the nine-case
1024-work control is under
`build/performance/solver-quality-gate8-qualification1024/`.

- All 18 product cases were recorded with no survivor processes. The known
  partial 3-to-4 case hit its 60-second watchdog and produced a partial
  observation. Seventeen cases completed normally.
- Every completed finite policy passed independent exact evaluation.
- Three old fixture expectations remain stale: Amethyst Ring clean two/three
  have no policy at their truthful resource caps, and Conquest clean two has a
  bounded exact-evaluated policy with zero open obligations while the fixture
  still requires open obligations. The predecessor Gate 0 reports fail those
  same assertions; they are not profile regressions.
- Eight of nine qualification cases completed with every authored expectation
  passing; the same partial 3-to-4 case hit its authored watchdog.
- Product-eight and qualification-1024 values remain identical on their
  overlapping completed cases.

Current selected costs remain:

| Case | Certified lower | Exact-evaluated upper |
| --- | ---: | ---: |
| Conquest clean four | 21.7725 | 8,501.69 |
| Conquest clean five | 36.4885317287664 | 14,454,067.4260706 |
| Conquest partial three-to-five | 36.4885 | 80,720.79 |
| Conquest partial four-to-five | 36.4885 | 7,896,721.25 |
| Conquest fractured four-to-five | 36.4286171891 | 2,698.87479601436 |
| Bow clean four | 212.39 | 223,349.00 |
| Bow clean five | 212.39 | 17,073,119,024.23 |

The historical proper 87,361-chaos five-T1 strategy remains replayable
observational evidence, not current publication authority. Gates 3-5 did not
qualify a new action scope, incumbent consumer, or proof extension, so this
milestone does not claim a five-goal quality improvement.

## Native and release-WASM acceptance

The retained 19-case control matrix is recorded at:

- `build/performance/native-solver-solver-quality-gate8-profile-acceptance-v1.json`;
- `build/performance/wasm-worker-solver-solver-quality-gate8-profile-acceptance-v1.json`;
- `build/performance/solver-solver-quality-gate8-profile-acceptance-v1-comparison.json`.

All 19 authored expectations and independent exact evaluations pass in both
runners. Oracle one-mod and explicit Imprint each complete exactly 10,000
successful simulator runs per runner. No WASM watchdog expires. The comparison
passes 180 checks across its two selected exact controls with zero mismatch.

The explicit Calculator-profile identity reports are:

- `build/performance/native-solver-solver-quality-gate8-calculator-profile-identity-case-conquest-lamellar-allflame-clean-1-goal-product8-v1.json`;
- `build/performance/wasm-worker-solver-solver-quality-gate8-calculator-profile-identity-case-conquest-lamellar-allflame-clean-1-goal-product8-v1.json`.

Both report profile `calculator_product_v1`, override mask zero, upper
`62.0877858315596`, lower `0.520018333333303`, transition hash
`68687872af324ae9`, policy hash `c4fef02b7e60af55`, and graph 18 / 39. The
generic comparison helper intentionally selects exact controls only, so this
bounded case was compared directly rather than represented as a passing
helper comparison.

## Test acceptance

- Native build and release-WASM build pass. The release module was rebuilt
  after the C ABI and facade changes.
- Focused native API, compile, policy-refinement, refinement, and quotient-
  proof suites pass 6,253 checks with zero failures.
- Complete nonvisual web tests pass, including 28/28 WASM engine-smoke checks;
  `npx tsc --noEmit` passes.
- `git diff --check` passes.

The warranted full `scripts/test.ps1` run passes 18 ingest tests, 12 economy
tests, canonical DB validation, fixture parity, compiled-artifact validation,
and all 17 Python binding tests. It then reaches the repository's documented
pre-existing broad native-solve failure: 13 stale goal-progress-gated
expectations followed by an empty-JSON parse exception. Gate 2 reproduced this
on both current source and an untouched historical worktree. It is not caused
by the profile change and remains a separate fixture/test-harness cleanup
boundary; no acceptance result here conceals it.

## Remaining boundary

The solver is stable and truthfully measured, but current zero-to-five quality
is still weak. The next selected work, if any, should be chosen from the
remaining upper-discovery/consumption and successor-complete rare/Eldritch
proof opportunities; no implementation boundary is active after archival.
