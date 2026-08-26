# Gate 6 — One product-profile authority

**Result: complete.**

The append-only C ABI now exposes `PC_SOLVE_PROFILE_CALCULATOR_PRODUCT_V1` and
a named override mask. Native profile defaults own:

- goal-progress-gated destructive reforges on;
- voluntary economic Restart off;
- generated automatic Imprint programs off;
- high-impact executable-upper work on;
- absolute and relative gap targets disabled; and
- `max_policy_refinement_states = 200000`.

Omitting the profile preserves the historical low-level defaults. A profiled
caller overrides a profile-owned boolean only by setting its override bit and
the corresponding public solver flag; the refinement allowance likewise has
an explicit override bit so zero is representable.

The WASM facade accepts `solve_profile: "calculator_product_v1"`, derives the
override mask from fields actually present in the JSON object, and rejects an
unknown identity. Calculator now sends the profile, positive user gap targets,
and only enabled user toggles. The native and release-WASM benchmark paths
forward the same identity. The generated Gate 0 corpus no longer recreates
the five profile defaults in every case.

Solver telemetry reports `execution.solve_profile.{id,override_mask}` and
compiled strategies retain the same provenance fields.

## Behavior-neutral comparison

The pre-profile explicit-field one-goal control at
`build/performance/solver-quality-gate0-product8-ledger-fixed/cases/conquest-lamellar-allflame-clean-1-goal-product8.json`
was compared with
`build/performance/solver-quality-gate6-profile-one.json`:

| Measure | Explicit fields | Native profile |
| --- | ---: | ---: |
| upper | 62.0877858315596 | 62.0877858315596 |
| lower | 0.520018333333303 | 0.520018333333303 |
| transition hash | `68687872af324ae9` | `68687872af324ae9` |
| policy hash | `c4fef02b7e60af55` | `c4fef02b7e60af55` |
| compiled nodes / edges | 18 / 39 | 18 / 39 |

The profile run reports identity `calculator_product_v1` and override mask
zero. The focused native API control reports a deliberate override mask of 28
and retains a passing exact Eldritch policy with 10,000 simulations.

Native build, profile corpus validation, and the focused solver API suite pass.
Release-WASM and complete cross-layer acceptance belong to Gate 8.
