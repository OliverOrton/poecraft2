# Verified Executable Graph-Fragment Core v1

**Status: selected; implementation has not begun.** Selected by Oliver on
2026-08-28 from clean stable checkpoint
`031d528709b6fb3bc8563c16231042382a945c07`.

Parent: [Active work](../README.md)

Progress and exact evidence belong in the [execution log](execution-log.md).

## Objective

Build the smallest exact executable graph-fragment foundation that cannot
repeat the removed carrier planner's authority or probability-mass failures.
A proposal may describe finite control flow, but only a private verifier may
materialize primitive transition probabilities from engine-owned mechanics.
The verifier must prove complete exit mass, absorption, and finite expected
resource use. One verified leaf fragment must then flatten into the existing
ordinary strategy JSON vocabulary and independently pass the existing
compiler/evaluator.

Version 1 remains an isolated shadow diagnostic. It must not change the core
solver graph, scheduler, action-envelope ledger, proof store, public lower,
incumbent portfolio, published policy, product defaults, or release-WASM
behavior.

## Inherited Qualification Wording

The immediately preceding Native Solver Lab Unattended Execution and Identity
Hardening boundary completed with an owner-approved qualification exception:
its six-hour soak was **owner-waived, not passed**. It is not overnight
qualification and must never be cited as such. This plan may use the Lab's
typed operator surface and its passing focused/full regression evidence, but
it does not inherit or imply sustained overnight evidence.

## Mandatory Fresh-Task Startup Gate

Do not implement this plan in the task that selected it. Start a fresh Codex
task so the configured `poecraft2-native-solver-lab` MCP server is loaded from
task startup.

Before editing anything, the fresh task must:

1. Read, in order:

   - `AGENTS.md`;
   - `docs/README.md`;
   - `docs/direction.md`;
   - `HANDOFF.md`;
   - this plan; and
   - this boundary's `execution-log.md`.

2. Verify the source boundary and worktree:

   ```powershell
   git status --short
   git rev-parse HEAD
   git rev-parse HEAD^
   git show --stat --oneline --decorate HEAD
   ```

   The tree must be clean. `HEAD^` must be exactly
   `031d528709b6fb3bc8563c16231042382a945c07`. `HEAD` must be the single
   documentation-only commit titled
   `Activate verified executable graph fragment core v1`, changing only:

   - `HANDOFF.md`;
   - `docs/README.md`;
   - `docs/active/README.md`;
   - this `plan.md`; and
   - this boundary's `execution-log.md`.

   Record the full planning `HEAD` in the execution log before implementation.
   Stop if the parent, subject, changed paths, branch, or clean-tree condition
   differs. Do not reset, clean, restore, or discard anything to manufacture
   the expected state.

3. Confirm the configured Solver Lab MCP server is available as version
   `0.2.0` with 31 bounded typed tools. If tools are absent, inspect the
   user-local registration with
   `codex mcp get poecraft2-native-solver-lab`, then restart Codex or open
   another fresh task. Do not implement in a task that cannot call the
   configured server, and do not change its registration as part of this
   boundary.

4. Use MCP, without mutation, to record:

   - profiles;
   - frozen cases;
   - immutable case revisions;
   - supervisor/dispatcher ownership;
   - bounded jobs; and
   - bounded attempts.

   A verified-live different dispatcher owner may leave the fresh server in
   `control_only`; record it and continue only if the existing owner is
   legitimate and operator actions remain available. Never force-clear
   ownership. Do not open the GUI.

5. Keep authority separated for the whole boundary:

   - MCP is the operator-level surface for case discovery, immutable
     submission, monitoring, cancellation, comparison, and evidence export.
   - Repository tools (`rg`, PowerShell, `apply_patch`, native builds/tests,
     Python tests, and Git) inspect and edit source and run tests.
   - Do not inspect or mutate the Lab catalog with ad hoc SQL or filesystem
     edits to substitute for an MCP operation.
   - MCP must not gain arbitrary shell, SQL, path-write, mechanic, probability,
     fragment-IR, or untyped native-argument authority.

