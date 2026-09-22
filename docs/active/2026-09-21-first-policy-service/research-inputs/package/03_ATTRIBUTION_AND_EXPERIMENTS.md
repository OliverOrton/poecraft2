# Causal attribution and the smallest useful experiment

## 1. Identify the right interval

The recorded input owner is `ladder_scheduling`, output owner `compilation`, outer quantum eight, lifecycle sequence 293→295. Those are coarse observations of one external call. The current early-publication coroutine advances once per parent call; its child assertion can do compilation/preparation plus evaluation within a 32-item batch. A numeric quantum is a count ceiling, not a wall-time guarantee. [R5–R7]

Build one trace of the actual path with these intervals, using existing bounded native progress events and worker metrics. Names below describe required distinctions, not pre-existing field names or a request to add all of them to the C struct.

| Segment | What belongs inside it | Important exclusion |
|---|---|---|
| Parent early-candidate dispatch | time around task resume and its actual caller | not equivalent to `compile_policy_strategy_json` |
| Materialization/snapshot | policy population, proof arrays, goal scan, input ownership | not independent verification |
| Certification emission | source policy → ordinary fail-closed graph | not JSON parsing or evaluator construction |
| Parse/admit | native strategy parser, economy, entry bindings, evaluator construction | not first evaluated row |
| Evaluation service | each retained evaluator resume and count/work delta | not post-evaluation product compilation |
| Product emission/pairing | successful paired graph and default-only structural check | not a verified changed recovery policy without its required evaluation |
| Release/transport | actual child destruction, export, host transfer and handle release | not an early acknowledgement |

Use one steady native clock for nested native intervals and one worker `performance.now()` clock for external calls. UI intent/ready subtractions use the UI clock. Record any cross-clock alignment explicitly; do not subtract unrelated clocks. Inclusive parents are not added to their children. Report an unassigned remainder instead of forcing percentages to sum by inventing attribution.

## 2. First-use compilation is a real hypothesis, not a generic explanation

The previous programme isolated this cause for the telemetry formatter only. It does not prove the same cause for the first-policy call. V8's primary architecture document describes baseline/optimizing tiers and blocking first-use compilation in lazy mode; runtime version/configuration matters. [R3, P1]

An in-body timestamp starts after code needed for entry is available. A long caller interval with a short in-body interval can reveal a pre-entry component, but also includes wrapper/bookkeeping and requires further tracing. Instrumentation can change optimization or inlining. Record build identity and code-body size; do not treat a specially named debug build as the ordinary release baseline.

Inspect supported tooling already installed with the current SDK/runtime. Use the SDK's disassembler/symbol tooling and native/browser profiler rather than introducing a new Wasm parser. Confirm available flags via that actual tool's help. Record Node `process.versions` and actual browser/V8 when used; do not assume current upstream V8 equals the installed runtime. Chromium's performance profiler can alter tiering, so a normal cold external-time trace must remain the acceptance anchor. [P1–P3]

No user/global browser flag change is selected. Tier-control flags may be used only for a labelled diagnostic counterfactual after confirming installed support. They cannot be the product fix.

## 3. Experiment ladder, not a large factorial campaign

**First:** inspect complete existing r2 traces around the identified sequence if their raw local bytes are present. The compact receipt is sufficient for the known gate but cannot supply unseen stage details. Hash before use; missing raw reports remain missing. Use the existing programme's summarizer/runner rather than another evidence schema or supervisor.

**Second:** reproduce the affected call once with bounded added attribution on the same resolved request. Retain full wall time, maximum call, stage work and source/runtime identity. End at the first useful verified artifact using the existing actual Calculator Finish probe; there is no need to wait four minutes just to observe a forty-second first-policy path.

**Third:** select one causal counterfactual. Examples: repeat the same first-policy compilation from a preserved immutable input in a warmed runtime with a fresh solver; compare normal and narrowly separated generated function boundaries; or switch the full-ledger audit to an independently verified exact delta path. Do not combine all treatments. A saved already-compiled final graph exercises a different path and cannot stand in for cold source-policy compilation.

**Fourth:** qualify the chosen repair using normal release settings and cold workers. For latency claims, reverse baseline/candidate order; record CPU/host admission, runtime warmup and whether processes or modules are reused. Run timed controls serially. A failed equivalence gate stops larger controls before launching them.

Cold should mean a fresh worker/process with declared module/data cache state; a new `SolveWork` inside an already warm module is not the same treatment. Loading the same bytes again may reuse runtime compilation artifacts. Report these distinctions instead of claiming that every “new solve” is cold.

## 4. Include the accounting path in attribution

The complete compiler memory audit is called by every emitted edge. Measure call count, total sampled/aggregate time and the sizes it walks. The condition nodes are immutable and already store JSON, but recursive ownership accounting and growing literal maps still cost work. [R8, R9]

A potential treatment splits the existing charge into a closed, unchanged contribution and mutation-accounted dynamic contributions. It does **not** reduce accounting frequency without preserving the cap at every original admission/check point. Compare with the original full audit at adversarial allocation/capacity changes. Counters must distinguish requests, full traversals, incremental updates and disagreement, rather than labelling all requests “avoided work.”

This is a conditional optimization branch. Do not implement a general memory-ledger framework or infer from the source that it explains the entire observed 373 ms.

## 5. Progress that actually answers the user's question

The selected trace should answer: what began; what finished; why this owner received service; whether a complete candidate exists; which generation is being compiled/checked; where Finish or Cancel is waiting; how much work and live storage are retained; and which large uninterruptible interval is still uncovered.

Use the existing bounded event buffer/sequence and phase-owner projections. Preserve omissions and distinguish current availability from a historical best scalar. A per-row text log, full graph hashing at every progress read or millions of cross-thread progress messages would distort the experiment. Internal compiler substage observations can be versioned optional trace fields; do not expand the 200-byte public progress ABI just to name a substage.

Repeated progress and trace reads must not emit new semantic work, build conditions, change candidate order, call preparation or authorize publication. Time spent reading is still real overhead even when the read is logically passive.
