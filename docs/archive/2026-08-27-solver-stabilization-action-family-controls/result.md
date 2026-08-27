# Solver Stabilization And Action-Family Controls Result

**Status: completed 2026-08-27.**

Parent: [Plan](plan.md)

## Result

The current native baseline was already truthful: the historical 13
goal-progress-gated failures and empty-JSON cascade did not reproduce on the
starting source. The complete native solve target passed 100,169 checks before
the new family contract was added.

The engine now owns a stable solve-envelope family vocabulary and accepts
`disabled_action_families` on goals. It filters direct candidates and rejects
fixed or carrier-local generated programs whose family or named primitive
dependencies are disabled. Unknown names fail request parsing. The C ABI and
WASM action descriptions expose native primitive family metadata, while
telemetry records the restriction and direct-family pruning. Empty or omitted
restrictions preserve existing behavior.

Calculator exposes these controls in an advanced diagnostic panel, defaulting
to every family enabled. Product-envelope and scoped Solve requests carry the
same restriction; exact Odds requests do not. Results explicitly say that
bounds and exactness apply only within a restricted requested envelope.

## PDR Attribution

The retained control and one matched `temporary_bench`-disabled run used the
same four-mod PDR Conquest Lamellar witness, 50,000,000 logical-work allowance,
1 GiB selected native-memory limit, product profile, Imprint off, economic
Restart off, and skipped Simulator verification.

| Measure | Retained control | Temporary Bench disabled |
| --- | ---: | ---: |
| Stop | 300 s watchdog | `max_solver_owned_bytes` |
| Native solve time | 328.33 s | 224.22 s |
| Exact states | 13,991 | 43,535 |
| Selected rows completed | 13,623 | 21,182 |
| Alternative rows completed | 14,054 | 0 |
| Logical / V3 reforge work | 49,999,992 / 43,404,344 | 6,240,606 / 2,415,465 |
| Alternative obligations created | 299,394 | 0 |
| Certified / partial alternatives | 15 / 14,038 | 0 / 0 |
| Policy improvements | 0 | 0 |
| Selected native peak | 1.31 GB | 1.072 GB |
| Independently evaluated policy | retained control `7866.432124027084` | `7862.857725228987` |

The ablation policy completed exact graph evaluation with success probability
one, zero off-policy mass, and complete pricing. Its final solve result was
truthfully `bounded_feasible / refused_resource_cap`, lower
`21.772459401271156`, upper `7862.857725228987`.

This does not show that temporary Bench should be removed. The restricted run
never reached alternative-obligation construction: strict selected-carrier
growth expanded to 3.11 times as many exact states and hit the memory boundary
first. Its shorter wall time is an earlier refusal, and its slightly cheaper
anytime incumbent cannot establish an unrestricted improvement because
disabling actions changes the explored proof path. The useful conclusion is
that temporary Bench does not own the control's 299,394 broad obligations and
may provide a more compact inherited policy/partition.

Local ignored evidence:

- `build/performance/native-solver-pdr-temporary-bench-off-v1.json`
- `build/performance/native-solver-pdr-temporary-bench-off-v1-strategy.json`
- `build/performance/native-solver-exact-same-side-feature-key-pdr-case-conquest-lamellar-allflame-clean-4-pdr-product8-v1.json`

## Next Owner

Make exact broad destructive-row work cooperative and resumable before adding
disk replay. The current control still contains a native work item above 100
seconds, while the temporary-Bench ablation demonstrates that family removal
can merely move the cap to an earlier carrier-construction phase. Once row
cursors are stable, a deterministic development checkpoint at the coarse to
strict boundary can make further family attribution and proof repairs much
cheaper without rebuilding the graph.

Five-goal tuning, state-dimension removal, mechanic changes, and broad family
matrices were not started.

## Acceptance

- fresh native release build passed;
- focused API, S8.3 automatic-option, and complete solver-solve suites passed
  2,643, 541, and 100,169 checks respectively;
- release WASM rebuilt and the live WASM solver/action-family smoke passed;
- the final repository pipeline passed 3,467,980 native checks plus ingest,
  artifact, benchmark-spec, binding, and nonvisual web tests;
- `npm test`, `npx tsc --noEmit`, and `git diff --check` passed.

The first repository pipeline attempt exposed a pre-existing timing race in
the exact-evaluation cancellation smoke: its three-node graph could complete
in one warm native work item before the cancellation message arrived. The
test now uses a 32-operation proper Alteration cycle so its first progress
notification is a real cooperative boundary. The standalone web suite and
the repeated full pipeline both pass that contract.
