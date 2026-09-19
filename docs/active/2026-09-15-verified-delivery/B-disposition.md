# B — neither successor passes the selection gate

The two permitted diagnostics are complete on one Ring family: the saved
execution-count controller and its cheaper native successor. No search was run.
[Observation receipt](B-observation-receipt.json),
[cold-check receipt](B-response-receipt.json) and
[comparison](B-comparison.json) retain the measurements and identities. The
[probe sources](probes/observation-probe.cpp) call existing native owners;
[response profiling](probes/response-cost-probe.cpp) preserves the full fixed-graph
state/pair/transition/memory allowances: 2M states, 10M pairs, 40M transitions and
4 GiB checker, with original pinned prices. It adds a conservative 400M logical
work guard; the public standalone checker normally has no finite work limit.
Both use only 6.8M work, so this guard does not truncate either result. Fresh host admission reserves
8 GiB plus at least 20% physical memory outside the worker. Both checks completed
in one supervised 20.470-second process, without timeout or survivor.

| Measurement | Earlier Ring | Retained native Ring |
|---|---:|---:|
| Original cost | 227377.06545019808 | 220743.46354691702 |
| Primitive actions | 583110.3499596338 | 564673.7576801107 |
| Success probability | 1 | 1 |
| Control nodes / edges | 161 / 528 | 165 / 533 |
| Native raw / refined pairs | 185337 / 10994 | 185344 / 11001 |
| Cold wall time | 10.022 s | 9.999 s |
| Pair discovery / refinement | 2.169 / 4.035 s | 2.174 / 3.984 s |
| Component construction / solve | 0.255 / 2.247 s | 0.237 / 2.269 s |
| Peak estimated owned bytes | 267559099 | 267566717 |
| Retained result bytes | 43536415 | 43542288 |
| Logical work | 6796680 | 6796680 |

## Representation result

`derive_node_observation_requirements` reaches its native cyclic fixed point in
25/20 rounds (6.176/4.758 ms), giving 6/8 unique requirements. Nineteen of 161 and
22 of 165 nodes differ from the global union, but 142/143 nodes already require
that entire union. Both unions retain item mask 1015807 and nine affix selectors.
Both native layouts retain four goal slots, 109 junk classes and 30 count-membership
observations. The new layout also has one discriminating tag, versus zero before.
No smaller global class partition or support was established.

`CountObservationMembership` denotes the whole membership vector; these contracts
do not identify particular predicates that can be omitted from its 30 entries.
Zero tag IDs in this requirement projection does not remove the native action's
tag requirement. Required legality, native goals, route conditions and surviving
affix/lock flows remain in the existing contract fixed point. A smaller local set
therefore grants no global layout reduction or whole-member equivalence proof.

The no-row observation probe used a 200k model state allowance and a 4 GiB
observation cap; it performs no discovery and is not full evaluator qualification.
It examines complete-controller evaluator layouts, not an exact replay of every
private candidate constructor. That constructor additionally copies parent count
observations and appends bench-conflict requirements before building its private
`CalcContext` (`solver_solve_return_bridge.cpp`). No numeric private-layout
reduction, restored-class count or saved row support is claimed. Missing
correspondence is a failed advancement gate, not permission to remove observers.

## Response result

The complete graphs share many local operation bodies: 161 old and 164 new nodes
belong to shared local syntax classes. Once exact route conditions, priorities,
defaults and successor control classes propagate to a fixed point, only `goal`
and `offpolicy` retain matching complete continuation syntax. The offline analysis
takes about 31 ms. It strips only node IDs and display `expected_cost`, and edge
IDs/endpoints used for adjacency. It is neither a native observer engine nor
semantic physical-row matching. A changed return boundary can propagate through
many syntactic continuations while still permitting an interior response; this
result does not prove that such an interior is absent.

**No reusable physical interior was established.** The seven additional raw and
refined pairs are cardinality differences, not seven proven new physical entries.
Native member correspondence, semantic overlap, boundary size/fill, dependency
matching, invalidation and response storage/scratch remain unknown, represented
as null in the receipt. Existing unchanged-selected-row reuse remains the baseline.
Node names, equal totals and local operation matches receive no reuse credit.

For these two actual checks, cold validation costs 20.021 seconds total. Even
free elimination of the second check's measured component construction/solve
would target only 2.505 seconds; its discovery/refinement still costs 6.158 seconds
unless separate semantic correspondence is proved. This is a measured phase
ceiling for that narrow proposal, not a bound on every possible reuse algorithm.
Matching, response building, invalidation, memory overlap and pilot cold validation
have not been avoided. No all-in saving or break-even over these two checks is
established; extrapolating to hundreds of comparisons would change the request.
Independent checking keeps separate terminal outcomes, primitive/resource rewards
and global properness. No response matrix, dense inverse or executable tail was
built.

## One-page successor decision and gate

**Select neither; stop this programme.** Representation has no demonstrated
global discriminator reduction, and response reuse lacks a measured reusable
native interior or credible all-in advantage. This is a bounded negative result
for one Ring family, not a universal rejection of either conditional argument.

A future separately selected representation experiment must first show a smaller
global layout from the existing contracts at an actual private constructor,
including bench/routing observations and complete native member domains. It is
the tie-break preference only after that gate. Negative controls must retain
mixed bench conflicts, equal goal masks with different blockers, surviving locks,
hidden offers/checkpoints, opposite-side effects and native junk-free goals.

A future response experiment instead needs a native item/control correspondence,
an explicitly changed boundary, measured sparse fill and total matching/build/
invalidation/validation/storage cost over the actual comparison count. Negative
controls must change router conditions, reward prices, member domains, hidden
context and recurrence; proper interiors with an improper boundary must refuse.
Full cold evaluation remains acceptance authority. Neither future experiment is
implemented or automatically authorized by this decision.
