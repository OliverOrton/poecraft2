# Usable anytime results, trustworthy progress, and preserved solver mathematics

Repository: OliverOrton/poecraft2
Reviewed main: 4594e44b6b851511ae5471e52f232bce57d16856
Task: implement this programme from the current repository.

This is the first implementation request for this next programme. Do not assume
that you have received another plan, that an addendum has already been imported,
or that any work below is running. Everything required to start is in this
prompt and the repository. Prior chats, sandbox downloads, research scripts and
Oliver's home computer are not prerequisites.

## 1. Objective and working rules

Make the already-early verified strategy usable through a graceful Finish control,
repair progress export and interpretation, and investigate the remaining worker
responsiveness failure narrowly. Preserve the mathematical reasoning in its
canonical solver documents as part of implementation, not as an optional final
summary.

The completed progress-and-delivery programme is the baseline, not work to repeat.
Its Conquest-five policy is verified at about 47 seconds instead of 245; Ring now
natively recovers C220743.46355. Default browser finalization still waits for the
four-minute finish request. Broad global sweeps, more action families, automatic
optimal stopping, hard crafting-action budgets and a new solver are outside scope.

Follow AGENTS.md. Inspect relevant local changes and HANDOFF first; reconcile later
work without overwriting it. Work sequentially without subagents, inherited
deadlines, automatic restarts, destructive cleanup or push. Preserve protected
root `0` without inspecting, staging, modifying or deleting it. Preserve unrelated
work. Native C++ owns mechanics; do not invent PoE rules or hand-edit SQLite or
derived compiled data.

Keep original-price expected Chaos as the optimization objective. Retain complete
probability/pricing, the native clean-goal predicate, root-only versus statewise
authority, and existing numerical acceptance. Use existing compiler/evaluator,
portfolio, worker, benchmark, Lab and reporting owners. No second supervisor,
tracing platform, equivalence service, catalogue or certificate store.

Use one living record and a short HANDOFF. Run tests to resolve an uncertainty or
qualify a retained change, not ceremonially at every phase. Do not routinely poll
long runs through model turns; retain native supervision and use existing blocking
or event-driven waits. Timed solver runs are serial.

## 2. Read the evidence and relevant owners

Read AGENTS.md, HANDOFF.md and docs/foundation/tooling.md, then:

- docs/active/2026-09-14-progress-and-delivery/README.md, qualification.json,
  retained-policies.json, and worker-conquest-M12-qualification.json.
- apps/web/src/app/components/pc-calculator.ts: startSolve, renderSolvePanel,
  bindPriceInputs, cancellation, export and cleanup.
- apps/web/src/app/engine-client.ts, engine-protocol.ts, engine-worker.ts and
  relevant WASM serialization/control functions.
- engine/src/solver_solve_telemetry.cpp: progress_trace_json; and
  solver_solve_focused.cpp: begin_focused_upper_solve.
- Existing bounded-finish, publication, retained-artifact and resource owners
  where the chosen control or measured hotspot actually touches them.
- docs/solver/mathematics/policies.md and search-and-resumption.md; relevant
  sections of upper-authority.md, resources-resume-replay.md, benchmarking.md,
  claims.md and research.md.

Do not recursively read every linked archive. Inspect historical failures only
for a specific question. Preserve the lessons of immediate-dependency retry
starvation, report rewriting on every event, private-service phase corruption,
missing original-root certificates, and legacy Bow service accidentally routed
through the new Ring path.

Baseline facts, to confirm against those committed receipts:

- Native Conquest verification pairs: 244.6783095 -> 46.6442546 seconds and
  244.8998977 -> 47.0061799, at unchanged C85558.70618560436.
- Final WASM verifies at native 45.0324439 seconds and exports at worker
  243.7347345 seconds. Native and worker clocks are distinct.
- Maximum recorded worker step is 387.339 ms against an unchanged 250-ms limit.
  Synchronous begin is separately about 13.093 seconds.
- M12 restored the original 200-byte public pc_solve_progress ABI and uses an
  additive cursor API. Do not append fields to that structure.
