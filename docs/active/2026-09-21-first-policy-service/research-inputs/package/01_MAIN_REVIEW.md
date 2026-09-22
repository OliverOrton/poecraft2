# Current-main review and directional decision

## 0. Completed hosted CI changes the ordering

Windows run **35618066993** now completes with **three of 17 native CTest suites failing**. Build, frozen-data preparation and Python validation pass. The knowledge workflow **35618067061** succeeds. The previous timestamp prerequisite is not the active failure. [R23]

The current log reports 17 solve assertions (constructive certificate behavior, shared-Fracture versus separate-lift cost/consumption, and one-step expansion staging), four API phase-owner assertions, and an uncaught S8.3 interval exception: `bounded incumbent evaluation violates L <= J_pi <= U`. The actual `L/J_pi/U` tuple and failing S8.3 subcase are missing, so this is neither evidence for dismissing all failures as stale tests nor proof of a wrong published user result. The guard is fail-closed. [R24–R28]

**First selected milestone: the focused integrity/consumer-contract repair in [08](08_INTEGRITY_GATE.md).** Performance attribution and implementation are conditional on that gate. Preserve the positive setup controls; the broader new failures do not make those measurements disappear. Equally, the selected C5/Regalia passes do not qualify every ordinary/automatic/partial-publication path. Hosted web/typecheck was not reached after the native failure; prior local passes remain separately scoped.

## 1. What actually landed

Main is `a48f055ccc07751807c334386f1fd25c6b4717de`, “Stage solver setup and qualify complete cancellation.” It is two commits ahead of the previous `0f376f4` baseline: S0/S1 are in parent `5a923d1fa57c37319a82bd838c95412ed93738df`, followed by staged setup and qualification. The completed programme is `docs/active/2026-09-20-cooperative-setup/`. [R1–R4]

**Recorded:** S0 reproduced the actual PowerShell timestamp defect locally: a literal timestamp became `08/30/2026 02:54:53`; the failing manifest differed only at `/generated_at_utc`. The compiler now reads the literal timestamp from the lock in Python. Frozen expected hashes, payloads and economy were not changed. This is not proof that the reviewer obtained the old hosted failed manifest; that payload was unavailable. Current hosted status is independently recorded in `evidence/ci_observations.json`. [R3]

**Recorded and source-confirmed:** heavy goal-cover and native-retention preparation now run as staged work in existing owners. Pending reads do not finish setup; complete components publish at their own established boundaries; complete evidence survives later refusal. Child quotient and probability work remains bound to frozen candidate generations. Frame admission and live construction overlap are accounted. [R3, R10, R11, R15]

**Recorded:** the first cancellation treatment failed at roughly 3.3 seconds. Its investigation isolated first-use WASM compilation of the large telemetry formatter, rather than a slow destructor alone. The retained narrow WASM-only `clang::optnone` treatment is part of the new baseline, not an unimplemented suggestion. Keep its failed r1 and passing r2 histories. No new release ABI or detached cleanup was required. [R3]

The completed retained-pool boundary remains in place. This programme does not select output/pending alias removal, a new portfolio, general utility migration or a rewrite of `SolveWork::Impl`. [R2, R17]

## 2. Current qualification, with clock and scope preserved

The following are **recorded** final controls, not fresh benchmark measurements from this review. WASM controls used the actual Calculator component, client, Node worker and release module. They were not rendered-browser review. [R3, R4]

| Gate or value | Current result | Interpretation |
|---|---:|---|
| Begin ≤250 ms | 2.17–2.37 ms | Passed selected controls; former ~12.4–12.6 s block removed |
| Setup call ≤250 ms | Maximum 28.7462 ms | Passed; total setup computation is not eliminated |
| Setup cancel → full release ≤1 s | 89.0246 ms | Passed including post-release probe/handle overhead |
| Retention cancel → full release ≤1 s | 34.8348 ms | Passed selected cancellation boundary |
| Ordinary call ≤250 ms | 362.2780 / 373.0484 ms | Still failed; quantum 8, cursor 293→295, ladder scheduling→compilation |
| Finish intent → usable ≤10 s | 411.9543 / 413.3166 ms | Passed; baseline pair ~3.69 /3.75 s |
| Worker request → usable ≤65 s | 43.0603 /43.6478 s | Passed; baseline ~45.0869 /45.7997 s |
| C5 original-cost policy | 85558.70618560436 | Same independently evaluated native graph |
| C5 primitive count / lower | 8407.202314771383 /405.3694021063399 | Preserved; not new exact closure |
| CB12 Regalia | C65.60036144971359; N127.11597414558129 | Exact control retained |

C5's `bounded_feasible`, requested Finish and the corpus's orthogonal `refused_unsupported_action` status still coexist. Do not flatten this into either “the solver refused to return a policy” or “the full action scope was solved exactly.” [R3, R4]

The same native C5 graph hash is `7df8b8be00a5c4513d6d5fb761605b5c399957c714079d8c39a103add036dc02`. Recorded native owned peak increased from 253288237 to 254140287 bytes, while final live changed from 153544866 to 153544716. This is a measured dynamic change, not the earlier retained-pool +16-byte structural delta. Public progress remains 200 bytes. [R3, R4]

### A useful qualification nuance: delivery improved, first verification did not

The two order-reversed pairs record first-verified worker observations:

| Pair order | Baseline | Candidate | Candidate minus baseline |
|---|---:|---:|---:|
| baseline→candidate | 41391.0755 ms | 42641.8461 ms | +1250.7706 ms |
| candidate→baseline | 42043.2251 ms | 43227.9535 ms | +1184.7284 ms |