6. Read `docs/foundation/change-impact.md`, revalidate the source owners named
   below against the actual planning `HEAD`, and record any changed owner
   before implementation. Archived audits are design evidence, not current
   implementation authority.

The fresh task must stop immediately rather than modifying or cleaning the
tree if any mandatory startup condition differs.

## Source-Confirmed Starting Owners And Historical Failure

Revalidate these at Gate 0:

- `engine/src/solver_executable_carrier_planner.hpp` retains only
  `ExecutableCarrierProjection` and `ExecutableCarrierActionProjection` as
  non-convertible diagnostic vocabulary. It has no proof, pruning, policy,
  probability, terminal, or incumbent authority.
- Existing finite programs are `FixedOptionSpec` in `solver_model.hpp` and
  exact `OptionKernel` rows in `solver_options*.cpp`. Neither is a standalone
  verified executable fragment or an exit/absorption certificate.
- Engine-owned exact transition construction lives in `solver_calc.cpp`,
  `solver_reforge.cpp`, `solver_options.cpp`, and the refinement transition
  owners. Exact state identity and policy evaluation live in
  `solver_refinement*.cpp` and `solver_refinement_eval.cpp`.
- Ordinary strategy serialization is owned by `solver_compile.cpp`; fresh
  strategy parsing is owned by `simulator.cpp`; exact compiled-strategy
  evaluation and reporting are owned by `solver_eval*.cpp`; final exact
  assertion is owned by `solver_policy_assertion*.cpp`.
- Existing upper authority remains
  `SolveDiagnostics::BoundedPolicyIncumbent` and `IncumbentPortfolio` in
  `solver_solve_types.hpp`. Lower/proof authority remains in the action-ledger,
  proof-pattern, quotient, and solve-bound owners.
- The removed executable-carrier planner produced attractive coarse estimates
  `12365.392875058` and `12197.277488393`, but independent compilation rejected
  probability-mass conservation. A later clean-case policy was caused by core
  graph growth and was not planner credit. Production planner behavior was
  removed; do not restore or source-copy it.
- Existing exact evaluators already reject transition-sum mismatches and
  reachable closed nonterminal SCCs, expose mass-conservation error, and
  compare cyclic strategies with a forward reference.

If a current owner or historical classification differs, update the plan and
log before implementing. Do not preserve a stale source diagnosis merely
because it appears here.

## Locked Authority Firewall

The implementation may refine names during Gate 0, but it must retain this
non-convertible authority ladder:

| Stage | Required type | Allowed numeric data | Authority |
| --- | --- | --- | --- |
| proposal | `ExecutableFragmentProposalV1` | heuristic estimate and ranking annotations | proposal only |
| structural control | `ExecutableFragmentIRV1` | node IDs and finite controller data; no probabilities | control-flow description only |
| exact leaf verification | `VerifiedLeafFragmentV1` | exact row/exit/resource/properness diagnostics | verification evidence only |
| flattening | `FlattenedFragmentCandidateV1` | optional candidate estimate and ordinary strategy JSON | candidate only |
| independent assertion | existing `CompiledPolicyAssertion` | exact evaluated cost/resources/status | existing assertion only |
| executable upper | existing `BoundedPolicyIncumbent` through `IncumbentPortfolio` | certified/evaluated upper | sole upper authority |

For this v1 boundary, the fragment lane stops at an independently evaluated
shadow candidate. It must not construct, convert to, or submit a
`BoundedPolicyIncumbent`, and it must not call `IncumbentPortfolio`.

Required compile-time and API properties:

- no proposal, IR, verified fragment, exit kernel, or flattened candidate is
  convertible to `double`, a proof-lower type, `CompiledPolicyAssertion`, or
  `BoundedPolicyIncumbent`;
- the only construction path for `VerifiedLeafFragmentV1` is the exact
  verifier's successful result;
- estimates, exact resource diagnostics, independently evaluated candidate
  cost, public upper, and independent lower retain distinct field names;
- no fragment type owns a pruning flag, closure flag, lower-bound field,
  scheduler priority, action-ledger retirement token, or incumbent field;
- `ExecutableCarrierProjection` may label a diagnostic but may not identify an
  exit, product state, SCC member, route, or merge class; and
