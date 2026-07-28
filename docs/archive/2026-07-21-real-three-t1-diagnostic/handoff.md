# Real Three-T1 From-Scratch Diagnostic — Final Handoff

The diagnostic is complete. It did not change engine code, mechanics, prices,
WASM, or production caps. The separate fixture and raw reports are indexed by
the [solver-scaling guide](../../../fixtures/solver-scaling/v1/README.md).

The real empty-start case does not close: it reaches the 200,000-state cap with
an infinite executable upper. A 2026-07-27 audit clarified that exact quotient
refinement did not run on this incomplete graph; its equal strict/working
counts do not prove zero merges. A small state-cap increase is not the next
move because rows and reforge work are already close to their caps.

No implementation boundary is active. If Oliver selects a follow-up, the
evidence supports an exact compositional policy-upper/chunking milestone first,
with temporary-bench carrier-signature reuse as the measured action-space
companion. Either change must retain all admitted actions and exact behavior.
