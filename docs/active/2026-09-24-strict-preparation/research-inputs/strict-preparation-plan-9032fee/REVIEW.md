# Review and next-direction decision

## 1. Evidence boundary

The main branch was read at `9032fee114aea3481482c57cdd30248cdc29a5bc`,
“docs(solver): record downstream role D1 gate”, whose parent is `9b1fb8e`.
The retained work is documentation, original research input and compact native
evidence. Temporary diagnostic source was removed. No solver, ABI, WASM, goal,
price, cap, lower, exactness or published-policy change was retained. [R1–R3]

This review used the GitHub connector, current source and the uploaded previous
prompt. It did not access Codex's ignored local reports, run native/WASM builds,
profile the engine, or edit the repository. A public raw-source download attempted
for local programmatic inspection failed DNS; source review continued through the
connector. The protected root `0` was not inspected. Both current-head Windows and solver-knowledge push workflows completed successfully; this is hosted metadata, not a local requalification. Older Claude/audit reports
remain historical; their approximate sizes and earlier defects are not current
measurements.

Labels: **Recorded** = committed experiment; **Source** = inspected implementation;
**Derived** = arithmetic/conditional mathematics; **Hypothesis** = untested cause
or benefit; **Selected** = what the next executor is authorized to do.

## 2. The D1 result is useful and bounded

Two temporary instrumented A4 runs used the ordinary 240-second Finish, 300-second
native watchdog, 315-second host cleanup, 1-GiB aggregate cap, existing work limits,
Allflame economics, retention reuse, and Restart/Imprint disabled. Both returned
C3746.1319409485764, 7,213 expanded states, 135,519 rows and an 811-node/2,200-edge
compiled graph after independent evaluation. A and B have different instrumented
binaries and overhead; their timing difference is not an optimization result. [R3]

| Recorded scope | Value | Meaning |
|---|---:|---|
| Automatic admission, A | 44.878 s | Inclusive expansion-preparation owner |
| Protected kernel work, A | 18.502 s | Child subset of automatic admission |
| Publication/extraction, A | 79.048 s | Inclusive finalization |
| Strict lift, A | 70.624 s | Inside publication; not additive to it |
| “Carrier discovery”, A | 65.873 s | Elapsed prefix with multiple phases |
| Direct certification, A | 7.333 s | Another publication child |
| Final sparse selection, A | 0.0201 s | Not the time of all Bellman work in the run |
| Strict cold selected-row construction, B | 0.4662 s | Only the measured selected-row calls |
| Strict pre-row identities, B | 0.5673 s | Only the measured identity work |
| Strict whole-locator loop, B | 16.004 s | Inclusive, not all strict preparation |
| “Carrier discovery”, B | 66.655 s | Includes work outside that loop |

The 50.651-second difference in run B is an **unseparated elapsed interval**,
not a reusable-work estimate. Run A/B timers must not be mixed into one partition
of time. Neither timer set is a hardware-cycle profile. [R3]

D1 found 490 of 656 coarse admission keys and 121 of 175 strict keys with multiple
prefix permutations. Those keys omit native member identity, four junk vectors,
observations and complete operator dependencies. They are descriptive matches,
not operational identities or safety guards. Final sparse selection was tiny;
selected-row generation was tiny after existing reuse; the top protected-kernel
keys did not justify the selected template pilot. D2/D3 did not execute. [R2–R3]

The previous plan specified a priority screen, not a universal no-go theorem.
Small individual keys also do not bound one lawful operation spanning several
keys. But no common pre-work guard for such a union was established. Widening the
histogram or reducing the threshold would not answer that missing question.

## 3. What source inspection adds now

### 3.1 The “discovery” timer crosses initialization and partition-node preparation

`lift_policy_quotient_pass_task` runs `oracle.initialize_cooperatively`, then its
selected-locator loop, then builds cached observation/exact partition nodes.
Only afterward it assigns `carrier_discovery_ns = elapsed_certification_ns()`.
That helper measures from `session.started_at`, not the selected-row function.
On persistent/resumed passes it is especially important to record pass and session
origins instead of summing successive cumulative snapshots. [R4–R5]

The readout therefore directs us to initialization and node preparation. It does
not establish that either accounts for the full remainder, or that removing
strict work would be sound or preserve the cheaper controller.

### 3.2 There is an actual shared-observation interface with uneven use

`PolicyObservationNode` supports `selected_action_source` and `successor_source`.
The observation fixed-point routine validates one-level authorities and builds
propagation groups from those authorities plus direct requirements. Without an
explicit authority it uses a node's own state ID in the group key. [R8]

