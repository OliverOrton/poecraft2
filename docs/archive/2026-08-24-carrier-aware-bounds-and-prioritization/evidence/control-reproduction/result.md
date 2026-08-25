# Retained-Control Reproduction Audit

Date: 2026-08-24

## Conclusion

The Gate 2 ordering candidate did not regress the Warlord or non-armour
controls. The plan mixed pre-exact-terminal acceptance artifacts with the
current exact-explicit-affix solver contract, and one current control was never
committed as a reproducible input artifact.

The first Warlord divergence is commit `2b8d5ac` (`Complete exact-goal carrier
ladder`). That checkpoint intentionally changed success from "enough requested
slots at the requested rarity" to "enough requested slots and no unrelated
explicit affixes." The accepted `224.123858897249`-Chaos Warlord report was
generated at `c192311`, before that contract change. Its policy uses one
Warlord Exalted Orb and repeated Harvest Reforge Fire actions to obtain
`PhysTakenAsFireInfluence2`; under the old terminal predicate it may finish
with the requested affix alongside junk. It is therefore not a valid exact-
terminal value or policy-hash authority for the current solver.

This is a control-definition defect in the active plan, not unexplained build
drift and not an ordering regression.

## Warlord reproduction matrix

| Source | Terminal contract | Result |
| --- | --- | --- |
| `c192311` | Pre-exact goal-plus-junk success | Exact `224.123858897249`, policy compiled and independently matched in `2116.5308` ms total. |
| `2b8d5ac` | First exact-explicit-affix checkpoint | `max_solver_owned_bytes` refusal after `27716.1501` ms total; lower `212.3`, no policy. |
| `1f68497` and `b6fb861` | Exact-explicit-affix successors | Same 1 GiB refusal class. |
| Clean `526ff6f` / restored Gate 1 | Current exact contract | 1 GiB refusal with lower `212.3`, but an independently exact-evaluated proper upper of `307.556312036793`. |
| Clean `526ff6f`, temporary 2 GiB cap | Current exact contract | No memory-cap hit, but the 60-second watchdog expired without closure. Policy evaluation reached a largest SCC of 250, 8,468 sparse iterations, 67,107 kernel groups, and 183,247 collapsed policy states. |

A temporary `2b8d5ac` experiment restored the old one-goal carrier ordering
without restoring the old terminal predicate. It still hit the memory cap.
That rejects carrier-subset order as the owner of the historical value change.
Raising only the memory cap also does not recover exact closure inside the
control's current 60-second window.

The current `307.556312036793` proper upper is a valid current-semantics
bounded result. It is not permission to relabel the Warlord case exact: the
lower remains `212.3`, the action envelope is not closed, and the accepted old
policy hash describes a different success predicate.

## Other retained controls

The tracked non-armour partial-five Bow fixture is current-semantics authority
for a 10-second requested bounded finish. On the restored Gate 1 tree it
publishes an independently exact-evaluated `6026985788.49406` upper in
`55791.0425` ms total. The Gate 2 candidate publishes the identical upper in
`55291.8589` ms. The `2042605033.16647` artifact named by the plan predates the
exact-terminal boundary, and the later owner-reported
`577532.360249086` result used an uncommitted 40-second override. Neither is a
reproducible baseline for the tracked 10-second case.

The tri-elemental Spine Bow recovery is documented in `HANDOFF.md` with a
current-semantics upper of `79285.28516643059`, 83 nodes, and 239 edges, but
the exact input request and report were not committed. Searches of the
repository, retained historical worktrees, and git history found only the
summary and unrelated natural-T1 fixtures. Reconstructing a case from that
prose would create a new control, not reproduce the accepted one.

## Required successor boundary

Before Gate 2 can be resumed, the retained-control authority must be repaired:

1. Oliver must choose whether Warlord remains an exact-closure performance
   requirement under current terminal semantics, or becomes a bounded proper-
   policy control using a newly pinned current-semantics report. Recovering
   exact closure is a separate solver boundary; the old value and hash cannot
   be retained.
2. The non-armour acceptance must either use its tracked 10-second case and a
   newly pinned current-tree baseline, or commit a distinct 40-second fixture
   and report. The two stop windows must not share one expected value.
3. The tri-elemental request must be committed as an explicit case/manifest
   and regenerated from a clean current build before it can gate future work.

After those authorities exist, the restored Gate 2 ordering candidate can be
reapplied and compared against them. Existing evidence already shows a roughly
9.25x five-T1 upper improvement, exact fixed-work parity after restore, and no
measured regression against the current tracked non-armour or Warlord
bounded results. Gates 3-5 remain out of scope until the corrected Gate 2
qualification passes.

## Evidence

- `warlord-c192311.json`
- `warlord-2b8d5ac.json`
- `warlord-1f68497.json`
- `warlord-b6fb861.json`
- `warlord-2b8d5ac-one-goal-legacy.json`
- `warlord-526ff6f-2g-cap.json`
- `../gate2/warlord-gate1-reproduction.json`
- `../gate2/non-armour-gate1-reproduction.json`
- `../gate2/non-armour-gate2.json`

All reproduction work was diagnostic. No production solver behavior from the
bisect or cap experiments was retained. The full acceptance pipeline, release
WASM, web tests, 10,000-run verification, and rendered review were not run.
