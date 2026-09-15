# Solver progress, earlier policy delivery, and graph-local continuation

Research revision: `fe5bdcb4111fa6785c5615896b993a6fc713d1a1` (`OliverOrton/poecraft2`, remote main checked September 14, 2026).

**Recommendation:** one observability-and-delivery programme with separately qualified pre-incumbent and post-incumbent branches. Do not install periodic global sweeps from screenshot round counts. First preserve meaningful progress, then consume a demonstrated ready candidate or missing continuation at a bounded safe opportunity. Preserve the unattempted graph-local Ring delivery work instead of replacing it with an open-ended instrumentation project.

This is a static repository/receipt review and analysis of Oliver's screenshots, not a native performance experiment. No repository modifications, native builds, solver runs, simulations or deployed-browser hash checks were performed here. The companion checks validate the screenshot arithmetic and small mathematical/trigger examples only. The prior graph-local implementation plan is **unattempted**, according to Oliver's correction; references to it as currently running are superseded.

## 1. The decisive new source finding: four minutes is a requested boundary

The Calculator calls `solverSolve` with `boundedFinishAfterMs: 4 * 60 * 1000`, explicitly reserving the final minute of a five-minute product boundary for compilation/certification/evaluation. Thus the screenshot's certification near 4:02 and first displayed verified upper near 4:03 are consistent with requested bounded-finish processing, not evidence of spontaneous convergence at round 271. UI and worker clocks have different start points, so the screenshot does not establish the exact request/acknowledgement timestamp. [R03]

This changes the useful question from “why does optimization finally work after hundreds of cycles?” to:

> At what earlier time did a complete usable candidate exist, and which work owner prevented its verification or delivery before the requested finish?

There are at least three possible answers: the candidate was not complete; it was complete but checking was deferred; or a different finalization route made a new complete candidate. Screenshots alone do not choose among them. A finite working value is not evidence that a materialized proper candidate exists. [R05–R08, R25]

The user explicitly deprioritized the apparent final publishing wait because this build completed on an earlier attempt. Keep coarse export/UI/release stage visibility, but do not select a hang investigation or browser rewrite on this observation alone. The Calculator runs a separate one-second elapsed timer; it also clears that timer before awaiting final handle releases. Therefore the previously suggested explanation that time only advances on solver callbacks is not accurate for this source. The frozen screen is not a measurement of native CPU activity. [R03]

## 2. What the screenshot series establishes

The full manual transcription is in `SCREENSHOT_EVIDENCE.json` and `SCREENSHOT_TRACE.csv`. Values are displayed/rounded observations, not raw native telemetry.

| UI time | Expanded / discovered | Frontier | Rows | Transitions | Reforge work | Sweeps | Focused round | Verified upper |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 0:30 | 3,021 / 10,872 | 7,851 | 33,666 | 169,673 | 1,125,529 | 0 | 40 | Pending |
| 1:07 | 3,277 / 11,331 | 8,054 | 38,729 | 201,263 | 1,756,126 | 4 | 79 | Pending |
| 1:36 | 3,277 / 11,331 | 8,054 | 38,926 | 202,130 | 1,966,325 | 1 | 112 | Pending |
| 2:03 | 3,277 / 11,331 | 8,054 | 38,974 | 202,130 | 1,966,325 | 0 | 142 | Pending |
| 2:40 | 3,277 / 11,331 | 8,054 | 39,094 | 202,130 | 1,966,325 | 0 | 183 | Pending |
| 3:17 | 3,277 / 11,331 | 8,054 | 39,364 | 202,130 | 1,966,325 | 0 | 224 | Pending |
| 3:37 | 3,277 / 11,331 | 8,054 | 39,516 | 202,130 | 1,966,325 | 0 | 246 | Pending |
| 4:02 | 3,277 / 11,331 | 8,054 | 39,623 | 202,130 | 1,966,325 | 0 | 271 | Pending |
| 4:03 | 3,277 / 11,331 | 8,054 | 39,623 | 202,130 | 1,047,952 | 0 | 271 | 85,558.55 |

