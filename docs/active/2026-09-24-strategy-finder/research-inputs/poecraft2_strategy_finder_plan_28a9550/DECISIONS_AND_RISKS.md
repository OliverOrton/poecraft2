# Decisions answering the supplied handoff

| Question | Selected answer |
|---|---|
| How much backend is reusable? | Mechanics, macro semantics, exact selected laws, compiler/evaluator operations, numerical contract, artifact checks and tooling. No unsupported reuse percentage; interfaces need narrow extraction. |
| Clean interface? | Immutable original problem; separate finder treatment; untrusted structural candidate; request-bound compilation/checking; immutable checked artifact. |
| First search? | Controller-sketch bounded best-first/beam with native macros and coordinated decisions; not primitive rollout-only MCTS. This is the chosen experiment, not a proven best algorithm. |
| Data scale? | Unknown until stratified setup/law/macro/check costs are measured. Native feasibility records and small existing candidate data already exist. |
| First representation? | Exact source/control identity plus lossy role set, context and candidate features, with availability masks and separate backend keys. |
| Bounds and pruning? | Use cheap valid scoped bounds when useful; heuristic pruning allowed in finder; neither heuristic omission nor private lowers enter proof authority. |
| Initially exclude? | Full optimality closure in finder, learned runtime executor, huge dataset, multiple new search engines, CUDA rewrite, concurrent mutable contexts. |
| Handcrafted before training? | Yes: establish from-root candidates, checking and diverse labels. Keep a scoring seam for subsequent simple/model comparisons. |
| GPU entry? | Offline training or measured large-batch inference later, only if end-to-end benefit appears. |
| Parallelism? | Serial first; independently owned bounded proposal/check workers after memory/thread-safety/cancellation audit. |
| Calculator modes? | Current solver default and real experimental finder through shared input/transport; portfolio/hybrid later. |
| Demonstration? | Best checked original-root cost vs total time/work/resources, no-policy rate and unseen-request coverage. |
| Later proof connection? | Explicit compatible checked-artifact import only; no learned values, role IDs or unsound lower/pruning data. |

## Major risks and how to distinguish them

**Grammar too weak:** the new search emits only renamed seed policies or isolated tiny patches. Measure semantic candidate diversity and ability to construct coordinated setup/recovery. Fix one native-supported structural limitation; do not confuse more candidates with more strategies.

**Verification dominates:** compact programs still induce huge native products. Measure actual selected-policy construction/checking, support and concurrent memory. Do not promise that removing proof makes evaluation cheap; do not preemptively build a response cache without useful repeated-candidate overlap.

**Aliasing harms proposals:** role features omit a blocker or control flag. Preserve exact source labels, expose unknown/availability, and use checker feedback. Do not repair this by merging true transitions or ignoring rare failure.

**Target spoofing or scope drift:** a new proposer manufactures an easier graph. Bind the original native target/scope/root independently, and run the F0 adversarial tests before any new policy can be published.

**No bootstrap:** the finder secretly depends on an old solve/donor. Require two real from-root tests with no full legacy work; label seeded arms explicitly. Unsupported is a truthful early state, not an excuse for uncharged hidden fallback.

**Misleading ML:** many entries of one graph look like a large dataset; capped runs become wrong negative costs; post-expansion features leak expensive work. Group split by request/lineage, type labels and feature availability, and count all production feature cost.

**Codebase growth without separation:** new beam/model state gets added to legacy Impl, or a generic framework has no consumer. Keep a small peer owner and candidate view; extract only the used backend operations. Follow one living record and dependency-ordered checkpoints.

**Unfair improvement claims:** a seed, extra checker, larger action envelope, changed price/count reward, training or concurrent work goes uncharged. Preserve separate problem/treatment/artifact identities and end-to-end budget accounting. A functional experimental lane is not automatically a superior default.

## Open implementation choices, not missing user decisions

The implementing agent chooses the smallest actual seed-helper extraction, proposal compile view, native option-descriptor seam and initial outer control-node bound after F0 source tests. It may simplify names/containers. It must not change requested objective, authority, scope, root or proof isolation. No broad algorithm search, full ML dependency or public default switch is automatically authorized.