- the old carrier planner cannot be re-enabled behind a flag.

## Locked Leaf Fragment IR v1

`ExecutableFragmentIRV1` is versioned finite control flow with no nested
fragment calls and no authoritative probability field. Its canonical identity
must include:

- schema version `1`;
- stable primitive/fixed-program action identities, never context-local
  numeric operator indices;
- exact entry-domain identity;
- caller action scope and disabled-family identity;
- exact goal identity;
- controller node and edge identities;
- route conditions expressed in the existing typed condition vocabulary;
- observed-choice policy and ordering where applicable;
- disjoint typed exit labels and parameters;
- finite controller-memory schema, if any;
- mechanics/data artifact identity;
- exact state-key/refinement/condition semantics versions; and
- verifier/properness/tolerance versions.

Allowed node kinds are route, primitive operation, observed choice, and exit.
Every router has an explicit certification fail-closed default. The IR cannot
contain a probability, exit estimate used as a row, nested option/fragment
call, implicit Restart, Imprint checkpoint operation, arbitrary action bag, or
new simulator operation/condition vocabulary.

If an exact semantic distinction cannot be expressed through the existing
ordinary strategy condition vocabulary, verification refuses the fragment.
It must not route on a coarser carrier projection or extend the strategy
vocabulary inside this boundary.

## Locked Exact Leaf Verification Contract

The verifier owns a private context containing an immutable copy of the exact
entry item, goal, action scope, economy/resource vocabulary, and mechanics/data
references. It has private exact-state interning and transition caches and no
mutable reference to the core solve graph, scheduler, action ledger, proof
store, lower state, or incumbent portfolio.

The reachable product state contains at least:

```text
exact item stable key
controller node
finite local controller memory
finite hard execution state required by the fragment
```

Imprint/checkpoint behavior is out of scope; a fragment requiring live
checkpoint state is rejected.

For every primitive node, the verifier must:

1. resolve a stable action identity under the exact caller scope;
2. check exact legality;
3. ask the engine-owned primitive transition owner for every physical outcome;
4. retain stored-double probability bits and exact successor identities;
5. attach transition-local resource quantities;
6. execute the IR's deterministic next-controller rule; and
7. preserve distinct exact successors unless replay-backed exact lumpability
   proves their entire executable behavior equal.

The proposal and IR supply no probability input. A computation cap,
cancellation, timeout, illegal operation, missing observation, or unexpressible
predicate is a refusal with no positive certificate, never a stochastic exit.

## Locked Exit-Mass, Properness, And Resource Contracts

Exit identity includes the typed exit kind and parameters, exact exit item,
and finite outcome memory. Every positive-probability physical outcome appears
exactly once. Merging is allowed only for completely equal exit identities.

Required validations:

- every nonterminal row has one executable action, at least one transition,
  finite nonnegative probabilities, unique exact successors, and total mass
  one within the named tolerance;
- no missing mass, duplicate mass, probability rewriting, or renormalization;
- exact exits that share a carrier projection remain distinct without a full
  exact replay/lumpability proof;
- exits have no selected action or outgoing transition;
- exit probability sums to one for every certified entry;
- resource diagnostics retain expected quantities by stable resource key and
  joint resource mass per exit, not only a priced scalar; and
- structural verification can succeed without prices, but priced diagnostics
  require complete finite prices and remain non-authoritative.

After row validation, run deterministic SCC decomposition over the complete
positive-probability product graph. Reject the lexicographically canonical
reachable closed non-exit SCC. Only after properness succeeds may the verifier
solve expected action-count and resource-vector equations over transient SCCs,
require finite nonnegative solutions, and validate residuals.

This proves absorption to a declared leaf exit. It does not prove final crafting
success and does not confer an upper bound.

## Locked Single-Fragment Flattening Contract

Flattening is structural graph copying into the existing ordinary strategy
JSON vocabulary, not probability composition. It consumes verified structural
control plus an explicit single-fragment exit disposition. It must not read,
multiply, rewrite, copy, normalize, or branch on verified exit probabilities.

The flattener must:

- copy route, primitive-operation, observed-choice, and exit control nodes;
- preserve exact route conditions and observed-choice ordering;
- map final-success only to the ordinary exact success terminal;
- map any unsupported/non-final positive exit to an explicit certification
  fail-closed route unless the recovery is already inside the one fragment;
- use canonical node/edge ordering and deterministic IDs;
- prune only structurally unreachable nodes;
- merge only complete equal executable behavior, never carrier projections;
  and
- serialize ordinary strategy JSON accepted by the existing parser.

The independent evaluator receives only the flattened JSON, exact start item,
goal, action scope, and prices. It must rebuild all route observations and
primitive kernels from mechanics and must not receive a fragment row, exit
kernel, expected resource vector, or proposal estimate.

Version 1 performs no incumbent promotion even when independent evaluation is
complete. Candidate/evaluator diagnostics remain separate from the ordinary
solve result.

## Real Cyclic Vertical Slice

The one real integration control is a small engine-backed
`clean_one_goal_transmute_scour_renewal_v1` fragment on an already supported
ordinary base and already implemented goal family:

```text
clean normal item
  -> Transmute
  -> exact goal success: FinalSuccess
  -> exact non-goal magic result: Scour -> clean entry
```

The exact primitive mechanics, legality, pools, weights, and goal predicate
remain engine-owned. The fragment supplies only this control flow. Its product
graph must contain a real positive-probability cycle, prove absorption, produce
finite Transmute/Scour expected quantities, flatten to ordinary JSON, and
independently exact-evaluate to success probability one with zero off-policy
mass and complete cost reconciliation.

If the chosen current base/goal makes any mechanic behavior ambiguous, stop
and ask Oliver rather than selecting a different interpretation. Do not add or
change a mechanic to make the control pass.

## Shadow-Only Integration Contract

The fragment core is activated only by an explicit versioned native
benchmark/Lab diagnostic case field that is absent by default. It runs in a
private context after capturing immutable inputs. It may append a separately
named `fragment_shadow_v1` diagnostic block, but it may not alter:

- core graph states, rows, generations, or transition caches;
- scheduler queues, work order, counters, or caps;
- action-envelope discovery, classification, retirement, or debt;
- proof pattern/quotient state or lower values/provenance;
- ordinary candidate ordering or incumbent portfolio contents;
- compiled/published ordinary solver strategy;
- solve status, termination classification, or public upper/lower; or
- product defaults, C ABI request shape, web request shape, or release WASM.

Control and shadow runs use fixed core work and compare every behavior-bearing
ordinary result/telemetry field. The only allowed difference is the additive
shadow diagnostic and its separately accounted private resource/time fields.

The preferred change surface is private native source, native tests, benchmark
fixture/schema, corpus validation, and stable solver documentation. No new C
ABI, strategy operation/condition vocabulary, product control, or WASM behavior
is expected. Stop and ask Oliver if the core cannot be implemented without
crossing one of those boundaries.

## Gate 0 — Current-Source Audit And Failure-Class Freeze

### Work

Revalidate every starting owner, the exact solver/refinement/compile/eval
interfaces, the engine source inventory, and the benchmark/Lab case schema.
Record the precise new-file/test integration points before production edits.

Freeze two baselines:

1. current option-disabled cyclic/refinement/compiler/evaluator behavior; and
2. a compact test-only historical failure fixture with physical outcomes
   `0.50 progress`, `0.30 recoverable miss`, and `0.20 blocker/wrong carrier`,
   where the latter two share a carrier projection but require different
   continuations.

The fixture retains the archived rejected estimates
`12365.392875058` and `12197.277488393` as classification metadata only. It
must reproduce missing-mass rejection before any value or incumbent operation.

### Focused Tests And Evidence

- Fresh-task MCP read-only inventory and dispatcher record.
- Current targeted native `refinement`, `eval`, `compile`, and `solve` control
  identities/statuses, recorded before source behavior changes.
- Test-only fixture proves that the old `0.80` coarse row cannot satisfy the
  current transition-sum contract and cannot reach an incumbent path.
- Record exact current files/functions that will own contracts, verification,
  flattening, benchmark diagnostics, and tests.

Do not run the complete repository pipeline at this gate.

