# 5. Validation and experiment plan

## 5.1 Reuse the actual infrastructure

Native case ownership remains `solver_corpus_runner` / `solver_worker`; fixed-graph checking remains the existing native evaluator and WASM worker tools. The current `probes/qualify.py` demonstrates resolved preflights, fresh host admission, serial runs, retained binary hashes and `run_isolated_process`. **It has historical fixed paths/case/output names, so do not execute it unchanged over completed evidence.** Extend the existing thin adapter for the selected matrix; do not add another supervisor, queue or universal comparator. Its process result field is `exit_code`, not `returncode`. [R19](SOURCES.md)

The existing Calculator probe accepts `case_id output_path control`, with current controls `finish` and `cancel_setup`. Source example, from `apps/web`, to be dispatched through the existing supervisor:

```text
<resolved-node> --import tsx test/calculator-delivery-probe.ts \
  cb01-cross-base-product8-long240 <new-output-path> finish
```

Replace placeholders with resolved local paths; this is the known interface, not a ready-made new production command. The source probe's early setup cancellation is not stage-complete coverage. Add explicit stage-aware cancellation through existing trace/control hooks for the new preparation stages, and label those additions as new tests.

The wrapper commands `scripts/test.ps1 -Scope Python -SkipBuild`, `-Scope Native -SkipBuild` and `-Scope Web -SkipBuild` are current interfaces. They require their selected dependencies and matching runtime data. Set `POECRAFT_PYTHON` to the resolved interpreter and run from repository root. Use the normal native/WASM build routes when those sources change; do not claim an old committed WASM qualifies new C++.

## 5.2 First prove structure and safety cheaply

| Fixture class | Required negative or comparison |
|---|---|
| Input and stage activation | High-impact with retention on; retention off; ordinary lazy cover; compatible coarse replay; invalid cheap request |
| Publication | Every pre-commit prefix unavailable; independently committed valid evidence survives another component's refusal; no early ready flag |
| Read passivity | Repeated progress, trace, lower query and abandonment snapshot do not advance cursors, allocate proof rows or complete setup |
| Resume determinism | Multiple nonzero quanta yield the same completed tables/relations and logical-work sequence; no skipped/replayed work |
| Retention generation | Candidate changes after suspension invalidate/rebuild value-dependent minima and coverage; stale relation rejected |
| Cap/error distinction | Cap before frame/allocation and mid-row; optional unsupported retention; cancellation distinct from both |
| Ownership | Parent/child frame admission; staged-copy overlap; one-time reservation transfer; detached pool candidate retained/charged |
| Release | Cancellation before first step, within each owner, between commit stages and during final cleanup; no double free, survivor, work refund or stale reentry |
| Controls | Duplicate/stale Finish and Cancel; Cancel wins before terminal commitment; no verified artifact means no fabricated Finish policy |
| Scope | Hidden offer/checkpoint, lock/blocker differences, native junk-free goal, original prices and paid recovery stay unchanged |

Existing bounded-Finish, selected-fallback, proof-pattern, phase-lower, return-bridge and API fixtures should be selected by touched ownership. Do not run the broad API suite during each edit just to repeat its embedded simulation. The previous ABI canary and passive sequence/trace checks remain relevant. New fixtures are needed for **pre-search pending setup**, not merely the already-tested suspended publication pool.

The completed target of a staged preparation should be compared to an untouched baseline under matching semantic input and logical fences. Exact lower provenance and numerical acceptance are checked separately from scalar equality. Do not infer global equivalence from one Conquest cost.

## 5.3 Small real matrix, only after fixtures work

**Identity:** one clean selected Windows reproduction under the actual workflow shell; no native rebuild to test only timestamp string transfer. Advance to required wrapper checks after the exact-byte gate passes.

**Conquest-four fixed graph:** same saved cheaper graph independently evaluated native/WASM at explicit matching evaluator limits. Record success/off-policy/cost/count, refusal reason, native member domain and graph identity. Do not change the browser's default budget to turn the result green. A separately admitted profile is labelled separately. This is compatibility testing, not rediscovery or full-scope optimality.

**Conquest-five setup and delivery:** use the current CB01 five-goal product8/240-second request and frozen economy. Retention remains the existing `reuse` treatment. Collect begin and all setup slices, committed lower, first complete verified policy, Finish, delivery and release. Use the current M3 build and the candidate as explicit treatments. For a timing claim, use serial baseline/candidate then candidate/baseline under fresh admission. Do not run timing arms during compilation.

**Full-budget behavior:** one justified native baseline/candidate default-240-second comparison checks the interaction with later search. Reuse a compatible preserved baseline executable, but use a fresh host window for timing comparisons. Keep the default finish's clock origin unchanged. If a shorter logical-completion fixture already settles a particular invariant, it does not need another long run.

**Other controls:** retain the exact Regalia control and small native cases covering the changed setup applicability/early-cap paths. Bow/Ring/Amulet high-water controllers remain preserved files. Do not automatically rerun their wide long searches; re-evaluate or rerun only if a touched mechanism makes that evidence necessary. No base-specific production code.

## 5.4 Clock and acceptance table

| Gate | Start and end on one clock | Target / current evidence |
|---|---|---|
| Begin | Worker before/after native begin call | ≤250 ms; M3 12598.9964 ms fail |
| Setup slice | Before/after each native call advancing preparation | ≤250 ms on declared controls; new separate attribution |
| Setup Cancel | UI/host invocation intent → same invocation's final native/worker handle release observation | ≤1000 ms; old 23081 ms fail, not rerun by M3 |
| Finish | UI intent → actual strategy readiness including cleanup | ≤10000 ms; M3 3571.2889 ms pass |
| Delivery | UI worker-request timestamp → UI usable strategy | ≤65000 ms; M3 45185.1174 ms pass in that probe |
| Broad ordinary step | Before/after every non-setup native step | ≤250 ms; M3 369.0194 ms fail |
| Work/memory | Native cumulative logical work and distinct owned allocations | No refund/undercharge; explain structural storage changes |

Do not subtract timestamps from unrelated clocks. Correlate invocation IDs and paired host/native milestone observations. Keep begin completion, setup completion, verified artifact available, worker response, graph export, UI-ready and handle-release distinct. A successful process exit tests only its implemented assertions; explicitly compute each timing gate from the receipt.

If S2/S3 produce no long setup units but ordinary compilation still exceeds 250 ms, the **setup milestones** can pass with the broader step gate explicitly failed. The overall result is partial responsiveness qualification, not a relaxed global target. A fresh Finish/delivery regression must be investigated; old passes cannot be assumed to survive.

The component/Node-worker/WASM probe is not a rendered-browser test. Oliver retains visual review. Browser behavior beyond that runtime must remain explicitly unqualified unless actually measured.

## 5.5 Costs, counts and evidence reuse

Retain original Chaos, expected primitive actions, success, off-policy mass, graph identity, independent checker status, lower/provenance, exactness and termination separately. A newly discovered different graph can be good but is not byte-preserving refactor evidence. Compare completed work at logical fences to diagnose algorithm changes; compare delivered policies at the same wall budget for user value.

New or altered executable graphs require independent native evaluation and, only when genuinely needed by the current operating policy, its 1,000-trial execution qualification. Unchanged graphs do not get a new Simulator run. Any censoring stays separate from uncensored cost. Do not import obsolete 10,000-run historical manifest language as a new mandatory campaign.

Keep compact receipts in the living record and bulk traces on disk. Preserve failed attempts and their input/build hashes. No automatic rerun of an already failed condition without a changed hypothesis or implementation. No sampled approximation becomes transition or lower authority.
