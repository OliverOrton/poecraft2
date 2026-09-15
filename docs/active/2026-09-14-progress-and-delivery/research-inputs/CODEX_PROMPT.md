# Explainable solver progress and timely executable policies

Repository: OliverOrton/poecraft2
Research baseline: fe5bdcb4111fa6785c5615896b993a6fc713d1a1
Task: implement this staged programme, using separate evidence for observability,
earlier Conquest policy delivery, and Ring graph-local recovery.

## 0. Owner intent and operating rules

Oliver wants to understand what the solver actually does over time, make useful
work happen earlier where justified, and retain the earlier proposal for
improving verified private policies. The previous downloadable plan was
inaccessible to the implementation session. Do not assume it was implemented or
is currently running. Check local source and HANDOFF, preserve genuine work
already present, and reconcile it with this baseline.

This prompt is complete enough to start without earlier chats, attachments,
screenshots, ZIPs, or access to Oliver's home machine.

Follow AGENTS.md. Work sequentially, without subagents, an inherited deadline,
automatic restart, destructive cleanup, or push. Preserve unrelated changes and
protected root 0 without inspecting, staging, modifying, or deleting it. Keep
commits local unless Oliver separately authorizes publication. Native C++ remains
mechanics authority; Oliver decides ambiguous mechanics. Do not research PoE
mechanics externally or hand-edit SQLite/compiled data.

Use one living implementation record and a short HANDOFF. Reuse the benchmark,
corpus runner, Lab, compiler, evaluator, portfolio and identity owners. No second
scheduler, catalogue, dashboard service, generic tracing framework or policy
library. Tests and runs must resolve a named uncertainty or qualify a retained
change; no routine full suites, repeated censuses or simulation of unchanged
strategies.

## 1. Outcome and non-goals

Deliver a useful live/persistent view of solver progression, then make one
evidence-backed improvement to pre-first-policy delivery on Conquest-five and
recover post-incumbent entry service for Ring through legitimate graph-local
provenance. Keep these treatments independently attributable.

A negative Conquest scheduling experiment may leave a useful diagnostic result;
it must not be relabelled as a speedup.

Do not implement blanket global sweeps, broaden every action family, change
canonical prices/probabilities/terminal semantics, weaken publication, or
implement the user action-budget optimizer in this programme.

Expected primitive count remains a separate measured reward; minimum expected
Chaos remains the optimization objective. Hard budgets need a separately
selected completion-risk/cost contract and duration-aware evaluation, not a count
weight or a simulator-cap change.

## 2. Evidence and corrections already established

The inspected Calculator
(apps/web/src/app/components/pc-calculator.ts, startSolve)
explicitly passes:

    boundedFinishAfterMs: 4 * 60 * 1000

Therefore first displayed certification near four minutes may be finish-triggered,
not convergence. Log request and acknowledgement separately from candidate
readiness and checking. Do not shorten the finish deadline to claim earlier
discovery.

Oliver's Conquest-five screenshots show Rare, level 86, three T1 Armour/Evasion
prefixes, T1 Physical Damage Reduction followed by T1 Spell Suppression. Restart
and automatic Imprint are unchecked.

Displayed samples:

Seconds  Expanded Discovered Frontier Rows  Transitions Reforge  Sweeps Round U
30       3021     10872      7851     33666 169673      1125529  0      40    pending
67       3277     11331      8054     38729 201263      1756126  4      79    pending
96       3277     11331      8054     38926 202130      1966325  1      112   pending
123      3277     11331      8054     38974 202130      1966325  0      142   pending
160      3277     11331      8054     39094 202130      1966325  0      183   pending
197      3277     11331      8054     39364 202130      1966325  0      224   pending
217      3277     11331      8054     39516 202130      1966325  0      246   pending
242      3277     11331      8054     39623 202130      1966325  0      271   pending
243      3277     11331      8054     39623 202130      1047952  0      271   85558.55

