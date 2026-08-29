# Verified Executable Graph-Fragment Core v1 Log

**Status: Gates 0–5 passed. Gates 0–2 are retained in local commit
`94f9584a98298c9987e335bbcd82c41423877997`; Gate 3 is retained in the local
commit `fdc7e10fd4f1e4112ebf680e1a734f467309b668`; Gate 4 is retained in the
local commit `d9ef102853ac7e00d7c7bae3e245e8518354346a`; Gate 5 automated shadow
isolation is retained in `faf6eba8704df8c79d4bc448d3bea18f14cbb185` and
MCP operator qualification passed from clean integration commit
`79c925f73f48c9bb60b02a5218a05bb236c84870`. Gate 6 final acceptance is
next.**

Parent: [Plan](plan.md)

## Selection Record

- Selected by Oliver: 2026-08-28.
- Stable starting boundary:
  `031d528709b6fb3bc8563c16231042382a945c07`
  (`Archive solver lab unattended hardening`).
- Branch: `main`, local-only; do not push.
- Selection-task tree: clean.
- Selected scope: authority firewall, versioned probability-free leaf
  executable-fragment IR for one exact entry, exact engine-built product graph
  with complete-key equality only, exit mass,
  SCC absorption, expected resources, one-fragment flattening, independent
  exact evaluation, historical mass/projection regressions, one real cyclic
  shadow control, and strict isolation from all live solver authority.
- Explicitly excluded: meta search, broad option generation, RCASSP, learned
  guidance, PDR memory, Imprint, incumbent promotion, product defaults,
  nontrivial lumpability, and reusable/symbolic multi-entry certification.
- Prior unattended-hardening wording: the six-hour soak was **owner-waived,
  not passed**. That boundary is not overnight qualification.
- No source, fixture, MCP configuration, catalog, runtime, or product behavior
  changed while selecting this plan.

## Mandatory Fresh-Task Startup Record

The fresh implementation task must fill every field before editing:

```text
fresh task: 2026-08-28 fresh Codex implementation task; gate restarted after
  Oliver explicitly instructed Codex to abort an unfinished incoming merge
branch: main
planning HEAD: 72abfaf44b73a7335048bd6e227e4584f745480b
HEAD parent: 031d528709b6fb3bc8563c16231042382a945c07
HEAD subject: Activate verified executable graph fragment core v1
HEAD changed paths: HANDOFF.md; docs/README.md; docs/active/README.md;
  docs/active/2026-08-28-verified-executable-graph-fragment-core-v1/plan.md;
  docs/active/2026-08-28-verified-executable-graph-fragment-core-v1/execution-log.md
git status --short: empty after the authorized merge abort and again before
  this first edit; conflict-marker scan empty
AGENTS/docs/direction/HANDOFF/plan/log read: yes, in mandated order after the
  authorized merge abort; the resolved plan was read in full
change-impact/source-owner audit: pass at 72abfaf; change-impact read in full;
  stated carrier/fixed-option/transition/compiler/parser/evaluator/assertion/
  incumbent and benchmark/Lab owners match current source; selected placement
  is benchmark/test-private and outside engine/engine-sources.txt
MCP version/tool count: configured poecraft2-native-solver-lab entry point
  resolves to this checkout's solver_lab_mcp.py; server version 0.2.0; exactly
  31 bounded typed tools
MCP profile count/identities: 1;
  native_allflame_no_imprint_v1@876824a29d51ef8e87013639a86120315ca13235833261980b3eb28917b6bb56
MCP frozen case count/identities: 5;
  conquest-lamellar-allflame-clean-3-prefix-extended-product8;
  conquest-lamellar-allflame-clean-3-suffix-product8;
  conquest-lamellar-allflame-clean-4-pdr-product8;
  conquest-lamellar-allflame-partial-4-to-5-product8;
  spine-bow-allflame-clean-4-goal-product8
MCP revision count/identities: 3;
  case-rev-dc1e9c206c4402dbeededea1819074db;
  case-rev-15ce203781cd7935c4e0326fc1a65ca0;
  case-rev-5e1df8024975909ab780c335b9769ba6
MCP supervisor/dispatcher owner and mode:
  verified-live supervisor-7a5fd080-1103-4240-9ffa-a790569e3178,
  process token 50836:134324226021189767, active; this MCP runtime is
  control_only with reason verified_live_dispatcher; queue not paused
MCP bounded job/attempt counts: 18 jobs (13 completed, 4 partial, 1 canceled);
  19 immutable attempts
GUI opened: no
startup decision: proceed
```

The first startup attempt stopped without editing when it found an unfinished
merge of `f89b67a839ca8199605422947195c72b9175b600` into the required local
planning commit. Oliver then explicitly instructed Codex to deal with the
conflicts and continue. Codex aborted the untouched merge; the incoming commit
remains reachable through `origin/main`. The same untouched merge reappeared
once during read-only baseline work and again while Gate 4 documentation was
being updated. Both were aborted under that same explicit instruction; the
later abort preserved all uncommitted, passing Gate 4 source work and restored
the committed local planning documents without conflict markers. No merge
commit, remote update, source edit, or catalog mutation was used to manufacture
the startup result.

