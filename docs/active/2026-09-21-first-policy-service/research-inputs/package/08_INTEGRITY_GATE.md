# T0 integrity gate: restore consumer contracts before another performance change

## Why this gate now takes priority

The completed Windows push run **35618066993**, job **106393816191**, is newer evidence than the cooperative-setup closeout's correctly historical “hosted CI unrun.” It builds, reproduces the frozen runtime and passes the Python families, but fails **three of 17 CTest suites**. The knowledge workflow **35618067061** passes. This is not the previous timestamp failure. [R23]

The selected failures include **17 solve assertions**, **four API assertions**, and an uncaught S8.3 `std::logic_error` at the bounded-incumbent interval check. Counts of failed assertions are not counts of independent defects. The log does not identify the S8.3 fixture, actual interval values, either Fracture graph, or previous-revision behavior. No solver artifacts were uploaded; the existing upload matches only runtime-identity failures. See [the exact observation record](evidence/ci_observations.json) and [selected transcribed excerpt](evidence/ci_failure_excerpt.txt).

**Selected action:** localize and repair the affected setup-consumer / artifact-evaluation / progress contracts through the existing owners, preserving cooperative setup and complete cancellation. Do this before T1–T3 compiler performance work. If the investigation requires a new mechanics decision, a different lower model, or a broad authority redesign, stop after the focused witness and safe repairs; the conditional compiler plan stays queued. A closed integrity gate is not an instruction to skip this work because C5/Regalia happened to pass.

## 1. Separate the failure classes

| Actual failure group | Source-confirmed interpretation | What is not yet established |
|---|---|---|
| Constructive certificate, ten assertions | The tiny fixture checks a bench-only goal at cost 3, elimination of two competing actions, two discovered states, retained witness and cache non-reuse. The final value/action checks are not among logged failures. [R24] | Whether deferred readiness lost one-shot service, whether another contract changed, or whether this predates the new main |
| Shared Fracture vs exact-lift cost/consumption, five assertions | The shared graph is also compared to the published solve's evaluated cost; that assertion is not logged as failing. Failure is against a separate `lift_policy_exact` result, plus four consumption values. [R25] | Same-controller equivalence, a changed reoptimized policy, a numerical mismatch or a test comparison-scope error |
| One-step expansion expectations, two assertions | The fixture manually stages an internal Iterating state and calls `step(1)` without establishing completed setup first. [R26] | Whether the test precondition is stale, or the owner fails to resume the intended search after setup |
| Public phase-owner expectations, four assertions | Assertions assume an owner range and `Refining => PolicyAssembly`; exposed setup can require a richer phase/owner relation. [R27] | Which samples actually violated the relation and whether the implementation or test is wrong |
| S8.3 interval exception | The guard checks a restored incumbent's lower, evaluated cost and upper before later publication classification/normalization. It fails closed by throwing. [R28] | Which inequality failed; whether values have compatible authority/entry/scope; whether the guard sees a transient tuple; whether a native model or numerical error exists |

Do not erase failure history or declare all five rows one asynchronous-setup bug. These are triage classes. Use the smallest native witness to distinguish them.

## 2. Reproduce without another long solver campaign

Preserve the current source-matched executable and the recorded `0f376f4` baseline if their actual bytes are available. Run the existing failing CTest selectors once on the current configured build to establish the local symptom:

```powershell
ctest --test-dir build/engine -C Release --output-on-failure -R '^(poecraft_solver_solve|poecraft_solver_api|poecraft_solver_s8_3)$'
```

These are **observed CTest names**, not invented executable flags. The suites contain historical embedded simulations; this one baseline reproduction is justified by actual failures, not a new policy campaign. Immediately isolate the failing function/subcase through the existing native test driver, a small new selector in that driver if needed, or a debugger exception breakpoint. Do not keep rerunning the complete suites during every edit. Do not change their expectations merely to get the first run green.

If the production artifact is unnecessary for an isolated synthetic case, use that case without loading the whole data pipeline. Keep the actual tested data path for API tests. Resolve toolchain/build/input identity before comparing old and new. A historical local GCC build versus hosted GCC 16.2 is not a clean single-treatment experiment. A compiler warning is not a diagnosed cause. If only the hosted toolchain reproduces a failure, preserve it as an unresolved affected lane and record a minimal reproduction rather than relabelling it passed.

For S8.3, name and flush the active subcase before entry or break on the throw; do not infer the failing case from elapsed time. Retain the original fail-closed guard while instrumenting. Capture a bounded structured failure record at the native owner, not a huge full-run log or a new evidence framework.

**Required diagnostic tuple:** source and runtime identities; exact source item/entry; goal, action and economy scope; producer stage and readiness generations; selected graph/candidate identity; `L`, `J_pi`, `U`, tolerance and the failing side; each number's authority; independently-evaluated flag; actual selected-controller and cap/Finish state. Record float bit patterns and normal decimal values where comparing numerical behavior. This separates scope transport, unfinished construction, actual inequality errors and numerical-scale effects.

Keep changed test fixtures narrowly registered in the existing harness. Preserve meaningful old scenario coverage while removing accidental dependence on exactly one `step()` call, only after the new state-machine contract is explicit.

## 3. A concrete research hypothesis: pending evidence is not negative evidence

Cooperative conversion introduces a third semantic outcome where a synchronous caller formerly received a finished result: **pending**. Returning zero can be safe for a nonnegative-cost lower query, but it is not automatically equivalent for a one-shot certificate trigger, candidate ranking, source-policy selection, or a fixture that directly drives an internal stage.