Between 1:36 and 3:37, 121 displayed seconds pass, focused rounds rise by 134 and rows rise by 590, while displayed states, frontier, transition storage and reforge work are unchanged. This is a clear **outer-counter plateau**, not proof that the complete graph has stopped changing internally, that all alternatives have been compared, or that expansion has finished. In particular, 8,054 frontier states are still displayed; that number is not a closure certificate. Current source computes these public counts from the outer calculator/cache. [R05]

Rows and transition storage do not have a one-to-one relationship. The expansion owner can reuse an existing transition span for a new row. Thus rows increasing while transition entries remain fixed could include useful new decisions with shared stochastic support. It does not establish redundant work, absent action evaluation, or lost transitions. Record completed row kind, admission, shared-support reuse and selected-policy changes before drawing that conclusion. [R17]

The screenshots show no verified upper at the captured points before 4:03. They do not prove that no candidate or short-lived checking phase occurred between captures. Likewise, an 8,804-pair certification snapshot at 4:02 and a publication snapshot at 4:03 do not establish that certification took one second, or that it is the dominant cost center. The memory increase to 212.3 MiB proves a sampled increase, not a timed attribution.

## 3. Counter semantics are partly explainable now, not an entirely open research task

### Sweeps are resettable by design

`progress()` returns the live `sweeps` variable. `advance_focused_lower_preparation` and `reset_focused_optimization_state` set it to zero. The latter also resets policy-improvement rounds, Bellman backup/action counters and optimization timing fields. The displayed 4 → 1 → 0 therefore fits phase/round-local measurement. Do not infer lifetime work by summing positive screenshot deltas, and do not feed these resettable values to an unsigned delta calculator. [R05, R07, R08]

Focused rounds also have a precise but limited meaning: `finish_focused_lower_solve` increments the round counter when entering its constructive/fallback work for a completed lower pass. It does not mean a new state was expanded, all native actions were reconsidered, or a verified policy was improved. The public phase “expanding” covers several owners. The native `current_phase_owner()` already distinguishes ladder scheduling, dependency preparation and state-local automatic synthesis, but the main progress panel renders a phase-derived description. [R04, R05, R08]

### “Candidate estimate” is an active working value, not consistently a candidate price

Before finalization, `start_value_bound` comes directly from `result.values[result.start_state]`. Focused lower preparation fills that array with lower-model values; upper passes save/swap it and use a different candidate domain. During finalization, progress instead reads the finalized result. The jumps from about 405 to 356.9 million and back therefore do not by themselves establish instability of one fixed policy or failure to preserve the cheapest verified policy. [R05, R07, R08]

The existing portfolio diagnostics already distinguish candidate source, stage, identity and verified status. Prefer that information for an actual candidate display. Preserve `start_value_bound` as a raw compatibility field, and label it with a working-value role/domain instead of silently changing its meaning. Use the independently verified upper for publication and cost targets. [R06, R25]

### Reforge work switches source at finalization

Live progress reads the calculator's reforge logical work; finalized progress reads finalized diagnostic work. The final decrease of 918,373 displayed units is therefore a real diagnostic reconciliation question, but not evidence that computation was undone or that a specific cap was bypassed. Audit charging/rollback/aggregation at that transition. Do not “repair” it by clamping every reported number to its maximum. Keep raw values, ownership and generation, with a separately defined lifetime performed-work counter only where it can be maintained accurately. [R05]

### Missing request identity prevents a claimed screenshot reproduction

The goal image shows Rare level-86 Conquest, three T1 armour/evasion prefixes and two T1 suffixes, ordered Physical Damage Reduction then Spell Suppression. The start item, actual price snapshot/base price, ordered goal identifiers, admitted priced actions, disabled-family mask and loaded WASM hash are not shown. The “11 missing” price-table label is not proof that eleven selected actions lacked prices during solving. Calculator constructs a priced subset before solving. [R03]