- Native M11 qualification and the transport-only M12 correction retain their
  actual build identities. Ring's 143 successes and 857 action-cap censorings
  in 1000 trials are not an uncensored cost or exact deadline probability.

## 3. Mathematical contract: establish these premises before changing control

### 3.1 Interrupting search selects a witness; it does not truncate crafting

Separate the fixed crafting target theta, this invocation's run ID, and the
immutable artifact/certificate identity. Theta includes actual root, goal,
action/program/observation scope, native mechanics/data and prices. Identical
crafting targets in two runs do not make an old control message valid for both.

Let E_n be compatible complete proper root-policy certificates committed by
computational step n. In the exact mathematical model, every pi in E_n satisfies

    V*(s0) <= J_pi(s0).

Selecting a retained minimizer preserves an upper at any finite computational
interruption. This is a pointwise feasible-witness argument, not an optional-
stopping theorem and not a convergence assumption. Stopping solver computation
does not impose an action cap on the returned crafting strategy.

The artifact must remain paired with its certificate and evaluated value. A
historical scalar minimum of 7 cannot be returned with a cost-10 graph. A bounded
portfolio may discard old candidates only while retaining a compatible no-worse
witness under its selection contract. Root-only evidence never becomes an
arbitrary statewise upper. UI numbers/stage strings are not availability checks.

This mathematical statement uses true policy values or justified enclosures.
Preserve the native numerical evaluation/reconciliation interpretation; do not
promote a floating-point display into a new exact rational endpoint or relax
acceptance tolerances. Open alternatives remain open at Finish.

### 3.2 Seal the result at a defined selection point

Distinguish Finish intent, safe-point acknowledgement, result selection/sealing,
terminal response commitment and usable graph delivery.

At the existing publication owner's selection point, consider every compatible,
publication-eligible artifact whose verification is already complete, including
a cheaper complete artifact held by a strict/private owner awaiting transfer.
An unfinished cheap proposal is not eligible and cannot hold up the fallback.
Do not start fresh speculative work after acknowledging Finish.

Seal graph bytes, certificate, root/context and evaluated value together.
Serialization must not switch graph or value after sealing. Define cancellation
precedence for an applicable cancellation received/latched before terminal response
commitment. There must be exactly one terminal outcome; stale control after
commitment is inert. Duplicate Finish is idempotent and stale run IDs cannot
control a later invocation that reused a solver handle.

These are safety obligations, not a claim that current main has an artifact-loss
bug or that all return paths have bounded latency.

### 3.3 A new suspension point must preserve the logical transaction

Model concrete work W as committed evidence K plus task cursor and staged scratch.
Let alpha(W)=K discard unfinished scratch. Each finer step must either leave
alpha unchanged or make a complete logical commit permitted by the existing owner.
Interruption discards staged work, not committed evidence. Induction over those
steps preserves the row/certificate invariants.

Current stop logic assumes no public suspension halfway through sparse-row append,
and restores an incremental upper pass's moved lower snapshot through its specific
owner. Any new yield must preserve these premises or introduce genuine staging
that fences every consumer off from partial evidence.

A row with known 0.9 goal mass and unfinished 0.1 trap mass cannot normalize its
prefix into an accepted action. Preserve cumulative logical debit across resumes;
releasing scratch does not refund consumed work. Proper local options can still
compose into an improper global controller, so full original-root checking remains.

Safety under finer slicing does not prove eventual completion, fairness, unchanged
arithmetic/scheduling order or identical timed outcomes. Native step boundaries
can influence scheduling; host batch changes are a separate treatment.

### 3.4 Reuse, predicate simplification and accounting

Unchanged rooted executable semantics and complete certificate context preserve
policy cost/count laws. Trusted relabeling additionally requires correspondence
of decisions, observations, terminals, transitions and rewards. Equal costs,
node counts or goal masks are not such a proof. Prefer existing retained checked
bytes; do not build a general equivalence checker to avoid a finish-time check.