### Pass

Current owners are known, the historical failure classification reproduces,
and option-disabled control identities are frozen without production behavior
change.

### Hard Stops

- Stop if the historical row is accepted, silently normalized, or reaches a
  value/incumbent path.
- Stop if current exact transition ownership cannot be reused without
  inventing mechanics or accepting proposal-authored probabilities.
- Stop if the vertical slice requires Imprint, a new strategy vocabulary, or a
  public product control.
- Stop if MCP inventory or source state differs from the mandatory startup
  record.

### Retained State

Retain the source-owner map, exact baseline identities, compact historical
fixture, and failure classification in the execution log/tests. Do not retain
prototype production behavior at Gate 0.

## Gate 1 — Authority Firewall And Versioned Leaf IR

### Work

Add a private native contract header for the authority ladder and versioned
leaf IR. Keep estimates outside structural control; exclude all probability
fields and nested fragment calls. Add canonical semantic identity using stable
action/goal/scope/version bytes and full equality after any digest match.

Construction APIs must make the legal direction explicit:

```text
proposal -> structural IR -> exact verifier result -> flattened candidate
```

There is no API arrow to proof/lower, scheduler/action ledger, or incumbent.

### Focused Tests And Evidence

- Compile-time non-convertibility assertions for every authority boundary.
- Construction tests prove verified fragments cannot be directly built by a
  proposal or deserializer.
- Canonical identity repeats byte-for-byte; behavior-bearing mutations change
  identity; labels/display ordering do not.
- Malformed schema version, duplicate node/edge, missing entry/exit, nested
  fragment call, implicit default, unstable operator index, and unknown action
  refuse deterministically.
- A source-include audit proves contract files do not include scheduler,
  action-ledger, proof-pattern, solve-bound, or incumbent owners.

### Pass

The type system and construction surface prevent proposal/fragment data from
becoming probability, proof, scheduler, or incumbent authority, and the v1 IR
represents finite fail-closed leaf control flow only.

### Hard Stops

- Stop if a fragment type needs an upper/lower field, pruning/closure bit, or
  `BoundedPolicyIncumbent` constructor.
- Stop if a proposal/IR probability field is introduced even as a convenience.
- Stop if stable identity depends only on a digest, display text, or
  context-local operator index.
- Stop if the IR requires new simulator operations or conditions.

### Retained State

Retain the private authority contracts, canonical identity tests, and v1 IR
shape. Do not checkpoint Gate 1 alone as a completed capability; its coherent
checkpoint is Gate 2's verified leaf core.

## Gate 2 — Exact Leaf Product Verifier

### Work

Implement private exact product-graph construction, complete row/exit
validation, deterministic SCC properness, absorption, and expected
resource-vector diagnostics. Reuse engine-owned primitive transition and exact
refinement/evaluation utilities only through read-only/private contexts.

Return a positive `VerifiedLeafFragmentV1` only after all structural,
probability, exit, properness, resource, and residual checks pass. Refusals
retain bounded canonical witnesses but no partial certificate.

### Focused Tests And Evidence

Cover at least:

- deterministic success;
- stochastic success/recovery;
- zero-probability physical edges after authoritative row validation;
- observed-choice ordering;
- illegal action and unexpressible predicate refusal;
- missing mass (`0.80`) rejection;
- renormalized `0.625/0.375` rejection against the authoritative outcome
  multiset/probability-bit signature;
- duplicate physical mass greater than one rejection;
- invalid carrier-projection merge of distinct exact exits;
- explicit recovery preserving mass one;
- reachable closed non-exit SCC with canonical witness;
- proper cyclic retry with absorption one;
- finite expected action count and per-resource quantities;
- transition-reward/occupancy/resource residual agreement;
- missing/nonfinite prices producing no priced diagnostic but not corrupting a
  structural certificate; and
- state/byte/time/cancellation refusal producing no certificate or exit mass.

Run only the new focused fragment suite plus the narrow existing refinement
and evaluator controls it reuses.

### Pass

Every accepted leaf has exact engine-built rows, mass-one exits, no unproved
projection merge, proved absorption, finite expected resources, deterministic
identity, and no path into live solver authority.