Use CB01 as an explicit repository surrogate when no exact export is present, retain any goal-order differences, and never pretend its same-looking target or roughly 85,558 cost establishes full identity. A loaded browser worker can also retain earlier module bytes. Record native source/build and WASM/JS/data hashes in new artifacts rather than treating “main” as runtime identity. [R03, R18]

## 4. An important prior failed experiment constrains the new trigger

The September 9 C9 experiment already changed candidate readiness so a waiting attempt could resume when its missing source gained a completed priced row, without waiting for a geometric row checkpoint. Its recorded WASM result was **324 attempts, 327 refinement rounds, only 682 expanded states**, and a worsened verified upper of **30,267,250.77**. The trigger was removed. The completed record states that immediate single-dependency resumes could starve ordinary alternative discovery. [R18]

That result does not forbid earlier checks. It does forbid presenting readiness alone as a newly established solution. The new design must bind readiness to a specific candidate, preserve compatible work, limit re-entry, deduplicate unchanged attempts, and charge time/work against the existing budget. Broadly invoking assembly after every new row is not selected.

The same history contains successful sibling discovery and named-continuation service, and a later WASM result around 85,558.706 after the original 60-second finish. That historical qualified finish does not prove the current screenshot's candidate was available after 60 seconds: goals/order/economy, build, timing, and the fact that a short finish changes the algorithmic path all matter. It does justify investigating whether today's 240-second run defers useful publication. [R18]

## 5. What telemetry exists, and the exact missing link

The native benchmark has a `--progress` text mode (roughly ten-second observations), one-second default bound sampling, focused-round/incumbent-kind/done samples, and atomic partial reports. Its current `BoundTraceEntry` omits sweeps, residual, active working estimate and several refinement/certification fields available in live progress. More subtly, the sampling predicate does **not** explicitly trigger on every phase-owner change or on a cheaper replacement that keeps the same incumbent kind. Those events can appear only at the next sampled checkpoint. [R09, R10, R24]

The system also contains candidate lineage, action-envelope accounting and bounded diagnostic samples. Extend these owners; do not add a second experiment catalogue, supervisor, generalized logging framework or dashboard service. The Lab already has a CLI and optional GUI. [R06, R14, R15, R22, R23]

Two additive views are enough:

1. A compact sampled progress projection containing the useful web fields plus source/owner/generation, actual native mode, observation age and current candidate identity/stage. Persist the corresponding native and worker records.
2. A small native event stream for real lifecycle transitions: candidate captured, support ready, queued, serviced, blocked/refused, checking begun/completed, verified incumbent retained, finish requested/acknowledged, native done, export/UI done. Keep source timestamps distinct from host observation timestamps.

Do not call the full `telemetry_snapshot()` at every hot step. It copies diagnostics and scans states/rows. Update cheap counters at their actual owner and drain bounded event records at existing cooperative boundaries. Sparse snapshots and event drops must remain explicit; a historical snapshot cannot be converted into an exact earlier event time. [R06, R10]

Diagnostic resource use is still real resource use. Charge native retained buffers honestly, bound host output, and measure trace overhead separately. Fixed-work tests should establish passive instrumentation preserves selected decisions; timed tests then include measurement overhead rather than hiding it.

## 6. Separate the two delivery gaps

### Before the first verified policy: Conquest-five

At the captured middle points the verified upper is pending. Post-incumbent graph-local improvement cannot explain or fix this interval by itself: it requires a verified base controller. The next diagnostic must identify whether a selected candidate is complete, which exact continuation is missing, whether the checker is already active, and what changes at the finish request. [R05, R14, R15, R25]

If a complete priced/proper candidate is ready, test one earlier use of its existing checker. If support is incomplete, service its named missing dependency through the retained owner. If numeric values alone are stale on a complete domain, test one cooperative broader improvement pass there. If a checker is progressing, do not interrupt it with a new sweep. “High focused rounds” should initially request an assessment, not select the remedy.