Preserve the existing Ring clean-entry argument: if retained condition C implies
omitted predicates F at every routed entry, C and C-and-F are equivalent. Exact
side occupancy k plus k distinct satisfying affixes assigned injectively to slots
implies no remaining junk. Overlapping requirements may match one affix twice;
below-tier family membership is not satisfying coverage. This is narrow guard
equivalence, not an all-action quotient or a uniform continuation-value proof.
No further runtime generalization is selected.

A frozen memory audit is reusable only while its entire transitive storage,
capacities and alias-ownership context remain immutable. Add changing evaluator
storage, simultaneous scratch and proposed allocations. Lazy cache mutation must
invalidate the audit. Preserve the existing aggregate cap.

## M0 — Repair and qualify progress export and labels

Confirm and repair these source findings:

A. data-progress-export is rendered in renderSolvePanel, but its listener lives
only in bindPriceInputs, which the solve-panel renderer does not call. Bind it
at the actual owning surface, with correct delegation or one binding per render.
Do not duplicate unrelated price handlers.

B. begin_focused_upper_solve sets both focused_upper_mode and focused_lower_mode.
progress_trace_json tests the lower flag first and can label an upper pass as
restricted_lower_workspace/focused_lower. Repair observation precedence or use
an unambiguous existing mode; do not change numerical flags for presentation.

Use focused component/logic tests: click the actual export button after rerenders;
check combined flags, lower-only, upper-only, ordinary and final states. Existing
worker JSON/transfer tests do not establish DOM interaction. Rendered visual
review remains Oliver's responsibility unless separately requested.

Freeze run identity and the actual ordered request/start/goals, prices/overrides,
activation and caps at submission. Export available runtime identities and mark
missing hashes honestly. Never substitute a later mutable form as the run input.

Allow export of bounded already-observed partial progress during a run; preserve
it after completion, cancellation or worker failure. Use existing callbacks and
sequence/omission fields. Avoid copying the full history on every progress event.
Keep source time separate from observation time. Dropped/missing events cannot
establish that readiness never occurred or identify its exact earliest time.

A logically passive read can still consume deadline time or affect a time-adaptive
host quantum. Test passivity at a fixed logical boundary separately from timed
overhead. No full telemetry scan each step, report rewrite each event, new solver
work on a status read, or forced monotonicity for mixed-scope counters.

Exit: real export/rerender and label tests pass; partial traces have frozen identity,
clock/coverage semantics and explicit status. Retain these fixes independently
of long-run scheduling outcomes.

## M1 — Graceful Finish with best verified strategy

Add an explicit Calculator action labelled “Finish with best verified strategy”
when a compatible independently verified policy is available. Keep Cancel distinct
and the automatic 240-second default unchanged. This is neither proof of optimality
nor resumable pause.

Carry a solve-request-ID-scoped intent through the existing client/worker control
transport and latch it at a safe cooperative boundary. An RPC queued behind the
entire long solve is not an interrupt. Do not reenter active WASM work from a
message handler. Use the existing native bounded-finish path, not abandon, worker
termination, a replacement solve or compilation from display annotations.

Apply the selection/sealing contract in section 3. Include complete eligible
artifacts before sealing; preserve the required original-root certificate and
private root-only authority. Handle duplicate/stale intent, handle reuse,
automatic-deadline races, cancellation and exact completion. Before verification,
the button stays disabled; forced/stale requests must still fail safely natively.
Show “Finishing…” as a request state, not as a completed-result acknowledgement.

Do not launch speculative improvements after acknowledgement. Eliminate duplicate
checking only after proving the existing artifact/context is unchanged and testing
that boundary. Matching numbers are insufficient to skip a changed-graph check.

Predeclared empirical target: on the matched Conquest surrogate, request Finish
at the first host-observed verified policy. Deliver a usable original-root graph
within 10 seconds of intent and within 65 seconds of worker solve start, including
native begin, at C <= 85558.70618560436 plus existing tolerance. Also record total
Calculator request-to-usable time, including any earlier UI preparation. Keep
source verification, host observation, request, acknowledgement, seal, response,
export and component readiness distinguishable.

This is a user-control/delivery result, not a new search or crafting-cost gain.
The timing target is not a mathematical latency theorem.

