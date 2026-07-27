# Session Handoff

**Status: no active implementation boundary.** Oliver must choose the next
chunk before implementation resumes.

## Latest completed result

The 2026-07-27
[Protected-Core Compression Ceiling Census](docs/archive/2026-07-27-protected-core-ceiling-census/README.md)
rejected cleanup as the next pre-bound solver architecture.

On the four frozen full/deep three-/four-target hard cases, impossible free
prefix/suffix erasure showed a large raw all-state ceiling (`55.5x` to
`904.8x`). The cleanup-relevant population did not: nonterminal states with at
least two satisfied goals on one protected side were only `0.14%` to `5.68%`
of the cap-censored observed stream. Even deleting every one of those states
for free cannot create material headroom at the first-row 200,000-state
failure.

The large all-state signal is not discarded, but it is a different question:
an unselected action-local side-factorization or source-elimination study. It
would require a complete continuation-observation contract, complete or
formally bounded kernel coverage, and counterexamples for every erased
feature. The census does not prove state equivalence, authorize pruning, or
select a cleanup mechanic.

The
[tracked evidence](fixtures/solver-natural-t1/v1/evidence/protected-core-ceiling-summary.json)
pins the four case hashes, projection counts, deterministic work, measurement
cost, and behavior parity. All measurement-only engine/test source was
restored. No mechanics, actions, goals, conditions, caps, public ABI,
compiled artifact, binding, WASM, web, or product behavior changed.

The preceding completed
[Streaming Broad-Lower Fold Falsification](docs/archive/2026-07-27-streaming-broad-lower-fold/README.md)
remains the latest production solver change: versioned solve-local successful
fallback properness-proof reuse cut its owner solve wall by 22.3% with
identical deterministic results. It does not improve the hard pre-bound
failures.

Nothing is pushed.

Commits must remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