Expected conditions:

- `HEAD^` is
  `031d528709b6fb3bc8563c16231042382a945c07`;
- `HEAD` is one documentation-only commit titled
  `Activate verified executable graph fragment core v1`;
- only `HANDOFF.md`, `docs/README.md`, `docs/active/README.md`, this log, and
  `plan.md` changed in that commit;
- the tree is clean;
- configured Solver Lab MCP version `0.2.0` exposes 31 typed tools; and
- MCP performs the initial inventory without catalog mutation.

Stop without editing or cleaning if any source/tree/MCP condition differs.

## Gate Ledger

| Gate | Status | Required retained evidence |
| --- | --- | --- |
| 0 — source/failure freeze | passed | current owners, option-disabled controls, historical `0.80` missing-mass witness |
| 1 — authority firewall/IR | passed | non-convertible types, canonical v1 identity, probability-free finite leaf IR |
| 2 — exact leaf verifier | passed | exact product rows, exit mass, SCC absorption, resources, historical regressions |
| 0–2 checkpoint | passed | clean local commit `94f9584a98298c9987e335bbcd82c41423877997` and exact HANDOFF continuation |
| 3 — real cyclic leaf | passed | exact-terminal Transmute/Scour renewal certificate, analytical oracle, deterministic ordinary control |
| 4 — flatten/evaluate | passed | FinalSuccess-only positive exit disposition, ordinary JSON, independent exact evaluator, resource/cost reconciliation |
| 5 — shadow/MCP | passed | immutable ordinary finalization, isolated private caps/process, full/core identity comparison, MCP operator evidence |
| 6 — final acceptance | not started | focused/native/10k/full pipeline/conditional WASM/docs/scope checks and archive |

## Gate 0 Record

```text
revalidated source owners: solver_executable_carrier_planner.hpp is projection
  vocabulary only; FixedOptionSpec is in solver_model.hpp; OptionKernel is in
  solver_calc_types.hpp with builders in solver_options*.cpp/reforge/calc;
  exact-state/refinement/evaluation owners, solver_compile.cpp, simulator.cpp,
  solver_eval*.cpp, solver_policy_assertion*.cpp, BoundedPolicyIncumbent, and
  IncumbentPortfolio match the plan
engine source/test integration: fragment contracts/verifier are
  engine/benchmarks/solver_executable_fragment.{hpp,cpp}; focused coverage is
  engine/tests/test_solver_fragment.cpp and the --solver-fragment-only suite
  selector in the monolithic native test executable
benchmark/Lab diagnostic schema owner: engine/benchmarks/solver_benchmark.cpp
  owns native case validation/reporting; solver_lab_cases.py owns bounded Lab
  import validation; solver_corpus_runner.py and solver_lab_service.py bind
  immutable cases/revisions to the native benchmark
native-only/common-source placement: native benchmark/test-private; no new
  translation unit in engine/engine-sources.txt and no include from a common
  engine translation unit
release-WASM participation and conditional acceptance: none for Gates 0–2
  under the selected placement; changing that decision later requires the
  plan's conditional release-WASM build/parity gate
option-disabled refinement control: PASS, 362 checks / 0 failures
option-disabled evaluator control: PASS, 15,761 checks / 0 failures;
  exchangeable-family hashes compressed=9a12bd4cc7d2f472 and
  raw=eee79a5659d79e68
option-disabled compiler control: PASS, 583 checks / 0 failures
option-disabled solve control: PASS, 86,220 checks / 0 failures
baseline native-test executable identity: sha256
  c0c5f27647526946ad72d4d85448ee9eb486dabfe382ddb07a8d4abac3e4b012
historical fixture identity: historical-coarse-row-missing-mass-v1; exact
  authoritative physical signature 0.50 progress / 0.30 recoverable miss /
  0.20 blocker-wrong-carrier; the latter two use the same diagnostic carrier
  projection but distinct exact keys and distinct typed continuations
missing-mass result: PASS; the physical row retained only progress and
  recoverable miss, so the verifier refused authoritative_outcome_missing at
  0.80 before graph value/resource work; the 0.625/0.375 rewrite and duplicate
  1.20 physical mass also refused
archived estimate metadata: 12365.392875058 and 12197.277488393, retained only
  as finite rejected-classification constants in the test fixture; never used
  as probability, value, resource, candidate, or incumbent input
decision: PASS; owners and placement are frozen, all four pre-change controls
  passed, the historical failure class reproduces, and no production/common
  engine behavior changed
```

Do not treat archived estimates as current values or executable authority.

## Gates 1–2 First Coherent Checkpoint

