# Exact Quotient Audit

**Status: completed on 2026-07-27.** The audit found no production quotient
defect, added the missing positive-merge and shadow-only regressions, justified
literal observation-choice identity, and corrected historical cap-censored
claims.

Parent: [Milestone archive](README.md)

## Objective

Establish that the exact outer-state quotient has a live, regression-protected
merge path on a completed graph; determine whether raw observation-choice
modifier identity is a necessary executable-policy distinction; and correct
any claim that treats an incomplete shadow partition as a completed exact
quotient.

This is a falsification-first audit. It does not assume that the quotient is
broken, that choice modifier identity is removable, or that the four hard
natural-T1 cases contain useful exact merges.

## Fixed contract

- Strict `AbstractState` identity remains the mechanic-correct oracle.
- Exact quotienting may merge two completed strict states only when every
  admitted action has identical legality, resources, probabilities,
  observe-then-decide behavior, and projected successor classes.
- A compiled policy must remain executable on every concrete state represented
  by a quotient class. Value equivalence alone is insufficient if a concrete
  observation branch can no longer be selected.
- Incomplete graphs do not prove exact equivalence or non-equivalence for
  unexpanded states. Any diagnostic partition over such a graph is explicitly
  shadow-only.
- `choice.mod_id` is treated as a potentially observable Unveil output until a
  completed witness proves that a projection preserves both Bellman behavior
  and compiled execution.
- A strict/quotient count equality at a resource cap is not accepted as
  evidence of zero exact merges unless actual completed refinement ran.
- No Path of Exile mechanic, action scope, goal condition, economy, solver cap,
  public ABI, compiled artifact, binding, WASM, web, GPU, or ML behavior is in
  scope.
- The uncapped true first-reforge successor count and bounded approximate
  factorization remain separate research questions. This audit does not raise
  caps to answer them.
- Commits remain local-only and end with
  `Co-authored-by: Codex <codex@openai.com>`.

## Change surface

Primary authority:

- `engine/src/solver_solve_quotient.cpp` for completed refinement, shadow
  diagnostics, row observation signatures, and exact representative mapping;
- `engine/tests/test_solver_solve.cpp` and, only if compilation is implicated,
  `engine/tests/test_solver_compile.cpp` for completed merge, parity,
  observation-choice, and incomplete-shadow regressions.

Documentation/evidence:

- `docs/solver/README.md`, `docs/evidence.md`, and
  `fixtures/solver-scaling/v1/README.md` for the durable distinction between
  completed quotienting and incomplete shadow diagnostics;
- a versioned summary under `fixtures/solver-scaling/v1/evidence/`; and
- this plan, the active index, and `HANDOFF.md`.

Under the [change-impact map](../../foundation/change-impact.md), a production
solver-algorithm change with stable request/output requires a native build and
focused solver acceptance. A WASM rebuild or web acceptance is required only
if browser-visible production behavior survives the audit. Documentation and
test-only outcomes do not trigger it.

## Gate 0 - Freeze semantics and locate the live paths

**Complete.** Completed refinement, literal shadow grouping, diagnostic
cardinality, representative lifting, and compiler routing were traced.

1. Trace completed and incomplete control flow through quotient refinement,
   shadow signatures, representative construction, Bellman consumption, and
   compiler lifting.
2. Enumerate every literal state or modifier ID retained in:
   - completed row observation cache keys;
   - completed behavioral signatures;
   - incomplete shadow signatures; and
   - diagnostic cardinality signatures.
3. Classify each identity as:
   - required mechanic/action observation;
   - required executable-policy output;
   - implementation-only cache identity; or
   - unproven over-distinction.
4. Freeze terminology:
   - **exact quotient** means stable refinement of a completed strict graph;
   - **shadow class** means a non-authoritative diagnostic grouping; and
   - **strict/quotient equality** on an incomplete run means no quotient was
     applied, not that refinement proved every state distinct.

**Kill condition:** if the alleged raw-ID defect is only in non-authoritative
diagnostics, do not change production quotient semantics to make a diagnostic
look smaller.

