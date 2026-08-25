# Exact-Goal Carrier Ladder

**Status: complete (2026-08-23).** See the
[accepted result](result.md).

Parent: [documentation map](../../README.md)

## Objective

Make every solver goal an exact explicit-affix target and recover practical
from-empty multi-goal planning through carrier-aware progress. A successful
item contains enough requested goal modifiers at the requested tier and no
other explicit modifiers. Empty affix slots are allowed. Junk, crafted
blockers, and metamods remain legal intermediate state and are removed only
when doing so improves a later action or is required for terminal success.

## Fixed boundaries

- Exactness applies to every goal size. `min_satisfied_slots` retains its
  threshold meaning, but every explicit affix on a successful item must be one
  of the satisfied requested slots.
- Implicits are outside the explicit-affix exactness test.
- Intermediate junk is not failure and is not cleaned merely because it
  exists. A later action may preserve, destroy, or replace it according to the
  engine-owned action contract.
- Carrier planning may jump directly between any feasible goal subsets; it
  must not force a fixed goal order or require a 3 -> 4 -> 5 sequence when one
  action can produce 3 -> 5.
- Any carrier planner is executable-upper authority only unless a separate
  admissibility proof justifies lower-bound or pruning authority.
- Do not hardcode an item, Harvest tag, action sequence, or cleanup rule.
- Run one final changed-layer acceptance. Do not run the full repository
  pipeline unless Oliver separately selects it as a merge gate.

## Work plan

1. Centralize exact terminal satisfaction in native solver authority and make
   every compiler/evaluator route use the same predicate and explicit-count
   condition.
2. Update clean-carrier and strict goal-cover terminal shapes so admissible
   lower bounds do not mistake a goal-plus-junk carrier for success.
3. Add carrier-aware focused scheduling over feasible goal subsets and actual
   structural state: side occupancy, protection, fracture, and disposable
   junk. Preserve direct multi-goal transitions.
4. Derive cleanup value from real successor effects. Verify a protected-side
   reforge can consume a dirty opposite side without preliminary cleanup, and
   retain a counterexample where cleanup is genuinely useful.
5. Measure the real clean Conquest Lamellar zero-to-five request, preserve the
   best independently evaluated executable strategy, and qualify native/WASM
   behavior proportionally.

## Acceptance

- Goal-plus-junk is nonterminal for every goal size; exact goals with empty
  remaining slots are terminal.
- Temporary blockers and junk may appear on intermediate policy paths, while
  compiled success routing rejects them at the terminal boundary.
- Focused scheduling distinguishes useful dirty carriers from irrelevant
  cleanup without action-name special cases and permits direct subset jumps.
- The exact protected-prefix/tagged-suffix regression skips wasteful cleanup;
  a paired obstruction regression still chooses cleanup when it changes the
  useful continuation.
- The real from-empty five-T1 diagnostic records first executable incumbent,
  upper trajectory, state/row/work/memory census, action composition, and the
  precise stop owner.
- Native build and focused solver/compiler tests pass; release WASM, web tests,
  TypeScript, and `git diff --check` pass. Any acceptance strategy is exact-
  evaluated and simulated 10,000 times.
- `HANDOFF.md` records the result and limitations. Commits remain local and
  carry `Co-authored-by: Codex <codex@openai.com>`.