The lower stays 405.3694. The displayed working estimate is usually 405.3694,
becomes 356873548.27 at 197/242 seconds, and becomes 85558.55 at publication.
Certification pairs first appear in a supplied sample at 242 seconds (8804);
memory moves from approximately 157 MiB to 212.3 MiB, then 157.6 MiB.

These are rounded snapshots, not an exact event trace.

Critical qualifications:

- The 96-to-217-second interval has 134 additional rounds and 590 rows with
  unchanged displayed outer states/transitions/work. It still has 8054 frontier
  states. Expansion is not proved complete.
- Rows can share stored transition spans. More rows without more transition
  entries can be useful action coverage, not redundant work.
- progress() reports live sweeps; focused preparation/reset explicitly clears it
  and several numerical diagnostics. These are not lifetime totals.
- start_value_bound reads result.values[root]; that workspace changes between
  lower and upper passes. Label its role rather than treating it as the best
  candidate's cost.
- Live and finalized reforge_work read different sources. Audit the decrease
  without clamping it away or claiming work was undone.
- A one-second gap between screenshots does not measure total certification time.
- Actual start item, prices/base price, ordered goal IDs, disabled-family mask and
  loaded WASM/JS hashes are unavailable. "11 missing" in the price panel does not
  mean eleven admitted solve actions were unpriced. Do not claim exact screenshot
  reproduction.
- Oliver deprioritized the apparent final publishing wait. Include coarse
  native/worker/export/cleanup stage and observation-age visibility; do not turn
  this into a hang investigation without new evidence.

The September 9 C9 experiment is a mandatory negative control: immediately
retrying a waiting candidate after one missing source received a priced row
produced 324 attempts, 327 refinement rounds, only 682 expanded states, and U
about 30.27M. It was removed.

Do not recreate retry-on-every-dependency or retry-on-every-row. Preserve ordinary
discovery and bounded candidate ownership.

## 3. Read the existing owners, not the entire archive

Start with AGENTS, HANDOFF, docs/foundation/tooling.md,
docs/solver/benchmarking.md, docs/solver/scheduling-bellman.md, and the relevant
portions of upper-authority.md and mathematics/policies.md.

Then inspect:

- solver_solve_telemetry.cpp: current_phase_owner, progress,
  refresh_incumbent_portfolio_diagnostics, and expensive telemetry_snapshot.
- solver_solve_focused.cpp: lower preparation/reset, upper-pass workspace swaps,
  and focused-round completion.
- solver_solve_incremental.cpp, solver_solve_finish.cpp,
  solver_solve_return_bridge.cpp: initial candidate checking, missing-continuation
  service, publication, execution_bottleneck_ready, private artifact retention.
- solver_compile.cpp, solver_eval_types.hpp, retained-artifact structures and
  relevant evaluator implementation: authored decision boundaries, composition
  remapping, actual global routing and entry certificates.
- engine/benchmarks/solver_benchmark.cpp: BoundTraceEntry, record_bound_trace,
  sampling predicate, progress/finish/extraction.
- Web pc-calculator.ts, engine-worker.ts, engine-client.ts, protocol and WASM
  serialization; current Lab/report parsers when extending their views.

Use docs/active/2026-09-14-cross-base-strategy-recovery/ for current receipts.
Read the C9 and C14/C15 sections of:
docs/active/2026-09-09-empty-start-partial-continuation/README.md

The historical success is not a current-run timing baseline. Consult other
archives only for a specific unresolved question.

## M0 — Freeze identities and define the live questions

Record local source/dirty/build identities, native binary and WASM/JS/data hashes,
full ordered goal and actual start, pinned economy/overrides, admitted priced
action IDs, dependency/scope controls, activation, capacities and clock origins.
Preserve original raw receipts.

Reuse the exact current request export when available. Otherwise use repository
CB01 empty-Rare Conquest-five as an explicitly labelled surrogate, not a reason
to block work until Oliver returns home.