```text
authority type names: ExecutableFragmentProposalV1 -> ExecutableFragmentIRV1
  -> ExactLeafFragmentVerifierV1/VerifiedLeafFragmentV1 -> private-token
  FlattenedFragmentCandidateV1; the last type is constructible only by the
  future SingleFragmentFlattenerV1 and is not yet an executable strategy
non-convertibility checks: compile-time assertions cover proposal/IR/verified/
  flattened cross-stage construction and conversion, scalar conversion, and
  CompiledPolicyAssertion conversion; VerifiedLeafFragmentV1 and
  FlattenedFragmentCandidateV1 have no public/default/deserializer constructor
IR schema/identity: schema 1, stable action/scope/family/goal/artifact and all
  named semantics versions, finite controller-memory schema, typed ordered
  fail-closed control, no probability field; canonical bytes are retained
  alongside FNV digest and equality checks both; exact-entry identity encodes
  its full canonical bytes after the digest; display labels/order are excluded
single exact entry identity/refusal evidence: two exact entries produce
  different separately verified certificate identities; symbolic: and domain:
  entries refuse symbolic_entry_domain_not_supported
malformed-IR refusal matrix: unsupported schema, duplicate node/edge/priority,
  missing entry/exit/target, nested fragment, Restart, Imprint, unstable action
  identity, missing/non-fail-closed default, invalid observed-choice ordering,
  unknown condition, and oracle-unknown action all refuse deterministically
exact product owner: benchmark/test-private ExactLeafFragmentVerifierV1 with a
  const ExactPrimitiveOracleV1; complete key is exact item key + hard execution
  state + controller node + finite controller memory; contract/source include
  audit found no solver solve/scheduler/action-ledger/proof/lower/incumbent owner
mass/renormalization/duplicate regressions: PASS for missing 0.20 outcome,
  stored-bit mismatch on 0.625/0.375, duplicate physical 1.20 mass, and
  authoritative/physical identity mismatch; zero-mass outcomes remain in the
  authoritative signature but create no reachable graph state; each physical
  record binds its own stored bits, exact successor/hard state, next memory,
  selected edge, and canonical resource quantities into certificate bytes
carrier-projection merge regression: same diagnostic projection with distinct
  exact exit keys remains two typed exits; diagnostic projection is absent
  from product keys, exit keys, routing, SCC membership, and certificate bytes
distinct exact-state/nontrivial-lumpability refusal: two distinct exact keys
  with identical future behavior remain three product rows rather than merge;
  there is no quotient/lumpability input or API
improper/proper SCC results: canonical reachable closed non-exit SCC refuses
  with no certificate; positive-probability cyclic retry verifies one cyclic
  SCC, absorption/exit mass 1, and deterministic repeated certificate identity
expected resource/residual results: at p=0.4, expected stable action count and
  transmute quantity are 2.5, priced diagnostic is 5.0 at price 2.0, per-exit
  joint resource mass reconciles to 2.5, and all mass/linear/resource residuals
  are below their fixed versioned tolerances; tolerance overrides refuse;
  unknown resource keys refuse against the copied immutable context vocabulary;
  missing/nonfinite prices omit only the optional price diagnostic
cap/cancel/refusal results: zero/binding state, transition, estimated-byte,
  graph/linear-work, already-expired deadline, and cancellation controls all
  return refusal with no VerifiedLeafFragmentV1 or stochastic exit mass
focused test command/result: powershell -File scripts/dev-engine.ps1 -Task
  Test -Suite fragment => PASS, 119 checks / 0 failures; required post-change
  controls: refinement PASS 362/0; evaluator PASS 15,761/0 with unchanged
  compressed/raw hashes 9a12bd4cc7d2f472 / eee79a5659d79e68
checkpoint commit: local commit `94f9584a98298c9987e335bbcd82c41423877997`,
  titled `Add exact executable fragment leaf verifier`, with
  Co-authored-by: Codex <codex@openai.com>
git status: clean after the local checkpoint commit; branch remains unpushed
decision: PASS; retain Gates 0–2 and continue only with Gate 3's real
  engine-backed cyclic leaf. No executable strategy, upper improvement,
  incumbent authority, product behavior, or overnight qualification is claimed
```

The first checkpoint may claim an exact verified leaf core only. It may not
claim ordinary strategy output, an executable upper, product behavior, or
option quality.

## Gate 3 Real Cyclic Leaf Record