## Gate 1 - Completed positive merge witness

**Complete.** The fast alt-spam oracle reduces 10 strict states to 3 exact
classes with 7 non-identity representatives, value/action parity, and zero
signature mismatches.

1. Add a small deterministic solve that closes its strict graph and has at
   least two strict nonterminal states that all admitted actions provably
   cannot distinguish.
2. Run strict and quotient modes over the identical session, goal, economy,
   start, and action envelope.
3. Require:
   - both solves complete and converge;
   - `strict_states > quotient_states`;
   - a non-identity representative mapping exists;
   - start values and selected start actions agree;
   - zero observation-signature mismatches; and
   - lifted exact kernels agree under the existing oracle.
4. Make the witness fail loudly if future changes silently disable all merges.

The historical 57,722-to-3 Chaos corpus remains real evidence, but this gate
requires a fast native regression rather than relying only on an archived
benchmark report.

## Gate 2 - Observation-choice identity audit

**Complete.** Eight literal offered modifiers map into projected successor
classes, including seven offers in one class. Compilation still emits every
literal offer as both a condition and operation, so `choice.mod_id` remains
required.

1. Construct the smallest completed observe-then-decide witness whose offered
   modifier IDs differ while projected Bellman successors are otherwise
   equivalent.
2. Trace the modifier identity through:
   - row grouping and partition refinement;
   - Bellman choice evaluation;
   - quotient representative materialization;
   - policy compilation; and
   - compiled observation matching.
3. Compare two shadow interpretations:
   - current literal modifier observation; and
   - projected successor behavior without literal modifier identity.
4. Retain raw identity if removing it would make one representative's compiled
   choice branch invalid for another concrete member. Change production
   semantics only if a completed counterexample proves the raw field is
   behaviorally redundant and compilation remains exact.

**Acceptance condition:** the result is a regression-protected explanation,
not a smaller class count by itself.

## Gate 3 - Incomplete-run truthfulness

**Complete.** A cap-stopped regression pins shadow-only reporting. Historical
real-three-T1 and four-hard-case prose was corrected: no completed quotient ran
on those unfinished graphs.

1. Add a deliberately cap-stopped native witness and assert that it reports:
   - `shadow_only = true`;
   - no behavioral representative map;
   - strict state count retained as the working quotient count; and
   - shadow class counts separately from exact merges.
2. Trace the four hard natural-T1 evidence records without rerunning them.
   Identify which completed exact-quotient fields and which incomplete shadow
   fields were actually produced.
3. Correct stable documentation that currently describes cap-equal
   strict/quotient counts as proof of zero merges.
4. Do not relabel a shadow reduction as an exact reduction, and do not infer
   the unobserved completed quotient.

## Gate 4 - Decision and focused acceptance

**Complete.** No production algorithm changed. Source comments and focused
regressions remain; the native solver-solve and solver-compile suites pass.

1. If Gates 1-3 find a production quotient defect, implement the smallest
   exact fix and run strict/quotient value, policy, kernel-lifting, and compiler
   parity as affected.
2. Otherwise retain only the positive merge, choice-observation, and
   incomplete-shadow regressions plus corrected documentation.
3. Run one final affected native build/test gate. Do not run the full
   repository pipeline; this audit does not touch mechanics, data, ABI,
   bindings, WASM, or web unless an unexpected production contract change
   expands scope.
4. Store a versioned evidence summary with source commit, build/compiler,
   witness identities, deterministic counts, result, and test command.
5. Extract lasting facts into stable solver/evidence documentation, archive
   this plan, clear the active boundary, update `HANDOFF.md`, and create one
   local completion commit.

## Completion criteria

This milestone completes only when:

- a fast completed test proves the exact quotient can strictly reduce a graph;
- raw observation-choice identity is either justified by executable behavior
  or removed with completed compilation parity;
- incomplete shadow diagnostics cannot be mistaken for a completed quotient
  in tests or stable documentation;
- any retained production change preserves strict-oracle exactness; and
- the result is committed locally with a clean worktree.