### After a verified private policy exists: Ring

The previous graph-local plan remains valuable and unimplemented. The outer entry trigger requires Fracture and parent decision bindings. Private publication deliberately clears the bindings and retains a root-only graph. Its values cannot be transplanted into the parent namespace. The inner entry contract also requires genuine identities and global routing. Removing only a fracture string check is insufficient and removing the binding protection is unsafe. [R11–R13, R25]

Preserve compiler-authored graph-local decision provenance alongside a root-only artifact, remap it on composition, and certify actual reached items at legitimate global decision boundaries. This supports bounded improvement without creating parent statewise authority. Retain the current three-entry shortlist and cost-only local comparison initially. Service one new graph/query wave, with a second wave only if a distinct later winner requires it and the experiment declares that bound. Do not recursively re-enter the active builder or recover private IDs by dummy metadata.

The known Ring target remains **220,743.46354691783**, compared with native **227,377.06545019808**. Recover it through native search from the original request, not by seeding the diagnostic graph. Keep Bow **12,770.827062219498**, native-wide Amulet **12,541.579648544115**, and separately scoped native Conquest-four **3,746.1319409485764** as preservation references. [R19–R21]

## 7. Broader sweeps: the narrowly useful interpretation

Existing policy-selection and component-evaluation code already performs substantial fixed-policy and Bellman work. A broader pass should reuse that code over a named completed domain, not introduce another linear solver or eagerly generate the entire action catalogue. [R16]

The mathematical point is simple. For a fixed proper finite controller, its value satisfies `J = c + P J`. Re-solving the same equation more thoroughly can correct an unconverged numerical estimate, but it cannot invent an absent alternative or complete an uncovered successor. A local patch repeated on every revisit changes the controller; its entire root-reachable law must be rechecked. [R25, R27]

Prioritized sweeping motivates servicing consequential updates instead of repeatedly processing everything [E01]. Topological value iteration motivates inspecting the actual component structure; FTVI also depends on *proved* action elimination, not simply low incumbent usage [E02]. These are design checks, not transferred native speed guarantees. The repository already has component machinery, so the selected first experiment concerns service timing and missing support, not adoption of TVI or a novelty claim.

## 8. Action budgets remain a separate product problem

The discussion of user action limits must survive in the successor record, but this implementation must not quietly change the minimum-expected-chaos objective. `E[T] <= B` is different from `P(T <= B) >= q`; a hard stopped session also has different cost semantics from a proper policy that continues beyond B. No default reliability threshold has been selected by Oliver. [E03; earlier supplied design note]

Keep actual primitive counts, option setup/retry/cleanup accounting, and policy identity in the new telemetry. First qualify earlier delivery and graph-local reuse. Then choose the action-budget contract and implement fixed-policy budget evaluation before budget-aware synthesis. A remaining-budget dimension can enlarge the state space; it is not a guaranteed remedy for present state explosion.

## 9. Selected programme and acceptance

Implement minimal trace parity and meaningful event capture first. Obtain one controlled Conquest-five trajectory and one Ring service witness using current owners; reuse a saved compatible witness wherever it already contains the required information. Do not begin with four expensive baseline repetitions.

Next, keep Conquest pre-incumbent service and Ring graph-local service as separate treatment receipts, then integrate and qualify the affected controls. A timing claim requires earlier **verification of the same predeclared useful target**, not a lower estimate appearing sooner. A cost-delivery claim requires native discovery and returned original-root evaluation. A new cheaper policy found under a changed profile is not a timing-only result.

The detailed self-contained programme, tests, stop rules, profile references and Codex operating instructions are in `CODEX_PROMPT.md`. That file does not depend on this report, the original plan, screenshots, a ZIP download, or files on Oliver's home machine to begin work.