Both candidate observations are later by approximately 1.2 seconds, even though usable delivery is earlier. This is an observation from two pairs, not a statistical speedup/regression theorem or an attribution to coroutine overhead. Sparse observation timing, runtime warmup and scheduling must remain distinguishable. The next programme must report first verification and usable delivery separately. Do not subtract worker timestamps from UI timestamps to invent a post-verification interval. [R4; arithmetic in the offline checks]

## 3. The Conquest-four evaluator question is answered

S1 evaluated the *same* cheaper complete C4 graph in native and WASM with matching frozen data, goal, prices and 1-GiB evaluator limits. Native/WASM costs were 3746.131940948572 /3746.1319409485764 and primitive counts 8608.88179365679 /8608.881793656798. Success was one, missing mass zero and both converged within the existing tolerance. [R3]

That qualifies this graph under that evaluation profile. It does not qualify default-browser discovery, or grant smaller default evaluator limits the same capability. The historical ~5218.04 browser discovery result is now a **discovery/service/profile** question, not a reason to repeat the same-graph compatibility experiment. No browser-wide correctness conclusion follows from a single graph.

## 4. Fresh source findings that shape the successor

### F1 — The observed phase label is wider than graph compilation

`certify_initial_candidate` first materializes the incumbent, copies the selected policy into a `SolveResult`, allocates goal/expanded vectors and scans states. Only afterward does it record `compile_start`. It constructs `CompiledPolicyAssertionWork`, sets the outer phase using the pre-step child state, then advances a 32-unit child batch before the first suspension. [R5]

Within the assertion owner, `compile_and_prepare` emits the graph, parses the complete JSON, constructs the economy and constructs `StrategyEvalWork` in one synchronous unit. Its `step(32)` can then continue into evaluation. The recorded outer quantum of eight is not a promise of eight cheap operations or eight early-candidate resumes: the parent returns after one early-candidate resume. [R6, R7]

**Implication:** the 362–373 ms span is not yet localized. It could include pre-compiler work, first-entry runtime compilation, compiler work, parsing, constructor work and evaluator batching. A blanket “make the compiler async” plan would be premature.

### F2 — The assertion has a second compiler path hidden inside evaluation completion

After a successful certification evaluation, `advance_evaluation` releases evaluation scratch and synchronously emits the ProductSafeRestart graph, then verifies that only compiler-designated default targets differ. The direct-recovery path separately recompiles and reevaluates a real product recovery graph when permitted. The successful paired re-emission is not covered by the `compile_and_prepare` timer. [R6]

**Implication:** splitting only the first compilation call may move the longest blocking call to evaluation completion. The selected boundary includes both emissions, pairing and evaluator admission, but does not replace the evaluator or its native authority.

### F3 — Per-edge complete compiler memory auditing is a plausible repeated-work cost

The emitter's edge lambda calls `audited_compiler_owned_bytes(&json)` for every edge. That audit walks fixed and evolving compiler structures: condition trees, route nodes/edges, maps and option-kernel payloads. `ConditionExpr::json()` is already a passive access to stored immutable JSON; `owned_bytes()` recursively walks children. This is not a missing JSON cache. [R8, R9]

The source therefore contains a possible repeated full-traversal cost. Its wall-time share is **not measured here**. If dominant, freeze only demonstrably immutable charge components at a closed construction phase and maintain exact conservative deltas for growing storage. Keep the existing full audit as an oracle. Preserve current conservative shared-node accounting; do not silently convert it to a less restrictive physical-deduplication policy or merely audit less often. [R8, R9]

### F4 — Current compile-phase summaries cannot by themselves refute the first-call issue

The compiler has an existing retained-artifact fast path. The native qualification's separate post-solve compile is only roughly four milliseconds, but that does not time the same initial compilation path. A cached final artifact, an initial source-policy compilation and paired product emission are different workloads. [R4, R8]

### F5 — Structural debt now has measured product consequences, but not a universal fix

The older supplied structural report was brace-script based, unprofiled, and included uncommitted edits. Its counts are not fresh main measurements. Its warning about giant functions nevertheless deserves attention: the newly recorded cold telemetry formatter failure and the existing finalization-TU compiler workaround demonstrate concrete costs of generated code shape. [R3, R12; historical attachment caveats]

The selected intervention should reduce the actual exposed dependency/lifetime/code-size boundary. Moving a giant function to another file without changing generated code or state ownership is not success. The historical 260-method/18-giant-function figures are not used as current acceptance metrics.

## 5. Decision and limits

Select **integrity repair first**, followed conditionally by a bounded **first-policy service** programme. The current CI failures concern the same setup/consumer/compilation/publication boundaries; they are not an unrelated omnibus gate. Once the affected authority and capability controls pass, attribute before choosing among code-shape repair, resumable compiler/assertion stages, or compiler-ledger amortization. Combine treatments only when one measured cause is repaired and another still prevents the declared gate. Preserve the new setup and cancellation work.

This programme does **not** reduce the SSP's state space, produce a tighter mathematical lower, or guarantee a cheaper crafting strategy. Its value is making first-policy work interruptible and maintainable while protecting what now works. Give it a stop boundary; the next substantive solver question is generic C4 discovery/service recovery. Keep the independent optimistic-winner lower-model investigation visible rather than letting maintenance consume the roadmap indefinitely. [07_RESEARCH_QUEUE_AND_DOCS.md](07_RESEARCH_QUEUE_AND_DOCS.md)
