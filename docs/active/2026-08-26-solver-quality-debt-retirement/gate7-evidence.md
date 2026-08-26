# Gate 7 — Measured debt retirement and ownership refactor

**Result: complete; limited to touched authority debt.**

The retained refactor is the removal of duplicated product-default ownership:

- one native profile now owns the Calculator behavior bundle;
- TypeScript presentation code owns only user intent and positive gap inputs;
- WASM owns JSON-to-C-ABI presence/override translation;
- native and release-WASM benchmark runners consume the same identity; and
- fixture cases carry the profile identity instead of inverse booleans and a
  copied refinement allowance.

The high-impact scheduler capability now has a public durable flag. Its former
high diagnostic bit remains accepted only as a compatibility alias, and the
native-only option header no longer claims that normal Calculator requests use
that private bridge.

No scheduler skeleton, automatic-admission cache, reforge evaluator, or broad
refinement component was removed. Gate 3 showed the existing warm-start
compatibility path was ineffective on one witness, not that it was dead for
every supported start. Removing it under that evidence would exceed this
gate's behavior-preservation contract. The rejected experiment remains
documented instead of becoming cleanup by assertion.