Keep two distinct experiment profiles:

Conquest product-oriented lane:
docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json
CB01 primary and CB02 preservation, with their 240-second finish, 300-second
native and 315-second host boundary, 1-GiB budget and product step eight.

Match actual retention activation to the intended WASM comparison. Derive any
necessary case through the existing owner; never overwrite a frozen case.

Ring/Bow development lane:
docs/active/2026-09-13-execution-aware-proposals/native-600-final840-400m/manifest.json
CB07/CB04 and affected CB08.

Preserve 600/840/870-second boundaries, 8-GiB aggregate, 4-GiB checker/final
evaluator, 400M shared reforge work and existing other caps. Recover resolved
treatment/argv, including execution-count activation/action price where present;
the filename or case's stored default is insufficient. Use current safe host
admission. Do not convert these to browser-default performance claims.

Record a compact field-semantics table: source owner, namespace, reset/generation,
units, whether cumulative, and authority. The source findings above are already
known; confirm applicability rather than spending a phase rediscovering them.
Do not infer lifetime work by adding positive sampled deltas.

The Conquest question is earliest complete candidate versus first verification
versus delivery. The Ring question is actual private-artifact provenance and
entry-service eligibility. State what evidence distinguishes missing support,
delayed service, numerical work, checker cost and publication cost.

## M1 — Minimal useful observability in existing surfaces

Extend existing progress records additively with useful web fields missing from
saved traces: sweeps/residual, working-value role, candidate identity/source/stage,
refinement/certification counts, finalization cursor and active work owner.

Keep outer search, private candidate, strict proof and evaluator units distinct.
Use existing cheap counters; add genuine lifetime counters at owner operations
only when needed. A reset/generation marker is preferable to a fabricated total.

The current native trace samples on interval, focused round, incumbent kind, and
done. Add observation on meaningful owner changes and verified identity/value
changes, including same-kind replacements. Preserve periodic snapshots and source
events separately.

Capture a bounded chronological event stream at actual lifecycle sites:

candidate captured; named missing support; support ready; queued/service start;
blocked/refused/completed; compile/check start/end; incumbent retained; finish
requested/acknowledged; native done.

Add corresponding worker/export/cleanup/UI-delivery milestones where needed.
Distinguish native event time from later host observation time. Do not fabricate
events inside a blocking call or backdate a sampled upper.

Prefer extending current candidate lineage/diagnostic owners. Do not copy the
complete telemetry snapshot every step: it scans states/rows. Bound and charge
native retained buffers, drain through existing cooperative transport, aggregate
high-volume row activity, and report omitted/dropped events and observation gaps.

No per-backup logging, full-policy checking from a status read, or observer-
triggered solver work. Every denominator has a scope: registry descriptors,
primitive candidates, operators, legal pairs, complete rows, verified candidates
and selected policies are different quantities.

Make this usable during development: expose actual owner, working-value role,
recent changes, candidate stage, ready/waiting reason, and last-native-update age
in the existing Calculator diagnostic area or Lab view. Retain/export the same
trace for later inspection. Native terminal/report access must work without a
GUI. Extend an existing bounded reader/view, not another service. No cosmetic
redesign is required.

Test passive instrumentation at a deterministic small work boundary, including
reset handling, same-kind replacement, truncation, and finish. Measure timed
overhead on an already needed matched run. Use 5% as a proposed investigation
threshold, not an excuse to enlarge budgets or a claim that one noisy timing
proves an overhead rate. Reduce capture cost if material.

Observability may qualify independently, but it is not a strategy-quality success.

## M2 — One decisive Conquest trace and one Ring witness

Reuse compatible saved evidence first. Where fields are missing, use one
justified instrumented baseline per primary, serially, not a four-base census.

For Conquest, locate the earliest selected candidate with complete positive
support and times of actual compilation/checking/retention. Count attempts and
meaningful changes, completed versus pending dependencies, new versus reused
rows, numerical passes and checker activity by owner. Determine what actually
changes at the four-minute request.