```text
fixture/case identity: benchmark case
  fragment-clean-one-goal-renewal-v1; private fragment case
  clean_one_goal_transmute_scour_renewal_v1; schema
  verified_executable_graph_fragment_shadow_v1; explicitly enabled only in
  the native diagnostic field and never admitted to an ordinary/product API
base/goal identity: Metadata/Items/Armours/BodyArmours/BodyInt17@86;
  magic exact one LocalIncreasedEnergyShield11 at min tier 1, exactly one
  prefix, zero suffixes, and no junk/unrequested affix
engine authority path: compiled artifact -> current SessionImpl/CalcContext ->
  actual Transmute/Scour ActionDescriptors and outcome kernels; resource keys
  come from the descriptors; conditions are compiled/evaluated through the
  existing native strategy parser/compiler rather than reimplemented
IR/certificate identity: full canonical IR identity digest
  c63a344470b77adf; full exact certificate identity digest
  82e161b8b3e07889; canonical bytes, not the display digests alone, are the
  equality authority
product states/rows/transitions: 4,031 exact product states / 4,031 rows /
  8,061 retained transitions from 4,031 authoritative Transmute physical
  outcomes plus actual Scour recovery rows
positive-probability SCC: 1 strongly connected component, and it is a real
  positive-probability cyclic component
exit mass: exactly 1.0 to one exact FinalSuccess exit; zero accepted positive
  failure/subgoal/recoverable mass
absorption/properness: PASS; the SCC has the exact final-success exit and the
  verified absorption system is proper
exact terminal probability p: 0.0041946308724832215; actual engine coverage
  also proves positive goal-plus-junk and other nonterminal mass, both routed
  through Scour rather than accepted as success
expected Transmute quantity: 238.40000000000001
expected Scour quantity: 237.40000000000001
analytical oracle 1/p and (1-p)/p: exact agreement within 1e-9 in the verifier
  and independent forward-reference bits; the ordinary solver's independent
  exact evaluator reports 238.39999999999708 / 237.3999999999971
residuals: exact row-mass error 0; maximum absorption/resource residual
  9.9708300426923357e-14; exit-mass error 0
deterministic repeat: two fresh artifact/session/IR/oracle builds produce
  identical full IR, product-graph/exit-kernel/resource certificate, base,
  goal, and forward-reference identities
benchmark contract validation: PASS; --validate-only accepts the case and
  --fragment-contract-rejection-probes proves that an unknown proposal-
  probability field and malformed private cap are rejected by the same shape
  validator
ordinary control identity: shadow work is not launched at Gate 3; two ordinary
  native benchmark runs both converge exactly at 23.789999999999708, both
  exact evaluations match with success 1 and cost 23.789999999999708, and both
  produce the identical 2,141-byte compiled ordinary strategy with sha256
  b230e26e579004d15b5cf506108209463002e4977094c540516fd28e785876b3
focused test command/result: powershell -File scripts/dev-engine.ps1 -Task
  Test -Suite fragment => PASS, 143 checks / 0 failures; observed work
  258,091,648 and peak private estimated bytes 4,321,232
decision: PASS; retain the native real cyclic leaf and its explicit benchmark
  fixture, then continue to Gate 4. No flattening, incumbent authority, public
  upper, product behavior, or overnight qualification is claimed
```

## Gate 4 Flattening/Evaluation Record

```text
flattened strategy identity: full candidate canonical bytes with display digest
  2edeed49ce1d5e6a; repeated fresh verified controls emit byte-identical JSON
nodes/edges/JSON bytes: 5 compiled nodes / 6 compiled edges / 2,049 bytes;
  only existing start, operation, router/condition, edge-default, and success/
  failure terminal vocabulary is used
verified structural projection: the exact verifier alone constructs
  VerifiedLeafStructuralControlV1 from canonical IR plus exact positive-exit
  dispositions; complete exact-exit canonical identities remain distinct but
  the projection exposes no exit probability, resource, action-count,
  certificate, priced-cost, or expected-value getter
clean start binding: FragmentCleanBaseStateV1 is part of the canonical IR and
  is independently copied into the immutable verification context; mismatch
  refuses before graph work. V1 permits only the exact clean normal entry
  shape needed by this selected leaf
probability-free flattener audit: SingleFragmentFlattenerV1 accepts only the
  structural projection. Compile-time view checks prove that parameter is not
  probability-bearing; the flattener body never receives VerifiedLeafFragmentV1.
  Missing-price and NaN-price verifier diagnostics produce identical JSON and
  candidate identity; emitted JSON contains neither probability nor
  expected_cost fields
fresh parser/compiler result: PASS through a new compiled-artifact session and
  compile_strategy_json call over the emitted bytes; every non-exit IR node
  emits its explicit certification-failure default
exact evaluator status/cost: production exact evaluator converged, complete
  pricing, cost reconciliation PASS; total expected cost
  23.789999999999708 at Transmute/Scour price 0.05 each
success/off-policy mass: success 1; failure, stop, action-not-applied,
  no-matching-edge, and unresolved probabilities all exactly 0
properness/mass error: PASS; terminal mass 1, production residual and maximum
  conservation/terminal-mass error 1.2212453270876722e-14
expected resource reconciliation: Transmute 238.40000000000001, Scour
  237.40000000000001, expected actions 475.80000000000001; resource,
  action-descriptor, node-operation, material, and cost dot-product
  reconciliation all pass within 1e-9
forward-reference comparison: independent high-precision whole-graph forward
  propagation agrees with production success/off-policy mass, expected
  actions/resources, and total cost within 1e-9; it consumes only the freshly
  compiled ordinary strategy and economy with maximum delta
  9.9752406140089234e-13, never fragment certificate/value evidence
non-final positive-exit fail-closed fixtures: separately verified positive
  Subgoal, Recoverable, and CertificationFailure fixtures all refuse
  non_final_positive_exit. Two FinalSuccess exits with distinct exact states
  but one diagnostic carrier projection retain two exact dispositions and two
  distinct flattened routes
seeded incumbent unchanged: an actual native IncumbentPortfolio seeded at
  identity feedbeef/value 10 remains bit-identical after a cheaper-looking
  flattened diagnostic plus malformed and improper fragment refusals;
  compile-time construction/conversion checks expose no candidate-to-incumbent
  path
focused test command/result: powershell -File scripts/dev-engine.ps1 -Task
  Test -Suite fragment => PASS, 207 checks / 0 failures
decision: PASS; retain one deterministic FinalSuccess-only flattener and the
  independently evaluated shadow candidate. No IncumbentPortfolio observation,
  public upper, product behavior, new vocabulary, or overnight qualification
  is claimed
```