## M2 — Narrowly attribute and address the post-start responsiveness failure

Use saved M12 evidence first. The recorded 387.339-ms step fails the unchanged
250-ms contract. Do not describe the earlier 717-ms run as a matched speedup.

Retain bounded slow-call attribution: input/output phase and owner, work quantum,
duration, event/generation cursors and relevant task stage. The returned phase
alone does not identify what consumed the call. Separate native work, bridge
serialization, trace capture, final export and cleanup. Current trace capture is
outside measured stepSolverSolve time and cannot automatically explain its maximum.
Measure actual control-message acknowledgement latency too.

Response latency includes dispatch, remaining noninterruptible work, staged
cleanup, transfer of complete evidence, sealing/export, transport and UI work.
A native-step limit covers only part. Keep the approximately 13-second synchronous
begin visible; meeting the stepped-call limit does not establish interruptible
setup. Do not turn this into an unrestricted startup rewrite.

Choose one measured owner-level intervention: safely split a large resumable unit,
remove a repeated immutable audit with correct invalidation, or repair a proven
bridge/persistence cost. At every changed yield state the staging, commit, rollback
and cumulative-debit premises from section 3 must hold. Preserve phase ownership,
complete rows, certificates and aggregate budgets.

No cap/tolerance increases, unconditional batch reduction, global sweep or policy
check every tick. Millions of tiny calls do not prove smaller batches help.
Stop after two substantive failed hotspot variants without a new causal premise.
Retain useful export/Finish fixes independently and report responsiveness as
unqualified if its unchanged contract still fails.

## M3 — Decision-gated qualification

Use existing runner/Lab/worker/reporter with pinned source/build/data, target,
economy, action scope, activation, machine/load and capacities. Reuse compatible
receipts. Native timed cases run serially without concurrent games/heavy builds.
Keep raw evidence locally and compact source-bound summaries in the living record.

Conquest primary: CB01 in
  docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json
Retain default 240/300/315-second finish/native/host boundaries, 1-GiB cap,
product-step maximum eight and actual WASM-compatible retention activation.
Manual Finish is a separately declared arm, not an unnoticed default change.
CB02 and exact Regalia are affected controls.

Start with small native fixtures and component/worker control tests. Then use one
real default Conquest worker run and one event-triggered Finish run, reusing
compatible evidence where allowed. If timing is noisy or near a threshold, perform
one necessary counterbalanced confirmation. Test optional-check interruption,
complete cheaper evidence transfer before sealing, immutable sealed output,
duplicate/stale requests, cancellation races, exact completion, partial/error
export, passive repeated reads and no leaked handles.

Current reference C / expected primitive N:
  Conquest-five: 85558.70618560436 / 8407.202314771383
  Conquest-four: 3746.1319409485764 / 8608.881793656798
  Bow-four:      12770.827062219498 / 66914.72854184538
  Ring-four:     220743.46354691702 / 564673.7576801107
  Amulet:        12541.579648544115 / 25518.491313143768
  Exact Regalia: 65.60036144971359

Use current retained-policies.json for artifact hashes. These are references,
not runtime seeds and not interchangeable request profiles.

UI/protocol-only changes do not require every long native solve. For a shared
native finalization/slicing change, qualify affected Bow and root-only Ring, with
Amulet when its path changes. Preserve the existing wide 600/840/870-second lane,
8-GiB aggregate, 4-GiB evaluator, 400M shared work and exact resolved treatment/
action-price identity. Run that integration only after local phase/retention/
certificate tests pass, not after each speculative patch.

Independently evaluate changed emitted controllers. Reuse unchanged-policy
execution evidence; use owner-approved 1000 trials only when genuinely changed
graphs need fresh execution qualification, retaining censorings. Rebuild/check
WASM for actual native/ABI impact and use the old-header probe where relevant.
No rendered-UI or browser-search claim from TypeScript or fixed-graph checks.
Documentation-only edits require no native builds or Simulator.

## M4 — Maintain the canonical mathematics and its implementation correspondence

