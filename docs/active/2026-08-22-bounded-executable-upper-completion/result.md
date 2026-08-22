# Bounded Executable-Upper Completion And Imprint Applicability Result

**Status: complete and accepted (2026-08-22).**

Parent: [plan](plan.md)

## Result

Incremental executable-upper passes now stop after one exact proper
fixed-policy evaluation. They no longer run Howard improvement merely to
optimize a policy on an open partial action graph. Alternatives evaluated
against one frozen carrier/frontier epoch are admitted before the next proof,
and final exact-envelope closure still runs ordinary Bellman selection over
all completed legal rows together.

The solver also retains the first stable materializable selected policy as an
anytime candidate. Finalization independently compiles and exact-evaluates
that immutable policy before attempting a cheaper broad policy. Appended
state-local action vocabulary does not invalidate the candidate when its
captured prefix is unchanged.

Calculator now sets `max_policy_refinement_states: 5000`. This is a
deterministic budget for optional post-solve direct certification or strict
lift; it does not cap main discovery, action admission, lower-bound work, or
the exact evaluation of the retained fallback. If direct certification
exhausts the budget after a fallback has been independently verified, the
solver publishes that fallback immediately instead of repeating the same
budget in strict lift. Native callers that omit the append-only option retain
the historical `max_discovered_states` refinement allowance.

## Imprint boundary

Imprint applicability remains exact and carrier-local. Discovery first asks
the native Bestiary mechanic whether checkpoint creation applies to the exact
carrier, then requires every program continuation to be legal across its
entire exact support. The four-T1 armour genuinely reaches eligible Magic
carriers, so rarity, goal count, or primitive price cannot soundly exclude
Imprint there.

When one Imprint grammar exhausts `max_imprint_program_work`, its staged
transaction is rolled back and the Imprint family remains an unresolved
exactness obligation. The same carrier is then replayed without Imprint so
unrelated Eldritch, Bench, protection, Harvest, Fossil, Fracture, and other
actions can finish. The already exhausted family budget is not spent again on
later carriers. Exact closure remains false and the final stop still names the
Imprint cap.

This is not an economic dominance claim. A cheaper route to an arbitrary
Magic item still cannot replace an Imprint restore; a future dominance proof
must return every relevant failure class to the same observable checkpoint
class at no greater expected continuation cost.

## Checked four-T1 measurements

Both runs use the pinned Allflame four-natural-T1 Conquest Lamellar request.
The disabled run changes only `consider_imprint_programs: false` and skips
simulation; the canonical fixture is restored with Imprint enabled.

| Measurement | Imprint enabled | Imprint disabled |
| --- | ---: | ---: |
| Solve wall | 14.204 s | 146.378 s |
| Native result | bounded / `max_imprint_program_work` | bounded feasible |
| Certified lower | 21.772459401332767 | 2889.7687877196995 |
| Independently evaluated upper | 3759.9763122101763 | 3759.9822404728984 |
| Action envelope | 0 unevaluated, 19,555 unresolved | 119,838 admitted, 0 unresolved |
| Upper passes / fixed-policy proofs | 7 / 7 | 98 / 98 |
| Policy evaluations | 35 | 327 |
| Optional direct certification | 5,000-state cap; strict lift not run | 5,000-state cap in 2.168 s; strict lift not run |
| Compiled graph | 87 nodes / 241 edges | 68 nodes / 197 edges |

The enabled run continues well past the old Imprint abort boundary before it
publishes: its measured rows include 2,278 Harvest reforges, 10,737 Fossils,
7,016 other automatic rows, and 291 Fracture rows. The lower remains the
universal action relaxation because Imprint is open. With Imprint explicitly
out of scope, the complete remaining envelope closes and the lower tightens
to 2,889.77; the retained executable upper remains about 3,759.98.

The final enabled strategy passed independent exact evaluation and 10,000
Simulator trials: 10,000 successes, zero failures, zero off-policy failures,
and sampled mean cost 3,737.4451776349074. Solve plus compilation and
verification completed in 74.072 seconds.

Evidence:

- [enabled accepted report](primary-imprint-enabled-accepted.json)
- [enabled accepted strategy](primary-imprint-enabled-accepted.strategy.json/conquest-lamellar-allflame-four-natural-t1.strategy.json)
- [disabled controlled report](primary-imprint-disabled-capped.json)
- [disabled controlled strategy](primary-imprint-disabled-capped.strategy.json/conquest-lamellar-allflame-four-natural-t1.strategy.json)
- [Warlord control](warlord-control.json)
- [Imprint retry control](imprint-retry-control.json)

## Controls and acceptance

- Warlord Exalt closes exactly at `224.123858897249` chaos in 2.148 seconds;
  its 8-node / 13-edge strategy completed 10,000 trials at 100% success.
- The dedicated Magic-carrier Imprint retry closes exactly at
  `252.653520212745` chaos in 0.610 seconds; its 9-node / 11-edge strategy
  contains native Imprint create/restore and completed 10,000 trials at 100%
  success.
- `powershell -File scripts/build.ps1`: pass.
- Native engine tests: 177,542 checks, zero failures.
- `powershell -File scripts/build-wasm.ps1`: pass; release WASM rebuilt.
- Complete `npm test`, including 28/28 release-WASM smoke checks: pass.
- `npx tsc --noEmit`: pass.
- Full repository pipeline: deliberately not run.
- Rendered/visual review: deliberately not run; it remains Oliver's.

The compiler also stopped emitting non-finite `expected_cost` metadata as bare
JSON tokens; the field is omitted unless the complete region value is finite.
Native compiler coverage pins valid JSON for that case.
