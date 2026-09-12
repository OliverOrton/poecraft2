# Resources: keep the new working envelope, change only real obstructions

The previous candidate-limit plumbing is IMPLEMENTED. This is a revision of the earlier resource proposal, not an instruction to re-add fields.

## Existing public controls

`PC_SOLVER_FLAG_DIRTY_CONTINUATION_SEARCH` opts a native Calculator-profile request into private dirty search. `pc_solve_options` exposes `candidate_max_states`, `candidate_max_pairs`, `candidate_max_transitions`, `candidate_max_owned_bytes`, guarded by struct size. Zero preserves inherited behaviour. Explicit candidate bytes fit in aggregate remaining ownership; a child is not an extra uncharged pool.

Source: [resource contract](https://github.com/OliverOrton/poecraft2/blob/23e03ba9d3fef2f67f2bf1597f69c2c36422e5a7/docs/solver/resources-resume-replay.md#candidate-checker-and-native-headroom).

## Primary matched native profile

Use the actual final checked requests under `docs/active/2026-09-11-dirty-state-continuation/native-600-final840/amulet/` and `/ring-four/`, resolving their manifest through existing tools. Do not pass this explanatory document as a runner schema or invent CLI flags.

| Owner | Effective current profile |
|---|---|
| Ordinary search | 200k discovered/expanded/state limits, 1,215,000 rows, 10M transitions, 100k sweeps |
| Shared work | 200M logical reforge work, product step eight |
| Aggregate solver | 8 GiB including child/checker overlap |
| Candidate checker | 4 GiB maximum within aggregate remaining; 2M states, 10M pairs, 40M transitions |
| Separate final checker | 4 GiB, 2M states, 10M pairs, 40M transitions |
| Output | 100k nodes, 400k edges, 64 MiB strategy JSON |
| Time | 600s requested finish, 840s native total watchdog, 870s outer safeguard |
| Host | 14 GiB process reservation including 6 GiB headroom; serial timed cases; fresh available-memory/commit check |
| External margin | At least max(8 GiB, 20% of physical memory) outside the reservation |

Keep true goal, full requested scope, zero-progress control, prices, generation activations and numerical tolerances unchanged. Original browser/product/default profiles remain separate preservation evidence.

## Authorized follow-through

Oliver permits larger native caps and reasonable time. If a newly necessary action or checker hits a named cap, inspect retained plus requested scratch and make one explicit profile change. Up to 16 GiB aggregate, 8 GiB candidate checker and 4M checker states are still permissible ONLY with safe host reservation and useful advancing work. Ordinary state/row limits may also be raised when that owner actually blocks the intended computation. Log effective values at the consumer.

The prior run could admit 14 GiB but not 28 GiB plus its required host margin. Do not assume the larger profile fits today. No paging-forced experiment or simultaneous unreserved large solvers. If more time is justified for one changing candidate, preserve separate requested-finish/native-final/outer-deadline owners and sufficient final-check allowance; avoid extending every case reflexively.

Time and resource limits are ceilings, not required run lengths. Do not spin merely to fill a horizon; if no useful current work remains, retain the normal honest bounded result without declaring unresolved proof closed. Known fixed controllers that already lose do not become better with more time. The retired Ring-two gated candidate does not need completion against its now much better 1620.3174 reference. Already-repaired replay and occupancy bottlenecks are not current missing features.

## Model waiting and deterministic batches

A batch can take longer than one native case. Its enclosing tool wait must cover the batch, not just one case's 870-second deadline. Wait windows are maxima, not sleeping periods. Use actual installed support; keep a terminal or event handle rather than launching duplicates. Native watchdog/cancellation continues during the wait.

Serial execution within one batch avoids contention. Parallel file/metadata reads or independent lightweight tests may be batched; concurrency of large native cases is not the default timing experiment.

## Fair interpretation

Capacity: same implementation with more resources. Algorithm: same resources and target with different implementation. Adaptation: same candidate repertoire and static guide with online correction enabled. Native opt-in delivery: actually exposed callable mode. Browser preservation: old browser default still works. None is a synonym for the others.

Do not rerun native work solely because a JSON duration was serialized as integer instead of float. Preflight typed identity. Any recovery of an existing comparison needs proven identical effective controls and immutable raw references through a versioned existing comparison owner—not edited historical metadata or waived mismatches.
