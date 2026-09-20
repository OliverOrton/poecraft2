# 5. Research synthesis and mathematical contracts

**Status:** analysis supporting the selected engineering programme. No new solver algorithm, runtime certificate, formal proof assistant development or mathematical novelty claim is selected.

## 5.1 What the current research says to do, and not do

The completed source/evidence programme improves delivery and preserves complete owned witnesses. Its negative Ring probes do not justify building an observer adapter or response cache now. The measured startup has two synchronous owners, with a ready-state/reentry problem, so an eventual cooperative repair must cover the real dependency boundary. These are repository findings, not conclusions drawn from generic software literature. [R05–R08](SOURCES.md#r05).

The external research used here is deliberately narrow. Pytest's own documentation supports conventional TestCase interoperability but leaves `load_tests` unsupported. Git documents path-controlled byte conversion. CTest offers explicit setup prerequisites. The C++ Core Guidelines recommend private access for state participating in an invariant. These inform the implementation shape; none predicts a percentage speedup or a maximum codebase size an LLM can handle. [P01–P04](SOURCES.md#p01).

Parnas's decomposition paper is background for selecting a boundary by the knowledge it hides, rather than by file size. Only its publisher abstract/record was reviewed here; no detailed claim depends on inaccessible full text. [P05](SOURCES.md#p05).

The engineering hypothesis is that reducing the set of places able to mutate an accepted artifact reduces the reasoning required for future changes. Measure the actual writer surface and focused fixture boundary before and after. Do not turn that hypothesis into an unsupported LLM-success or performance statistic.

## 5.2 The owned-witness invariant

Let θ describe the semantic target: exact root, native goal/laws, permitted actions/programmes, observations/control memory and original prices. Keep invocation identity and storage/build identity distinct from θ, although they are necessary provenance for using a specific artifact.

At computational step t, let E_t be the set of **currently owned and compatible** complete proper controllers whose evidence satisfies the relevant issuer/checker contract. For each π in E_t, the mathematical expected original cost J_θ(π) is feasible, so

\[
V^*_\theta(s_0) \le J_\theta(\pi).
\]

When E_t is nonempty, selecting any valid owned controller preserves feasibility; selecting a minimum among the eligible candidates preserves the intended selection rule. This says nothing about an evicted historical scalar, an incomplete proposal, or an old certificate from a different θ. The current canonical interruption argument already makes this distinction. [R17](SOURCES.md#r17).

The actual implementation stores numerical evaluations under its existing reconciliation rules. **This inequality is not a claim that every accepted floating-point scalar is a rigorously upward-rounded enclosure.** The refactor preserves the current evaluation contract; stronger native endpoint certification is separate work.

Represent the supporting object conceptually as

\[
A=(G,\ C_{root},\ \theta,\ \text{provenance},\ \text{evaluation}).
\]

The graph G and its full root certificate C_root must refer to the same policy and domain. Pairing a historical cost 7 with a cost 10 graph does not support the value 7. A root-only certificate cannot be transported to arbitrary parent states. The current private-artifact validity checks are concrete guards for these distinctions. [R13](SOURCES.md#r13), [R16](SOURCES.md#r16).

### Retention need not imply publication authority

A staged or compiled candidate can be retained without membership in E_t. This is useful work, not an error. Admission to retained storage and admission to the set of verified deliverables are different transitions. Their validators must remain distinct. That distinction is why a superficially cleaner “all pool entries are certified” rewrite would be incorrect for the current pipeline.

Monotonic historical best cost is not sufficient to prove current availability. If an artifact becomes incompatible or is no longer owned, the corresponding scalar may remain in historical telemetry but is not a deliverable. Any claim of nonincreasing current deliverable cost requires the additional premise that a compatible supporting witness is preserved.

## 5.3 Atomic transfer and failure safety

Model replacement as preparation followed by one commit:

\[
(K,S,D) \longrightarrow (K,S',D') \longrightarrow (K',\varnothing,D').
\]

K is committed owned evidence, S staged scratch, and D cumulative logical debit. During preparation, the old eligible witness remains valid. The commit is permitted only after the new object satisfies its role-specific invariant and memory admission covers actual old/new overlap. Failure discards or releases S through its owner, preserving K where still compatible. D does not decrease on release.

This is a local invariant argument, not a requirement to allocate a general transaction framework. It applies directly to candidate-copy admission, completed assertion attachment and move-out to publication. Test failures at those existing commit points, not every line of the solver.

The publication owner's existing sealing point remains authoritative. Complete eligible evidence already awaiting handoff must be considered under that contract; unfinished optional work cannot hold an available fallback hostage. After sealing, transport must not mutate the tuple. Invocation-scoped cancellation precedence is a control contract and cannot be inferred from the policy equations. [R16](SOURCES.md#r16), [R17](SOURCES.md#r17).

## 5.4 Behavioural preservation is more than matching one successful hash

A useful relation between old and refactored implementations preserves their committed evidence, selected roles, candidate order, remaining obligations, consumed logical work and externally significant terminal behaviour. The new implementation may take extra internal steps that do not change those observations, but must not expose an intermediate invalid state.

A deterministic focused fixture can compare sequences of retention/admission/prune/take decisions. A successful output hash alone does not test failed replacement, stale pointers, invalidated scope, partial private transfer or cancellation. This motivates the negative cases in the ownership design.

Even such a semantic relation does **not** establish equal wall-clock performance. Extra progress validation can perturb a deadline-bounded run without changing its mathematical decisions under unlimited work. This is why read purity and observer overhead, as well as fixed-work and timed comparisons, are kept separate.

No broad eventual-progress or exact-closure claim is discharged by this refactor. The canonical mathematics already separates validity, eventual service and efficiency. [R17](SOURCES.md#r17).

## 5.5 Memory accounting is a semantic resource contract

For the documented accounting population, use an ownership decomposition such as

\[
M_t = M_{committed} + M_{staged} + M_{frames} + M_{other} - M_{documented\ overlap}.
\]

This is bookkeeping notation, not a new estimator. Every subtraction needs a real documented overlap or intentionally excluded structural shell. Capacity, not just current logical size, matters wherever the existing owner accounts reserved storage.

The current four-reference compensation is exactly such a structural adjustment. If a shell disappears from the object layout, leaving its old subtraction can undercount. If only one disappears, removing compensation for the other three changes the resource population in the opposite direction. Reconcile actual layout and both ledger paths, not a guessed pointer count. [R15](SOURCES.md#r15).

Cached memory or validity is safe only while all transitively relevant storage/alias/capacity or semantic dependencies remain unchanged. `const` access to an outer object does not prove that condition. Releasing memory changes live storage, not the work already performed. [R17](SOURCES.md#r17).

## 5.6 First-exit and response mathematics remains useful but unselected

For a proper finite controller with nonterminal transition matrix Q and immediate reward c,

\[
V=c+QV.
\]

Partition boundary B and interior I. Eliminating I gives

\[
\bar Q=Q_{BB}+Q_{BI}(I-Q_{II})^{-1}Q_{IB},\qquad
\bar c=c_B+Q_{BI}(I-Q_{II})^{-1}c_I.
\]

These are the existing conditional response ideas, not a proposal to form a dense inverse in the engine. A useful cache additionally requires native item/control correspondence, correct context/routing dependencies, proper interiors and boundaries, manageable sparse fill, and net savings after matching, construction, invalidation, overlap and validation. The current B result establishes none of those missing empirical premises. [R07](SOURCES.md#r07).

Likewise, a smaller per-node observer set is not automatically a smaller native global layout. Required whole count-membership vectors, legality, surviving blockers/locks and routing observations can restore distinctions. Reopen only with an actual private-constructor correspondence and a useful measured reduction. Do not replace the existing observation fixed-point owner.

## 5.7 Action limits remain a separate product/mathematical decision

Oliver's earlier request to find a best strategy within an action limit remains relevant, but M0–M3 does not choose the meaning of that limit. Expected count, a hard pathwise cap and a required probability of finishing before a cap define different problems.

A proposal penalty C+λN, with N the expected primitive count, does not enforce a hard cap. Nor does properness establish completion by a chosen finite budget. A simulator's action-cap censoring is not an uncensored policy cost estimate. The original-cost objective, primitive counting and private count-guidance treatment remain separate. [R08](SOURCES.md#r08), [R17](SOURCES.md#r17).

A future action-limit programme must specify the terminal outcome when the budget runs out, treatment of failure/salvage/restart, whether the budget resets, whether randomization/history dependence is allowed, and whether an expectation or a tail probability is constrained. Without this, “best within a limit” is under-specified. No default interpretation or public optimizer is silently added here.

## 5.8 Lower-proof research is preserved, not advanced by cleanup

The project still aims to extend certified exact closure. Better delivery or safer retention does not strengthen an admissible lower, retire more alternatives or establish full-scope optimality. [R35](SOURCES.md#r35).

The earlier persistent-witness/optimistic-model question remains a possible research direction, but this review did not requalify its native lower producer or measure a new ceiling. Do not resume lower work simply because local counters are available. A future treatment should identify a specific source of optimism and a native-valid change capable of affecting the root or competitive obligations.

## Documentation disposition

Preserve the current arguments about owned interruption and staged preparation in `mathematics/search-and-resumption.md`; update actual implementation correspondence when the selected owner changes. Keep response/recurrence arguments in their existing policy chapter, observer conditions in their existing abstraction owner, and numerical enclosure obligations in numerical closure. No mathematical status is promoted solely by moving code or passing lifecycle fixtures. [R17](SOURCES.md#r17), [R34](SOURCES.md#r34).
