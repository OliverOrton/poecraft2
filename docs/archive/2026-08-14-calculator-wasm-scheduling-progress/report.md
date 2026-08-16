# Calculator WASM Scheduling And Selected-Policy Refinement Report

**Status: completed and archived on 2026-08-16.**

This milestone began with a normal Calculator three-prefix solve that appeared
stalled at 1,263 expanded states and a zero lower bound, then remained inside a
blocking finalization call. It ends with native-owned cooperative progress
through refinement, compilation, and certification, plus useful selected-
policy publication for Oliver's representative from-empty five-natural-T1
Conquest Lamellar.

The key result is strategy quality, not another fallback. The five-T1 product
request now returns a 184-node/666-edge strategy exact-evaluated at
`624800.9519118543`, about 59.7 times cheaper than the old Chaos renewal. It
finishes in about 14.25 seconds in the eight-item release-WASM profile with a
185.229 ms maximum step. Its lower remains zero and its termination remains
`numerical_stability` because the full action envelope is still open.

Exact finalization is no longer hidden behind native `Done`. Retained exact
lift and graph assertion work advance under the stepped solve API, expose
append-only `refining`, `compiling`, and `certifying` phases, and remain
abandonable. `finish()` transfers an already finalized result. The exact
three-prefix and three-prefix-plus-spell-suppression witnesses retain their
values and hashes with 164.756 ms and 238.509 ms maximum steps.

Proof authority remains strict: a selected coarse policy is stored in a
separate unverified type, becomes portfolio-eligible only after independent
exact execution checks, and competes with the certified fallback only by exact
evaluated cost. No mechanic, price, goal, action scope, cap, objective,
tolerance, or Simulator horizon changed.

The retained four-natural-T1, Eldritch, Warlord, Imprint, and three-prefix
controls passed their exact graph checks and exactly 10,000 Simulator trials
with zero failures, cap hits, or off-policy executions. The five-T1 policy is
too long for meaningful sampling under the unchanged 100,000-action limit, so
its independent exact graph evaluation is the acceptance authority.

The one full repository acceptance pipeline passed. Detailed identities,
hashes, timings, control values, the Emscripten 6.0.0 LTO workaround, and the
disclosed benchmark expectation-vocabulary mismatch are in
[final acceptance evidence](evidence/final-acceptance.md).

No implementation boundary remains selected. Oliver must choose the next
milestone before source work resumes.