Let an optional certificate depend on a table generation `g`. With the old blocking producer, the consumer sees completed `h_g` and decides once. With a cooperative producer, it can first observe `pending(g)` and a safe numerical fallback. If it records that encounter as “attempted and failed” permanently, later commit of `h_g` cannot reach the same decision. Mathematical lower safety is preserved while useful service is lost. Extra states may then be constructed even when the original certificate could have avoided them.

This is a **falsifiable explanation of the constructive fixture**, not a proven diagnosis of current main or the entire state-explosion problem. Instrument pending/ready/refused and whether the consumer is revisited. Distinguish “proved inapplicable” from “not ready.” Preserve one deterministic deferred obligation or an explicit service fence in the current owner if that is the missing dependency. Do not invent a global event bus or make every lookup block again.

A suitable implementation contract for each affected consumer is:

- `pending`: no authoritative decision from incomplete data; continuation/debt retained when the original decision is still required;
- `ready(g)`: read one complete compatible generation and make the original decision once;
- `refused`: use the existing conservative fallback and report the reason, without recursively reconstructing the same refused producer;
- `cancelled`: stop and release, not ordinary optional-proof refusal.

These are proposed semantics, not mandated new public enum names. Reuse the current setup/service state. Cap refusal, terminal state, cancelled work and a legitimately superseded candidate can discharge a pending obligation without a retry. Fair eventual service applies only while the request continues, its inputs remain compatible and its budget admits that service.

## 4. Restore the interval contract without laundering the evidence

The source guard assigns `L = certified_global_lower_bound()`, `U = incumbent.certified_upper_bound`, and `J_pi` from the independently evaluated incumbent when available (otherwise from the provisional upper), then checks `L <= J_pi <= U` with the existing scale-dependent tolerance. Later code can normalize publication scope. [R28]

The *first question is which side fails*. Appropriate repairs differ:

**Incompatible lower authority:** a restricted/coarse or different-entry value must not be labelled a global lower for this native controller. Find and correct the transfer/classification that lost its scope. Preserve other genuinely independent accepted lowers. Do not simply replace every lower with zero or clamp `L` to `J_pi`; `J_pi` is a policy upper and cannot make an invalid lower certificate valid.

**Stale or mismatched upper bundle:** repair graph/cost/certificate/entry generation pairing and retain the prior valid artifact. Do not widen `U` after the fact merely because an unrelated evaluation returned a higher number.

**Premature consistency check:** only if the captured tuple proves that the check is seeing explicitly provisional values, move it to the first well-defined complete authority boundary while keeping a final fail-closed check before publication. A narrower early check can remain. Removing the only bracket check or asserting the final result after exporting it is not acceptable.

**Native-model or numerical mismatch:** retain the original counterexample and investigate the actual equations/coefficients and tolerances under their existing contracts. A rounding explanation requires measured residual/scale evidence, not an epsilon increase. If a new proof obligation is required, stop the compiler-performance branch and return it explicitly.

Acceptance requires the identified S8.3 case to pass with a supported tuple, plus negative cases that still refuse mismatched authority and actual violated intervals. A caught exception converted to a success status is not a repair.

## 5. Fracture comparison and phase tests have different acceptance rules

For Fracture, save both complete graph/evaluation pairs and resolved original prices. Compare operations, entries, default routes and selected policy, not only a scalar. Establish whether the independent lift reoptimized a different controller. If both are valid but different, a test may compare policy quality/semantics only according to its actual intended contract; keep a *same-controller* compiler-sharing control to replace the accidentally invalid equality. If the controller truly is the same, the cost and resource mismatch is a substantive bug and must be repaired. Do not drop per-resource checks or normalize away operations.

For progress tests, write an explicit allowed `(phase, owner, readiness)` relation. Setup is a legitimate visible owner when pending; Done still means already checked, usable result. Drive to the intended stage using bounded owner-side predicates, then assert that incomplete action coverage cannot be finalized and cancellation remains possible. A wildcard “any enum value is fine” or unbounded loop is not a replacement for the original assertion. Query reads remain passive.

For constructive certificates, retain both the certificate-enabled and disabled controls. Final cost equality alone is insufficient because the original fixture intentionally checked avoiding unnecessary state/row construction and invalidating price-bound replay. Restore that capability or report the concrete intentional contract change and its impact; do not silently rewrite expected counters to zero.

## 6. Tooling improvement with a narrow owner

The runtime-identity upload now works for its own intended failure family, but it does not retain a solver failure record. Extend the **existing Windows workflow** to retain relevant CTest logs and small structured failure tuples on failure, with bounded paths and no unreviewed bulk game data or local credentials. No new CI workflow, supervisor, schema empire or permanent dashboard is selected.

Do not repeat timed controls to repair report extraction. Keep failed logs and source/binary identity together. Preserve the already-passing knowledge lane. No automatic hosted dispatch, rerun or push is authorized; a local repair is not a hosted green claim until a corresponding run is actually observed.

## T0 exit and performance continuation

T0 closes when all five failure groups have an evidence-supported disposition and affected native controls qualify. Actual correctness/capability failures must be repaired; justified fixture changes must retain their semantic assertions and have a focused native regression. Re-run the three originally failed families once after the retained repairs, with the known scope/cost. Preserve setup/Finish/cancellation tests and positive C5/Regalia results appropriate to changed owners.

Only then start the T1 causal first-policy attribution and choose one bounded T2 treatment. If T0 is the substantial retained change, use it as a clean committed baseline and attribute its effects before adding performance changes. Do not combine unresolved numerical repair and optimization in the same timing comparison. A safe T0 closeout with an explicit blocker is a valid stop; falsely passing the gate is not.