## Gate 5 Shadow And MCP Record

Continuation startup and read-only MCP inventory:

```text
fresh continuation task: 2026-08-28/29 configured Codex task
branch/worktree: main; clean
integration HEAD: 79c925f73f48c9bb60b02a5218a05bb236c84870
integration parents, in order:
  cc9facd527d86ae72d187bc1771499c1ecd9808c
  f89b67a839ca8199605422947195c72b9175b600
integration subject: Resolve verified fragment planning divergence
server/version/tools: enabled configured poecraft2-native-solver-lab stdio
  entry point; loaded implementation version 0.2.0; exactly 31 bounded typed
  tools
profile inventory: one profile,
  native_allflame_no_imprint_v1 @
  876824a29d51ef8e87013639a86120315ca13235833261980b3eb28917b6bb56
frozen case inventory: seven cases; all get_case reads returned ok=true:
  conquest-lamellar-allflame-clean-3-prefix-extended-product8 @
  4de8e1100eb2f541cf0654e3891577a725823ed418db1a1c2d9dfbfad160f758
  conquest-lamellar-allflame-clean-3-suffix-product8 @
  714588ed7f2325a2204a7d6adf4d8a7935c271b8e9261a4bc77ebed11b168883
  conquest-lamellar-allflame-clean-4-pdr-product8 @
  bbd55011c734132ef9f211eff16e67a44cfc72988c65d23f2152acade5ec130f
  conquest-lamellar-allflame-partial-4-to-5-product8 @
  02b3b2fa5b23b7d3f902ccdf1232969c8c590fae01960329d073df91aefe7b3d
  fragment-clean-one-goal-renewal-control-v1 @
  f681b20cf4b497011945f6e2fb6b57db8ef67752ecce4bd8a677ecd15fb3267c
  fragment-clean-one-goal-renewal-shadow-v1 @
  5ee9f53544a1cb38995c4e7b4733f57452e7a88bb3a83cae932253323cc1ba7b
  spine-bow-allflame-clean-4-goal-product8 @
  81da6f12dba87d89f0163b099e17d06fe547dda35fa583b18eaaa4f63ab3a606
immutable revision inventory: three revisions:
  case-rev-dc1e9c206c4402dbeededea1819074db @
  dc1e9c206c4402dbeededea1819074db6ca6ce27787e12361eff0c3445138611
  case-rev-15ce203781cd7935c4e0326fc1a65ca0 @
  15ce203781cd7935c4e0326fc1a65ca065e6448e91667b8f107a1f7fc085ab3b
  case-rev-5e1df8024975909ab780c335b9769ba6 @
  5e1df8024975909ab780c335b9769ba661be5c65453f32d3984807376dbd8e17
dispatcher owner/mode: catalog_owner,
  supervisor-c0ee9221-d818-461f-8f38-046e0f1dda7a,
  process token 57428:134324392766030429, active; queue unpaused; max_workers 1
initial bounded jobs: 18 total = 13 completed, 4 partial, 1 canceled
initial bounded attempts: 19 total = 13 completed, 4 watchdog, 2 canceled
GUI opened: no
startup decision: PASS; seven-case corpus and required fragment pair are live
```

Automated isolation:

```text
ordinary result/strategy/evaluation/telemetry finalized before shadow: PASS;
  ordinary-finalization.json is atomically written and hashed at phase 1
  before the separately invoked native shadow command at phase 2. The real
  worker sequence rehashed the final report and strategy after shadow and
  found both unchanged
shadow process/lifetime isolation: PASS; poecraft_solver_benchmark runs a
  second time with --fragment-shadow-only, a fresh artifact/session/verifier/
  parser/compiler/evaluator lifetime, separate report/log files, and no
  ordinary partial, strategy, checkpoint, Simulator, or solver arguments
shadow wall/work/state/transition/byte caps: external whole-process wall 180s;
  exact verifier max 20,000 states, 1,000,000 transitions, 1,000,000,000 work
  items, and 1 GiB estimated bytes; the fresh exact evaluator receives the
  same state/transition/byte allowances. The passing real shadow used
  258,091,648 work items and 4,321,232 peak estimated bytes in 25,374.8409ms
ordinary allowance/non-consumption proof: PASS; ordinary caps, watchdog,
  bounded-finish, status/termination, cap classification, report hash, and
  compiled strategy hash are captured before launch and remain unchanged;
  shadow cancellation/timeout/refusal never rewrites ordinary classification
control/shadow full_request_identity (expected unequal):
control/shadow core_solve_identity_v1 (required equal):
individual start/goal/economy/artifact/executable/vocabulary/scope/family parity:
individual ordinary cap/watchdog/bounded-finish parity:
allowed diagnostic differences: only fragment_shadow_v1 request/report plus
  its private process command, caps, wall/work/state/transition/byte evidence
core graph/scheduler parity: PASS; component sha256
  b2132dd21e9b81dc795fec2e36f910268989582b1d27acb85239a1b2685194b2
action-ledger parity: PASS; component sha256
  a92d9aad6d74608dcf31d7e60ceb739fb6de1ce08b49a5b9bbc28d6f01c4fbb6
proof/lower parity: PASS; component sha256
  9f7001eb4ba1175a052b236d7f6f6351d05c4bb0cd96b8658f9f102fe6410523
incumbent/public-upper parity: PASS; component sha256
  163c0b7a3f74a9654747c37f35fd941fbad6919dbc968a7b6171708f76ceec33
compiled-policy parity: PASS; both ordinary strategies are byte-identical at
  57,287 bytes with sha256
  8b1e6be735e3de9703ca27411ae19721b0445500a7691a8abf720a38edb55a59
status/termination/resource parity: PASS; the nine-component ordinary result
  identity is
  1f40d2f423ffd66d324087f624fd3a3a7c823e018c172c38a20ca14d460d0283
  on both real worker runs
malformed/capped/canceled isolation: PASS; malformed private wall caps refuse
  before launch, native independent refusal remains diagnostic, and killed or
  canceled shadow processes preserve completed ordinary status and hashes
real shadow diagnostic: verified_evaluated; IR 041927484ddf6dd2,
  certificate 4005aaa4a497331d, 4,031 states/rows, 8,061 transitions, one
  positive-probability cyclic SCC, exit mass 1, flattened identity
  2edeed49ce1d5e6a, independent success 1 / off-policy 0, and complete
  Allflame cost 91.58869999999888; local report sha256
  6fe05ef19e6a16fdd5f74a82136491056d30b2d104be72c6e205263412a10011
focused test command/result: fragment native suite PASS, 207 checks / 0
  failures; focused Solver Lab/runner/contracts/supervisor/MCP suite PASS,
  52 tests; native Lab case validation plus malformed fragment probes PASS
source checkpoint decision: automated isolation is coherent and ready for a
  local source commit. MCP operator qualification remains pending; Gate 5 is
  not yet claimed complete
```

MCP-only operator transcript:

```text
server version/tool count: poecraft2-native-solver-lab 0.2.0 / 31 typed tools
rediscovered immutable cyclic inputs:
  control fragment-clean-one-goal-renewal-control-v1 @
  f681b20cf4b497011945f6e2fb6b57db8ef67752ecce4bd8a677ecd15fb3267c
  shadow fragment-clean-one-goal-renewal-shadow-v1 @
  5ee9f53544a1cb38995c4e7b4733f57452e7a88bb3a83cae932253323cc1ba7b
  both are frozen corpus inputs with revision_id null; their immutable file
  and canonical content hashes are bound in each submitted request
known long-running frozen cancellation witness:
  conquest-lamellar-allflame-clean-4-pdr-product8 @
  bbd55011c734132ef9f211eff16e67a44cfc72988c65d23f2152acade5ec130f
cancellation submit key: gate5-79c925f-cancel-pdr-product8-submit-v2
cancellation job: job-cf9cd214-8642-4cea-b817-8e5d7c28c150;
  identity/validated request
  97aeb2795fb267750293c8d0b0a4f70a85f4f09cfe7ab07c6ce7491f9def6143;
  watchdog 120s; solver cap 1,073,741,824; worker headroom 536,870,912;
  reservation 1,610,612,736 bytes
cancellation attempt: attempt-535ea8a1-1fbb-4ce1-a96f-06295c48b808;
  command identity
  a8ca414a7425fc811a527ba4c52eeffbd095f99b2f95503f6c82f9e12dad306a;
  process token 41336:134324394551475355
genuinely live partial: phase 1 at 11,218.3255ms; 3,523 discovered / 3,128
  expanded / 395 frontier states; 34,430 rows; 64,023 transitions;
  2,486,821 reforge work; lower 21.772459401271156; source
  unindexed_live_observation
cancellation key/result: gate5-79c925f-cancel-pdr-product8-cancel-v1;
  MCP acknowledged running -> canceling; terminal canceled in
  476.5198000241071ms via graceful_then_process_tree_termination;
  survivor=false, process_group_kill_then_parent_poll, timed_out=false
cancellation terminal artifacts, all integrity_status=verified:
  artifact-73e4dfeff4985828487c1c30d2381ce3 partial_report @
  c28af53a072ebe5a185cc83c76d5a520fba952b62f50640b23b059bd23eb4c64
  artifact-0c53171c6816c5210cd22e77e3f6a590 supervisor_error @
  1ef36a4cb6ad8a8b21e44bc25bcc3672a327eb9d31de08f9d5a8f4702fbe3100
  artifact-f596cbd4daf1e933bd62943988b27dc6 worker_log @
  e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
process-tree/reservation release: PASS; terminal survivor=false and the
  singleton supervisor immediately dispatched and completed the next MCP job
  with no running/canceling remainder. Final supervisor inventory has no
  running job status and remains active/catalog_owner; no ownership or catalog
  state was force-cleared or edited outside MCP
control submit key/job/attempt: gate5-79c925f-fragment-control-submit-v1;
  job-b190c0ee-a2c9-4c18-8c36-b91686e0f76c;
  attempt-05faedf6-0614-46f4-b7b4-6ce481f1d623; natural completed/converged,
  exact_closed, survivor=false
control job/command identities:
  25537d321b20815471f337bd5fa951e64c602c05f8c98a67fe9f6500b741545b /
  5498a09d9963f68b692e6588b23656bb8a48cad2807f7fe54f27dfec82c2a85a
control full/core identities:
  9fc1e35bc2fa6785623376ff8f54a972f32e6c357469d63ac170975b8569f402 /
  87d11484686d552a07437e4e83b8f20879093631893b6a777ad17429a8dd619b
control ordinary finalization/result/report:
  f887aa9557dcb508d43c64efb6eaf8856dc6eebba4d71a91086c46df0939ff99 /
  1f40d2f423ffd66d324087f624fd3a3a7c823e018c172c38a20ca14d460d0283 /
  58376738b29a440046d23ca03db7f9b441f4e20ff8665f3c8ecc2f1d24809729
control strategy: 57,287 bytes @
  8b1e6be735e3de9703ca27411ae19721b0445500a7691a8abf720a38edb55a59
control artifacts, all verified: artifact-c048fd57ad6d70b0e6bcb48e9402689e
  ordinary_finalization; artifact-26852f32c807dd9df12f9d8e960fe9ad
  partial_report; artifact-dc7ea1ba91579f5aa3ee838277517ef8 report;
  artifact-dcd051ac9fd8c907f92556c6b4bb0075 strategy;
  artifact-0a64e49ac55237d1fa3458be7f62faba worker_log
shadow submit key/job/attempt: gate5-79c925f-fragment-shadow-submit-v1;
  job-c3134c27-a64c-47cd-a77a-5097c75cc00a;
  attempt-63fea1b3-6ab0-40e7-8cc6-1c8cef121a18; natural completed/converged,
  exact_closed, survivor=false
shadow job/command identities:
  22c039a4049b85596d2d2d183d66a6d82329a106cee25a8922ba65d170632469 /
  0c129e3c9b41077577fd7d56e97dbff85fe370c71411d5bbb274da7db31a2c16
shadow full/core identities:
  8e98f9c381e368c5882a81a1e0f40a0a4b0ab4d780fd2f80fc4aafcbbb0c3e77 /
  87d11484686d552a07437e4e83b8f20879093631893b6a777ad17429a8dd619b
shadow ordinary finalization/result/report:
  d9708b94687d8f799b5e0e893ebc0cd6379ffa676553d9f8613213c30e137efe /
  1f40d2f423ffd66d324087f624fd3a3a7c823e018c172c38a20ca14d460d0283 /
  3f4a1f55a6585f827aeffdb7777767a7aabfcf3cf86efe30db4dcdbe629d4cd6
shadow strategy: 57,287 bytes @
  8b1e6be735e3de9703ca27411ae19721b0445500a7691a8abf720a38edb55a59
shadow diagnostic/report: verified_evaluated @
  c462774ab3e95d9c34ddae8a57549759ae19b0006221f4dbf3aeecee21b0d695;
  ordinary finalized before shadow=true; ordinary unchanged after shadow=true;
  isolated process survivor=false
shadow artifacts, all verified: artifact-bffb4f1c7c8423ad08d14b5c22f61056
  fragment_shadow_log; artifact-6e83ac0e1b7aef799a7482c3efc465f9
  fragment_shadow_report; artifact-bc26d28a6888b79011ea0aa62788da42
  ordinary_finalization; artifact-57c39feb97ef5558dd30603941281728
  partial_report; artifact-a7962c9b9cedd70260311e31c95ad279 report;
  artifact-a560b2bc60bc999d5546b670a01a4133 strategy;
  artifact-bceebfc33089a3434612a4a3157f39eb worker_log
MCP comparison: full_request_identities_equal=false;
  core_solve_identities_equal=true; all_core_solve_components_equal=true;
  ordinary_result_identities_equal=true; all_ordinary_components_equal=true
enumerated core equality: action_scope, action_vocabulary,
  case_without_id_or_fragment_shadow_v1, compiled_artifact, corpus,
  disabled_families, economy, executable, goal, measurement, profile,
  requested_bounded_finish, scheduler, solver_caps, source, start, and
  watchdog_seconds are all true
ordinary component identities, equal on both attempts:
  action_envelope_ledger
    a92d9aad6d74608dcf31d7e60ceb739fb6de1ce08b49a5b9bbc28d6f01c4fbb6
  cap_resource_classification
    3d0ac453cf90eb0140a857963280e341b16fe4ee8b6674d14177dd86d3d4a1a2
  compiled_ordinary_strategy
    ca41e85b6097c584ade6bc2f80f9024494970b55e793a3540a77dee324682c1a
  core_graph_scheduler
    b2132dd21e9b81dc795fec2e36f910268989582b1d27acb85239a1b2685194b2
  exact_evaluation
    9e4f35f45b99277139347ed8fb31ed4aac0b6f16eed7849a095f982eea687cec
  incumbent_public_upper
    163c0b7a3f74a9654747c37f35fd941fbad6919dbc968a7b6171708f76ceec33
  ordinary_inputs
    967819594ffa4247ab294ee7ae12ce6a2d50de36f3f1787fe10b11f22c775fe5
  proof_lower_provenance
    9f7001eb4ba1175a052b236d7f6f6351d05c4bb0cd96b8658f9f102fe6410523
  status_termination
    ca2aa7ad53288aafb179f3fb235fb3e5fc9b14ec5ef9e24e7493c4fac90df267
allowed difference: control fragment_shadow_v1 absent; shadow block alone is
  present and verified_evaluated. No ordinary component differs
bundle export/replay key: gate5-79c925f-fragment-shadow-bundle-v1
bundle identity: bundle-ded87deaef203dc6696308f4 @
  1850e74ae282ebb6deed7edfcc4d8c8cc87f209f267a684294f8949e2dfc08ab,
  264,714 bytes
same-key replay: identical bundle ID, path, SHA-256, size, job, and attempt
artifact replay audit: two repeated MCP get_job reads returned the identical
  seven shadow artifact IDs/hashes/sizes; every integrity status remained
  verified
initial rejected submit: a nonexistent optional experiment label produced an
  MCP execution error before any job/key was recorded; list_jobs proved no job
  was created. The corrected submit used a fresh canonical key. No SQL,
  catalog edit, GUI, process kill, ownership clear, or repository CLI operator
  substitute was used
GUI opened: no
decision: PASS; complete MCP discovery, submission, live monitoring,
  cancellation, terminal process-tree/release verification, natural
  control/shadow completion, comparison, bundle replay, and artifact integrity
  evidence is retained
```