### Hard Stops

- Stop on any missing/duplicate mass acceptance or renormalization.
- Stop if a carrier projection decides state equality, SCC membership, or exit
  identity.
- Stop if a cap/refusal is represented as stochastic failure mass.
- Stop if expected resources are computed before properness or can be
  nonfinite/negative without refusal.
- Stop if verifier work mutates a core solve owner.

### Retained State And First Coherent Checkpoint

Retain Gates 0–2 together as the first coherent local source checkpoint:
authority firewall, versioned leaf IR, exact verifier, historical regressions,
and focused tests. Update `HANDOFF.md` and the execution log, commit with
`Co-authored-by: Codex <codex@openai.com>`, and leave a clean tree. This
checkpoint is useful even if the session ends before flattening.

Do not claim an executable strategy, upper improvement, or product behavior at
this checkpoint.

## Gate 3 — Real Cyclic Engine-Backed Leaf

### Work

Materialize the one-goal Transmute/Scour renewal fragment using an existing
supported ordinary base, current exact goal predicate, and engine primitive
kernels. Add an explicit versioned benchmark/Lab diagnostic fixture that is
absent from ordinary/product requests.

The fragment must internally route non-goal Transmute outcomes to Scour and
back to its exact clean entry, exposing only exact final success as its
positive-probability accepted exit.

### Focused Tests And Evidence

- Benchmark `--validate-only` accepts the new diagnostic case and rejects
  malformed/unknown fragment fields.
- Two native runs produce identical IR, product graph, exit-kernel, SCC,
  resource-vector, and certificate identities.
- The graph contains a real positive-probability SCC and proves absorption.
- Expected Transmute/Scour quantities match an independent forward-reference
  calculation and tight residual tolerance.
- Exact goal terminal semantics reject any junk/unrequested-affix terminal.
- Option-disabled paired control retains its ordinary solve identity.

### Pass

One real engine-backed cyclic leaf verifies deterministically with complete
mass, proper absorption, exact-terminal success, and finite expected resource
diagnostics, while the ordinary control is unchanged.

### Hard Stops

- Stop if the fragment needs hardcoded pool weights or mechanic behavior.
- Stop if the cycle is synthetic rather than built from actual engine
  transitions.
- Stop if a non-goal physical outcome disappears, is normalized away, or is
  mapped to success.
- Stop if the diagnostic is enabled by default or changes ordinary solve
  output.

### Retained State

Retain the versioned real diagnostic fixture, deterministic certificate
evidence, and exact control comparison. Runtime reports stay ignored/local;
summarize identities and results in the execution log.

## Gate 4 — Single-Fragment Flattening And Independent Evaluation

### Work

Implement structural flattening for one verified leaf into ordinary strategy
JSON. The flattener receives no probability-bearing view. Parse the emitted
JSON afresh and evaluate it through the existing exact compiler/evaluator with
the exact start, goal, scope, and prices.

The independently evaluated result remains a shadow
`FlattenedFragmentCandidateV1`; do not invoke incumbent promotion.

### Focused Tests And Evidence

- Flattening API/include tests prove it cannot access an exit probability or
  expected-value field.
- Poisoned/unavailable certificate diagnostics cannot change serialized JSON;
  only structural control and exit disposition can.
- JSON is byte-identical on repeat and uses only existing operation,
  condition, and terminal vocabulary.
- Fresh parser/compiler accepts it; fail-closed defaults reject any uncovered
  route.
- Existing exact evaluator and independent forward reference agree on success
  probability, expected actions/resources, cost, properness, and mass error.
- The real cyclic fragment has success probability one, zero failure/stop/
  action-not-applied/no-matching-edge/unresolved mass, complete prices, and
  exact cost reconciliation.
- Distinct exact exits sharing a carrier projection remain distinct in routing
  fixtures.
- A malformed, improper, or cheaper-looking fragment candidate cannot change
  a seeded verified incumbent identity/value.

### Pass

The real verified leaf becomes deterministic ordinary strategy JSON whose
primitive probabilities are independently rebuilt and exactly evaluated, with
no fragment data promoted into incumbent authority.

