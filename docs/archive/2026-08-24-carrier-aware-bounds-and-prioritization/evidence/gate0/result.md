# Gate 0 — Current-tree attribution

Gate 0 passed. The measurements support the plan's two separate owners:
carrier/action ordering is the first five-T1 bottleneck, while current lower
coverage collapses on the dirty and protected carriers that dominate the
reachable and published policies. No Gate 1 behavior change is inferred from
these measurements.

## Neutrality

Three before/after full-evidence runs of the current exact-semantics
four-of-five Conquest Lamellar retained:

- `bounded_feasible` / `requested_bounded_finish`;
- lower `0.01165`, independently evaluated upper
  `2698.8747960143623`;
- 6,820 discovered, 6,812 expanded, one goal, and 526 policy-reachable
  states;
- compiled graph census 211 nodes / 559 edges;
- work-policy hash `6f86a0d538abc4a8`; and
- the same selected action/material set.

The before median total wall was `21,978.1029 ms`; the after median was
`21,748.0132 ms` (`-1.05%`). The requested-finish graph's transition hash is
not stable at this wall boundary even on the untouched executable: the three
before runs contained `0524511740203811` twice and
`7210b3faa817fee4` once. Therefore the strict hash-neutrality check used the
same 1,000-expansion fixed-work case against a clean `526ff6f` worktree. It
retained value `59810.9537769745`, 3,315 discovered / 1,000 expanded states,
18 goal states, 163 policy-reachable states, graph census 51 / 149, transition
hash `fb8dc170b29920df`, policy hash `1b98ca41e69ad1b1`, and the exact selected
action set bit-for-bit.

The previously committed `anytime-capped-1000-native.json` is not a
current-tree baseline: a clean `526ff6f` rebuild does not reproduce it. It was
excluded in favor of the clean-worktree measurement above.

## Required attribution answers

1. **Start-lower owner.** The fractured four-of-five start is clean-ineligible
   because of `fractured_goal`; its `0.01165` lower is owned solely by the
   universal cover. The clean five-T1 start is clean-eligible; universal is
   `0.01165`, clean MDP is `36.4286171890906`, strict-clean is disabled for
   this Eldritch session, and the selected maximum is therefore owned solely
   by clean MDP.
2. **Clean coverage loss.** Four-of-five loses clean coverage on 5,465 / 6,812
   expanded states (`80.22%`) and all 526 policy-reachable states (`100%`).
   Five-T1 loses it on 11,500 / 12,847 expanded states (`89.52%`) and 1,780 /
   2,884 policy-reachable states (`61.72%`). Rejection reasons are reported as
   independent counters rather than one mutually exclusive label.
3. **Satisfied-but-dirty zero.** Four-of-five has 13 satisfied-mask but
   nonterminal expanded states; 12 are non-clean and all 12 select zero.
   Five-T1 has 24; 23 are non-clean and all 23 select zero. This is the exact
   non-goal-aware bound hole proposed by Gate 3.
4. **Operator pruning.** Five-T1 performs no incumbent operator-lower
   evaluation and therefore no operator-lower prune: its initial failure is
   ordering/upper ownership, not a weak value at an already-active prune call.
   Four-of-five proves that the call site is live: 4,721 evaluations yield
   3,439 state-incumbent prunes, concentrated in expensive currency, Harvest,
   and Fracture families. Temporary-bench evaluations have negative margins
   and no prune; carrier/action automatic admissions are not evaluated by
   that call site. Gate 2 must therefore remain first, and Gate 3 must show a
   measured new separation rather than assume one.
5. **Five-T1 goal discovery.** Yes. The new baseline still discovers zero
   goal states: 14,225 discovered, 12,847 expanded, 1,378 frontier, and 2,884
   policy-reachable states. Its exact-evaluated upper is
   `14454067.4260706`; first finite upper appears at `40.2376 ms`, while first
   independent verification does not complete until `68,546.0167 ms`.

The scheduling attribution also makes the order bottleneck concrete. In the
five-T1 run, only 1,272 / 21,438 (`5.93%`) admitted carrier/action pairs are on
four-of-five or five-of-five subsets, despite subset-round-robin enriching
those shapes to 232 / 1,633 (`14.21%`) carrier admissions. This supports a
within-bucket, non-filtering carrier/action priority rather than a new goal
order.

## Evidence

- `workhorse-before-{1,2,3b}.json`
- `workhorse-after-{1,2,3}.json`
- `fixed-work-baseline-head.json`
- `fixed-work-after.json`
- `fixed-work-no-evidence.json`
- `five-t1-attribution.json`

The native build completed after telemetry integration. Redundant partial
benchmark files from the initial baseline runs were removed; the complete
reports above are retained.