All listed operator actions must use MCP. Repository tools remain the source,
build, and test surface only.

Gate 5 passes. Automated isolation and the configured MCP operator workflow
agree, every ordinary identity/component remains equal, and only the isolated
shadow diagnostic differs. Gate 6 final acceptance is the exact next action.

## Final Acceptance Record

```text
final source checkpoint:
native build:
complete focused fragment suite:
refinement/compiler/evaluator/solve/API suites:
benchmark/corpus validation:
Solver Lab integration suite:
real cyclic deterministic repeat:
independent exact evaluation:
10,000-run Simulator result:
MCP operator evidence audit:
full powershell -File scripts/test.ps1:
git diff --check:
native ABI/strategy vocabulary/product/WASM scope audit:
Gate 0 source-placement decision:
conditional release-WASM build/parity result or native-only proof:
documentation link/reachability audit:
stable docs updated:
result/archive checkpoint:
push: no
```

No rendered/visual review is planned. Gate 0 decides whether changed source is
native-only or participates in release WASM. Common engine source requires the
normal release-WASM build/parity; proven native-only source does not. Stop and
ask Oliver if implementation requires C ABI, strategy-vocabulary, public
request-shape, browser-visible/product-default, or WASM behavior changes.

## Stop/Handoff Record

If a gate stops, record:

```text
first failing invariant:
exact reproduction command:
observed vs required result:
why it cannot be fixed safely inside scope:
deepest coherent checkpoint:
retained files/evidence:
tree/build status:
one exact next command:
```

Do not weaken mass, identity, properness, residual, process, or independence
assertions to finish a gate. Preserve Gates 0–2 as the preferred coherent
fallback if they passed.

## Current Stop/Handoff Record

```text
first failing invariant: none; Gate 5 passed
deepest coherent checkpoint before this log update:
  79c925f73f48c9bb60b02a5218a05bb236c84870
retained evidence: immutable MCP jobs/attempts/bundle listed above plus the
  committed Gate 5 automated source and tests
tree/build status: clean integration source before this documentation update;
  configured MCP dispatcher healthy, queue unpaused, no running job status
one exact next command: create the Gate 5 documentation checkpoint, restore a
  clean source state, then begin Gate 6 with powershell -File scripts/build.ps1
```