### Hard Stops

- Stop if flattening reads or composes an exit probability.
- Stop if exact evaluation consumes a fragment certificate or expected cost.
- Stop if any route defaults to implicit Restart/recovery rather than
  certification fail-closed behavior.
- Stop if existing strategy vocabulary must change.
- Stop if a fragment candidate reaches `IncumbentPortfolio`.

### Retained State

Retain the single-fragment flattener, deterministic JSON fixture/hash,
independent exact-evaluation record, and focused tests. Do not retain any
multi-fragment/meta-controller scaffold or publication hook.

## Gate 5 — Shadow Isolation And MCP Operator Qualification

### Work

Integrate the fragment diagnostic behind the explicit versioned benchmark/Lab
case field, disabled when absent. Run it in a private context without sharing
mutable core state. Add bounded separate telemetry sufficient to audit
fragment status, identity, state/row/SCC counts, exit mass, expected resources,
flattened JSON identity, and independent evaluation—never a public bound.

Then use the configured Solver Lab MCP surface, without opening the GUI, for
the operator workflow.

### Focused Tests And Evidence

Automated shadow-isolation tests compare fixed-work control versus shadow runs
and require bit-identical ordinary:

- request/action/caller-scope identity;
- core state/row/generation and scheduler work counters;
- action-envelope ledger contents and retirement state;
- proof/quotient telemetry and lower/provenance;
- incumbent portfolio identities and public upper;
- compiled ordinary solver strategy identity;
- solve status/termination; and
- cap/resource classification.

Only the additive `fragment_shadow_v1` diagnostics and separately accounted
private time/bytes may differ. Capped, canceled, malformed, improper, and
independently rejected fragment work must leave every ordinary field
unchanged.

The fresh-task MCP workflow must:

1. rediscover cases/revisions through MCP and identify the immutable cyclic
   control/shadow inputs;
2. submit through MCP with complete idempotency keys;
3. monitor live job/attempt/partial evidence through MCP;
4. cancel a genuinely live selected attempt through MCP and verify terminal
   process-tree removal and released reservation;
5. submit fresh complete control and shadow attempts through MCP;
6. compare the terminal attempts through MCP, including ordinary authority
   parity and separate shadow diagnostics; and
7. export an investigation bundle through MCP, replay the same key, and verify
   stable bundle/artifact identities through MCP.

Repository tools may build the native executable and edit source/fixtures;
they may not substitute for any listed operator action.

### Pass

The shadow lane is demonstrably isolated, the real cyclic case completes with
verified flattened/evaluated diagnostics, cancellation releases all ownership,
and compare/export evidence is complete and immutable through MCP.

### Hard Stops

- Stop on any ordinary authority/trajectory difference caused by enabling the
  shadow diagnostic.
- Stop on leaked process, active/quarantined lease after proved termination,
  unhashed artifact, identity drift, or unstable bundle replay.
- Stop if MCP cannot discover/submit/monitor/cancel/compare/export the case
  without an untyped native argument or GUI.
- Stop rather than force-clearing dispatcher ownership or editing catalog
  state outside MCP.
- Stop if telemetry labels a fragment estimate/evaluated candidate as an upper
  or gap input.

### Retained State

Retain immutable Lab attempts and bundles as ignored local evidence, summarize
all IDs/hashes/results in the execution log, and retain focused isolation and
operator tests. Do not commit bulky runtime catalogs/artifacts or user-local
MCP configuration.

## Gate 6 — Final Acceptance, Documentation, And Handoff

### Work And Focused/Complete Tests

Run once after all source and tests are coherent, from a clean source
checkpoint:

1. native build through `powershell -File scripts/build.ps1` or the equivalent
   repository-owned final build path;
2. complete focused fragment, refinement, compiler, evaluator, solve, API,
   benchmark-schema/corpus, and Solver Lab integration suites;
3. deterministic repeat of the real cyclic native control;
4. independent exact evaluation of the flattened ordinary strategy;
5. exactly 10,000 Simulator runs for the compiled cyclic strategy, requiring
   10,000 successes and zero failure/stop/limit/inapplicable/missing-edge/
   off-policy outcomes;
