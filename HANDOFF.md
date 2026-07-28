# Session Handoff

**Status: Gate 0 active for the pre-expansion probability-lower audit.**

Plan: [Pre-Expansion Probability-Lower Audit](docs/active/plan.md)

Branch: `codex/pre-expansion-probability-lower-audit`

Starting source: `8976750` (`main`)

## Current Boundary

Test whether a probability-only action-conditioned relaxation can separate one
of the archived exact renewal uppers before the frozen hard cases attempt a
broad row.

The current clean-goal preparation is not graph-free: it calls
`CalcContext::outcomes` for destructive actions while constructing exact
relaxed envelopes. The new measurement must use immutable pool/descriptor
facts only and must leave states, rows, transitions, reforge work, scheduling,
values, policies, termination, and caps unchanged.

## Immediate Next Step

Finish the Gate 0/1 code and proof audit, define the isolated probability-only
data flow, and validate it on small exact oracles. Then add only the shadow
telemetry needed to measure full root-action-class lower tables.

Run the five frozen product-cap cases only after the small oracle proves both
admissibility and zero state/work growth. Proceed beyond measurement only if a
previously refused hard case strictly separates its archived exact renewal
upper over the complete product action envelope.

## Standing Boundaries

- A finite but non-separating bracket is not a pass.
- Do not call exact outcome generation from the candidate.
- Do not raise caps or restrict the action envelope.
- Unknown/unpriced/unsupported/deferred members block certification.
- Do not add public, ABI, WASM, worker, or product surfaces in this pass.
- Commits remain local unless Oliver says to push and end with:

`Co-authored-by: Codex <codex@openai.com>`