The inspected `ProductionPolicyOracle` producer currently supplies copied selected
actions and successor vectors using `{state, policy.selected, {}, policy.successors}`,
leaving these optional sharing fields empty. A separate refinement-graph producer
already populates these authorities from interned selected actions/shared edges.
Thus the feature is not missing globally; a candidate integration seam exists in
this producer. [R7–R10]

This is not yet proof of savings. Equal operator names do not establish equal
complete selected contracts/routing observations, and equal local role masks do
not establish equal successor dependencies. The next test counts actual lawful
authorities and cost. It must reuse existing collision-checked semantic inventory,
not insert aliases based on names or goal count.

### 3.3 Repeated full-container accounting is visible in concrete loops

During observation input assembly, the producer appends a node and calls
`policy_observation_nodes_bytes(nodes)` after each append. That helper walks every
current node and nested selection/successor storage. Later assignment loops call
`policy_observation_fixed_bytes(fixed)` repeatedly while moving results. [R7, R12]

For n constant-sized items the prefix walk examines n(n+1)/2 item positions, not
n. This is an algorithmic operation-count fact; its time share in A4 is unmeasured.
An incremental sum could preserve the same inventory, but only with actual
capacity/lifetime deltas, correct transfer bookkeeping, saturation handling and
concurrent parent/child ownership. Merely skipping memory checks is prohibited.

The oracle already has a cached non-child estimate, projected growth and a
512-check full-audit interval. Partition nodes are already cached once for replay.
Those existing optimizations must not be repitched as missing. [R5, R11]

### 3.4 Partition preparation recomputes symbolic requirements per locator

The `replay_node` lambda unions selected-row and alternative routing requirements,
then canonicalizes the requirement and evaluates it against that carrier's exact
features. It is called for both observation-disabled and observation-enabled
projections per locator. Alternative-descriptor payloads have already been
collision-checked and interned. [R5, R8]

A valid opportunity is to prepare the **requirement** once for a complete input
combination and still observe each native state separately. Reusing the observed
feature result or partition key for a different state is not justified. Keying
only by alternative-count, selected operator number, or pointer address surviving
a generation change is insufficient.

## 4. Decision

Select **strict-preparation work reuse**, not another general role-template search.
First reconcile the measured prefix into its real owners. Then implement one
measured treatment in an existing owner: shared observation input/transfer work,
shared canonical requirement preparation, or correct incremental accounting of
one repeatedly walked inventory. The two latter branches are alternatives, not an
automatic shopping list. [Selected]

This preserves a narrower useful form of Oliver's intuition: unequal native
states may share the symbolic question “what must be observed?” while their
features, probabilities, actions and values stay separate. It does not demonstrate
broad heterogeneous policy compression or justify a state/value quotient.

No new broad role census, larger permutation sample, symbolic MDP engine, response
cache, alternate candidate vocabulary, cost objective or global sweep is selected.
D1 gives enough evidence to leave the cheap selected-row and final-selection paths
alone. The remaining high-cost prefix provides a concrete next location.

## 5. Why this is a capability enabler, not an automatic cost reduction

The strict path participates in constructing and certifying the stronger original
controller. Making its preparation cheaper may release time/memory for useful
policy work or deliver the same verified policy earlier. It does not by itself
choose a cheaper crafting method. Require a same-input stage result and same-budget
whole-run comparison; forced 240-second total duration is not a throughput metric.

A faster phase with unchanged root cost may be a useful result. A smaller graph or
a cache-hit counter alone is not. If the strict preparation is not critical to
first usable policy, report later stronger-policy time separately from first U.
Do not shorten the deadline or skip proof to manufacture a win.

## 6. Research interpretation

Dataflow fixed-point computation and sharing invariant program inputs are established
techniques, not newly discovered symmetry theorems. Partial evaluation separates
static input from later dynamic input; that is the useful analogy for preparing
contracts once and applying them to different native states. [P1–P2]

Incremental verification of related Markov chains is another established direction,
but its stable-subgraph assumptions have not been established for these candidate
pairs. No response/factorization cache follows from this review. [P3]

The canonical role, continuation-port and guarded-specialization arguments remain
valuable. Their native high-impact application is still open. This programme must
label exact metadata reuse honestly rather than use it as a rhetorical replacement
for the stronger hypothesis.

See [SOURCES.md](SOURCES.md) for the inspected source ranges and literature.