6. the full `powershell -File scripts/test.ps1` repository pipeline exactly
   once after coherence;
7. final audit proving no C ABI, strategy vocabulary, product-default, web
   request, generated data, or WASM behavior change;
8. `git diff --check`; and
9. one-off Markdown link and active/archive reachability audit after final
   documentation moves.

No rendered/visual review is in scope. A WASM rebuild is not expected because
the selected boundary forbids C ABI, strategy-vocabulary, browser-visible, and
product-default changes. If implementation unexpectedly crosses one of those
boundaries, stop and ask Oliver rather than adding a rebuild/product gate after
the fact.

Update the owning stable solver/internal documentation with the authority
firewall, v1 IR, exact verifier, exit/properness/resource contracts, flattening
and evaluator separation, historical regressions, and shadow limitation.
Record exact tests and MCP evidence in the execution log, write a result,
archive the boundary, set `HANDOFF.md` to no active boundary, and create
coherent local commits ending with
`Co-authored-by: Codex <codex@openai.com>`. Do not push.

### Pass

Every focused, real-engine, MCP, deterministic, exact-evaluation, 10,000-run,
complete-pipeline, diff, scope, and documentation check passes. The published
behavior remains unchanged and the result claims only a verified shadow leaf
core—not meta-option quality, an incumbent improvement, or product enablement.

### Hard Stops

- Stop on any mechanic, probability, mass, properness, cost, or resource
  reconciliation mismatch.
- Stop if exact evaluation is not independent from fragment verification.
- Stop if proof/lower, scheduler, action ledger, incumbent, policy, or product
  default changes.
- Stop if the full pipeline reveals native/WASM semantic drift.
- Stop if the MCP workflow differs from automated stdio/fixture authority.
- Stop rather than weaken test counts, tolerances, exactness, process, or
  identity assertions.

### Retained State

Retain only a fully accepted shadow-core boundary. If final acceptance fails,
keep the deepest coherent checkpoint—at minimum the Gates 0–2 authority/
verifier checkpoint when it passed—leave the tree buildable and clean, update
the execution log/HANDOFF with the first failure and exact next command, and do
not archive or describe the boundary as complete.

## Explicit Non-Goals

- multi-option or multi-fragment meta-policy search;
- subgoal-order enumeration or a meta-controller;
- broad/automatic option-library generation;
- cache architecture beyond any test-local deterministic identity check;
- option-derived incumbent promotion or product upper improvement;
- scheduler integration, prioritization, proof scheduling, or action-ledger
  consumption;
- independent lower changes, proof-pattern changes, quotient changes, pruning,
  or exact-closure claims;
- RCASSP or retention/capacity abstractions;
- learned guidance, feature logging, training, inference, or GPU work;
- PDR memory attribution/repair or scheduler-aware replay;
- Imprint/checkpoint fragments or automatic Imprint behavior;
- economic Restart as an implicit failure handler;
- product-default enablement, Calculator controls, GUI features, or rendered
  review;
- new crafting mechanics, rule changes, action kinds, or strategy vocabulary;
- C ABI, Python binding, WASM API, web protocol, or remote-worker changes; and
- another six-hour/overnight qualification claim. The prior soak was
  owner-waived, not passed.

## Cross-Gate Stop Conditions

- Never reset, clean, restore, discard, or overwrite pre-existing user work.
- Never hand-edit canonical SQLite or compiled runtime data.
- Never accept proposal-authored, normalized, missing, duplicate, or
  projection-merged probability mass.
- Never convert a fragment artifact or diagnostic into proof/lower, scheduler,
  action-ledger, incumbent, or publication authority.
- Never let a private shadow context mutate or lend storage to the core solve.
- Never use a historical strategy/estimate as fresh current authority.
- Never change mechanics without Oliver's explicit ruling.
- Never broaden into a meta-search, option library, RCASSP, learned guidance,
  PDR, Imprint, product-default, ABI, or WASM boundary without a new selected
  plan.
- Never use GUI, SQL, direct catalog edits, or arbitrary native arguments to
  replace the required MCP operator workflow.
- Never cite the prior unattended-hardening boundary as overnight qualified.