A flat outer graph and many rounds justify a diagnostic assessment, not a claim
of numerical stagnation.

For Ring, record the real fallback at publication and at a possible service
boundary: graph/portfolio identity, root-only status, parent-binding count,
available local metadata, fracture flag, attempted guards, active task and
remaining resources.

Locate the known cheaper diagnostic's physical entry as an acceptance witness;
check native automatic synthesis and full kernel support there. Do not seed
product search with that graph or hardcode item/modifier/node IDs.

Produce a compact causal verdict before runtime scheduling changes. If a candidate
is not ready, say what is missing. If ready but waiting, identify the guard/owner
and delay. If checking is progressing, do not label it stalled from outer counters.

Classifier output is initially observational, versioned, and explicit when
insufficient evidence prevents a diagnosis.

## M3 — One bounded pre-incumbent intervention, selected from evidence

Choose the smallest supported branch:

1. Complete candidate waiting:
   invoke its existing cooperative compiler/evaluator earlier at a safe owner
   boundary, retaining the frozen candidate and ordinary continuation cursor.

2. Missing continuation:
   service one bounded set of named dependencies through the existing owner,
   then return ordinary discovery its turn. Do not reproduce C9's retry loop.

3. Complete numerical domain with unconsumed information:
   test one cooperative broader policy-improvement pass using existing numerical
   machinery and complete compatible rows. Do not synthesize all missing actions
   or use unknown frontier values as executable tails.

4. Checker progressing or cause unresolved:
   retain instrumentation and the precise obstruction. Do not force a sweep to
   complete this milestone.

High round counts may request assessment. Service requires a valid candidate/
domain, useful changed support or unconsumed work, a safe checkpoint, and
available resources.

Bind attempts to semantic candidate/support identity. Unrelated row growth and
cumulative counters cannot perpetually re-arm them. No recursive builder/checker
entry, lost ordinary work cursor, or repeated reset of unfinished expensive work.
A no-benefit pass on unchanged inputs stays closed until a meaningful premise
changes.

Keep deadlines, candidate vocabulary, ranking, prices and total capacities fixed
for the causal comparison. Do not conflate --proof-handoff-seconds with a user
finish or enable it silently. Preserve verified fallbacks and the original
lower/closure ledger. Missing support remains unknown; a partial-model optimum
is not full-scope exactness.

Predeclare the useful cost target from the compatible baseline before treatment.
Measure time to first verified policy and time to that target, all-in
preparation/checking/delivery, with final quality as preservation.

A 10% earlier same-target result is the proposed material timing milestone and
requires one counterbalanced confirmation when observed. A changed cheaper policy
may qualify separately, not as a timing-only effect. Stop after two substantive
failed variants without a materially different premise; build/test typos are not
such experiments.

## M4 — Graph-local Ring improvement without false parent authority

Keep this separate from the Conquest treatment. The current outer trigger requires
Fracture and parent bindings; private publication intentionally clears those
bindings and marks the result root-only.

Do not remove that protection, insert dummy IDs, or pretend deleting the fracture
test is the whole repair.

Preserve compact compiler-authored graph-local decision provenance in the existing
artifact route. Parent-bound bindings retain their meaning. Bind local metadata
to immutable graph, native operation semantics and real observation/control
boundaries, not transient private numeric IDs.

Composition must use its actual node/router remapping to carry or explicitly
refuse metadata. Old artifacts without recoverable provenance remain valid root
policies but unavailable to this feature.

Extend the existing evaluator to certify actual reached item/control entries
through those declarations and the real global router. Refuse mandatory option
interiors, active hidden offers/checkpoints, stale graph/price/scope/data identities
and wrong routes.

A graph-local entry does not supply a parent statewise upper, lower, equivalence,
retirement or foreign row handle. Initialize local work from the physical entry
and native descriptors.

