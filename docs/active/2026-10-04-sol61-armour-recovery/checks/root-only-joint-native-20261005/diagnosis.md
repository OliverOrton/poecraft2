# Root-only baseline provenance failure

Implementation source is 073bb92a22981dc75d2328b899aef4fcb08dbe64;
checkout is 6a089cdc6fb25892a8fbbb3bd5cd9cae4e513539, engine tree
edade696d33378970b93dfb364e0f6ec50e77437. CI qualifies the freshly linked Tests
binary SHA-256 43f9ec25c820ef3e31bf5564f73e29eeb2465d3af678a4cb3c311a9728d01e8c,
20,595,019 bytes. The compact CI provenance preserves all three changed object
pins and the original successful build receipt locations/hashes.

One granted finite selector exits 3221226505 in 423.883 ms at the fixture's
"independently checked root-only fallback" requirement. Zero of three cases
complete. The log reports the common require helper at test_solver_solve.cpp:16990;
the failing requirement is at lines 17128-17131. No new root, build or source edit
occurs. LOCAL releases at 17:46:30 UTC with clean owned-job drain and empty census.

## Exact source-determined cause

The fixture at test_solver_solve.cpp:17109-17110 clears policy, policy_rows and
policy_reachable while values remains a nonempty length-n vector. Its root-only
flag causes populate_incumbent_policy to return immediately. Therefore the
proof copied into certify_initial_candidate also has empty policy/reachability
vectors and nonempty values.

The first OriginalRootController guard in solver_policy_assertion_work.cpp:401
requires policy.size() == values.size() and policy_reachable.size() == values.size(),
with every policy index invalid and every reachability flag zero. This is explicit
root-only provenance: an accidentally empty table does not satisfy the role.
The deterministic guard result is CompiledPolicyAssertionStatus::CompilationFailure,
with exact reason:

> original-root controller requires explicit supplied graph, root checking and no parent statewise authority

finish_failure sets executable/proper/zero_off_policy false and exact_cost to
infinity. The initial-candidate coroutine's verification gate then returns false.
advance_initial_candidate_publication consumes that boolean without logging it.
The retained-root validator additionally requires policy, policy_rows and
policy_reachable lengths to equal values length; the same malformed candidate
returns private_compiled_entry_witness_changed before receiving upper authority.

The aborted runtime log did not serialize the returned assertion or progress
ring. The status/reason above are source-determined from the exact pinned inputs
and guard, not a recovered runtime payload. This guard precedes graph parsing,
caller-scope checks, economy construction and independent evaluation. The failed
run therefore does not establish graph invalidity, a goal/scope mismatch, an
improper recovery or missing prices. Those remain subsequent checking obligations.

## Native laws reached before the failure

Control flow reaches the later baseline requirement only after the native
Annul/Scour/Alchemy assertions pass: root Annul has goal mass one half and one
positive loss port; Scour on that loss has one unit-mass successor; Alchemy on
that successor has one unit-mass return to the original root. Complete positive
support, native availability, the declared state/time/owned-byte bounds, native
row construction and graph emission also pass their preceding requirements.
These are finite fixture observations inferred from its first stopping point;
the log does not retain outcome keys or a checker cost.

The independently checked cost-13 requirement lies after the failure and was
never executed. Exalt construction and the cost-3 requirement also were not
reached. Neither number is an independently checked economic result.

## Smallest proposed fixture-only correction

Replace the three clear operations with role-correct placeholders:

```cpp
baseline.policy.assign(n, PolicyOperatorRef{}); // each index is kNoId
baseline.policy_rows.assign(n, std::numeric_limits<std::uint64_t>::max());
baseline.policy_reachable.assign(n, 0);
```

Keep only the root value finite, all parent actions invalid, all reachability
flags zero, root-only authority and the original independent checker. The existing
native root-only producer in solver_solve_return_bridge.cpp:2428-2429 uses the same
placeholder layout. The checker must still issue its exact root continuation
certificate and validate complete cost, properness, scope and off-policy mass.
No flags, measured cost or expected probability are forced to accept this fix.

A fixture-only failure dump of the existing progress_trace_json(0), certificate
flags, vector widths and certified_incumbent_invalid_reason would retain future
check_refused reasons instead of discarding them. No production instrumentation
or checker gate change is needed. These corrections are proposals only: native
source stays frozen, with CI owning later builds and no rerun granted here.