This is a completion requirement, not another research project. Import this prompt
once into the programme's research inputs. Preserve and reference existing
arguments instead of duplicating their chapters. External scripts are optional;
unavailable scripts must not be represented as read or executed. Small independent
oracles may be authored where they test an actual premise; label them separately
from native validation.

Give every material finding a disposition: incorporated, already present, open,
refuted, out of scope or blocked. Accepted reasoning belongs at canonical anchors,
not only in this prompt, HANDOFF or an inaccessible attachment.

A. docs/solver/mathematics/search-and-resumption.md
Integrate section 3's finite-prefix committed-witness argument, artifact lifetime,
result-selection point and cooperative-transaction proof, with numerical, liveness
and performance qualifications. Suggested anchors:
  interruptible-verified-results
  cooperative-refinement
Include logical passivity versus timed overhead. Present conditional mathematics
as conditional; separately identify tested native correspondence.

B. docs/solver/mathematics/policies.md
Existing cost/count rewards, changed-controller occupancy, stopped laws, graph-local
boundaries and clean-entry reasoning already exist. Add stable anchors and only
necessary rooted-reuse/predicate clarifications. The general policy-difference link
currently points to the scalar first-return section; give the earlier general
identity its own policy-difference anchor and repair that link. Preserve legitimate
first-return links. Suggested other anchors: rooted-policy-reuse,
graph-local-boundaries, implied-entry-predicates.

C. docs/solver/resources-resume-replay.md and upper-authority.md
Map the actual implemented request-ID owner, safe-point latch, selection/sealing,
evidence transfer, cancellation order, changed suspension boundaries and frozen
memory accounting to the canonical derivations. Keep Finish distinct from Cancel
and the unchanged timed default. Do not claim prospective behaviour is shipped.

D. docs/solver/benchmarking.md and affected docs/engine/wasm.md
Record actual clocks, partial trace coverage, control stages, response measurements,
compatibility and export behaviour. Keep failed worker qualification and synchronous
setup visible until new evidence changes their disposition.

E. docs/solver/claims.md
Append scoped application histories for relevant existing CLM-0002, CLM-0004,
CLM-0021 and CLM-0022 using actual dates/reviewers/evidence. Preserve original
propositions, preconditions and failed experiments. The reviewed statuses are
CLM-0002 accepted; CLM-0004/0021/0022 broadly open. Verify current entries and do not
promote broad statuses from a local passing test. The finite-prefix result is a
corollary, not a reason to create claim IDs for buttons or labels.

F. docs/solver/research.md#rq-003
Briefly connect the already-committed Bow recovery, earlier same-policy Conquest
verification and native Ring recovery. Keep verification distinct from default
delivery, the M11/M12 identities separate, and full-scope proof open. Add this
programme's actual Finish result only after qualification; do not copy a full
experiment diary into the canonical research question.

Repair navigation described as current that still points to historical mechanism
versions: use current relative links or explicitly label the history. Preserve
genuine historical citations and authored dates. Do not hand-edit generated
research-state.md, invent metadata for an unsupported series, or duplicate status
across new indexes/databases.

Run focused edited-link/diff review and the existing solver_knowledge lint --base
4594e44b6b851511ae5471e52f232bce57d16856 when supported. Missing base/tool evidence
is not a pass. Lint checks references/history rules, not mathematical truth.

## 4. Final acceptance and handoff

Report separately: export/label correctness; early usable-strategy timing;
unchanged unattended quality; post-start step/control responsiveness; remaining
setup limits; actual mathematical integration; and full-scope proof status.
Preserve CB03/CB05/CB09 and Ring browser-default limitations unless independently
resolved. A flat upper is not an optimal-stopping certificate.

Identify import location, canonical anchors, claim-history dispositions, actual
source/build/request identities, emitted graphs, synthetic versus native versus
timing checks, failures/unrun checks, and true commit/push status. No blanket
“everything passed” when worker responsiveness or another gate remains unmet.

Finish this bounded programme and return a self-contained text handoff. Do not
append the action-budget optimizer, another global search campaign, or automatic
successor work. Do not push without separate authorization.
