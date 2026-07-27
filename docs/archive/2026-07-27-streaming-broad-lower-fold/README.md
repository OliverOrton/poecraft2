# Streaming Broad-Lower Fold Falsification

**Status: complete (2026-07-27).** The fixed shadow candidate failed before
publishing a fold, so the broad-kernel direction was rejected for this
milestone and all measurement-only solver source was restored. The prescribed
Gate 3B fallback properness-proof reuse was retained.

Parent: [Archive](../README.md)

- [Final report](report.md)
- [Completed plan](plan.md)
- [Tracked evidence summary](../../../fixtures/solver-scaling/v1/evidence/streaming-broad-lower-fold-summary.json)

The retained versioned proof cache reduced the 30M Dire Pelt owner's 17
start-properness scans to one scan plus 16 validated reuses. Solve wall time
fell from 36.391 seconds to 28.269 seconds with identical bounds, graph/work
counts, termination, and transition/policy hashes. This is a post-incumbent
engineering gain; it does not improve the four hard 11M pre-bound failures.

No mechanic, action scope, cap, public ABI, strategy vocabulary, corpus,
economy, or product presentation changed. The release WASM artifact was
rebuilt because the internal solver path is shared with the browser.