Keep every positive-mass exit: true clean goal, a specifically compatible certified
old continuation, or new complete paid recovery. A representative cannot certify
a heterogeneous class. Re-evaluate the entire recurring original-root composition
before retaining it; old occupancy times a local delta is not acceptance.

Start with one ordinary service wave, at most the existing three clean Rare
one-goal-missing entries, same ranking, cost-only local improvement and unchanged
incoming proposal treatment. Preserve the Magic opportunity separately.

A second wave requires a distinct later verified winner and recorded need, with
an explicit maximum of two and unchanged global limits. No re-entry on every tick
or dependency. If the witness misses the shortlist, record why before selecting
a separate narrow coverage adjustment.

Native discovery target:
C <= 220743.46354691783 under existing reconciliation tolerance,
compared with current native Ring 227377.06545019808.

The diagnostic is a test target, never a runtime seed. Another equivalent or
cheaper native graph is acceptable. Keep root-only parent authority intact and
preserve the inherited root-Chaos compatibility refusal separately unless a
distinct justified fix is selected.

## M5 — Focused qualification and closeout

Required native negatives/positives cover:

resettable counters; native/source versus sampled time; same-kind replacements;
no duplicate unchanged query; no early retry storm; missing-support refusal;
tiny positive trap; actual global routing; private-ID namespace mismatch;
mandatory interior refusal; composition remapping; changed graph/goal/price/scope
identity; finish/abandon preserving the cheapest verified artifact.

Include a small genuine second-generation improvement fixture: a private policy
is improved, retains legitimate boundaries, and can be improved again without
parent IDs.

Preserve current compatible references:

Bow:
C=12770.827062219498, N=66914.72854184538

Ring diagnostic target:
C=220743.46354691783, N=564673.7576801177

Native-wide Amulet:
C=12541.579648544115, N=25518.491313143768

Native Conquest-four:
C=3746.1319409485764, N=8608.881793656798

Exact Regalia:
C=65.60036144971359

Conquest-five screenshot C=85558.55 is rounded and request-unbound, not a byte-exact
baseline. Preserve CB03/CB05/CB09 historical limitations. Separate weaker
original-profile Amulet results from its stronger development reference.

Use retained-policies.json and qualification.json under the current cross-base
record for exact graphs/hashes. Run necessary matched treatments and affected
preservation controls once at integration; no automatic fourteen-case campaign.
Keep trace-only, Conquest scheduling and Ring service changes distinguishable.

Reuse compatible baseline receipts. Timing claims require matched machine/load/
identities and a same-treatment repeat when material.

Every changed returned graph needs complete independent native original-root
evaluation. Run owner-approved 1000 execution trials only for genuinely changed
graphs when fresh execution qualification is needed; retain the 100000-action/
4000000-step limits and all censored trials. No routine unchanged-policy Simulator.

Rebuild native/WASM and check affected protocol, compilation, finish/abandon and
TypeScript paths. Include an actual relevant WASM worker solve for a browser-
delivery claim. Fixed-graph WASM evaluation is not browser search. Ring's existing
100000-state default refusal stays distinct from larger diagnostic evaluation.
Do not raise browser caps.

Deliver source/build/request identities, semantic counter table, inspectable
live/exported traces, readiness-to-service-to-verification timings, action/
continuation coverage attribution, emitted graph identities, matched outcomes,
tests actually run and failures/unrun checks.

State separately whether observability qualified, Conquest got a useful verified
policy earlier, Ring natively recovered its diagnostic, and full-scope proof
remained open. Update existing canonical knowledge only for retained findings.

If the machine cannot run the required native build/profile, complete source/
fixture work that is possible and leave native performance explicitly unqualified.
Do not substitute screenshots, synthetic models, inflated caps or an unexecuted
command for a measured result.

Finish with a text handoff understandable without home-directory artifacts.
No push or automatic successor work is authorized by this prompt.