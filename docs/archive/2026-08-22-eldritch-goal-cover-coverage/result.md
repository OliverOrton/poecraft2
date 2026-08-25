# Eldritch Goal-Cover Coverage Result

**Status: completed and accepted on 2026-08-22.**

Plan: [Eldritch Goal-Cover Coverage](plan.md)

Starting checkpoint: `d2341dd`

Implementation checkpoint: `300e9ed`

Release WASM checkpoint: `f74a549`

Compact evidence: [acceptance summary](evidence/acceptance-summary.json)

## Result

Eldritch-eligible Helmet, Body Armour, Gloves, and Boots solves no longer lose
their completion-cost seed solely because automatic side actions exist. The
goal-cover MDP now contains explicit optimistic projections for automatic
Eldritch Annul, Chaos, and Exalt on either side. Each projection retains the
real final-currency price but grants setup/dominance and outcomes at least as
favorable as the exact carrier-local option, preserving an admissible lower.

The independent floor now survives an open incremental action envelope and an
unfinished strict refinement. Publication uses `global_action_relaxation` only
up to the independently proved numeric amount; restricted-policy values still
have no global authority. The strict clean-state refinement remains disabled
on eligible sessions until it models the same automatic option rows.

Synthetic Restart also uses its exact deterministic fresh carrier for the
operator lower. It no longer receives goal progress from the item it discards.
This correction is deliberately not generalized through
`destroyed_affixes`: that contract means an affix *may* be destroyed, so using
it as must-destroy authority for Annul or stochastic reforges could prune a
real optimum.

## Soundness controls

Native controls prove a finite positive eligible seed, unchanged ineligible
and missing-price fallbacks, monotone option addition, deterministic repeated
construction, and exact Bellman inequalities for direct/setup-bearing prefix
and suffix Annul, Chaos, and Exalt rows. A small exact compiled policy remains
above the heuristic and passes 10,000-run simulation. Open-envelope and
unfinished-refinement publication retain only the independent floor and reject
numeric overclaim.

The Restart regression compares the new lower with the legacy carried-progress
expression on a three-of-four crafted-goal carrier, proves a strict increase,
and matches the relaxation of the evaluator's single fresh successor. The full
native solver suite reports 96,516 checks and zero failures.

## Checked primary

The native Allflame four-natural-T1 Conquest Lamellar primary reached the
300-second watchdog with:

- lower `21.772459401332767` instead of zero;
- unchanged accepted upper `3745.7295960574743`;
- 3,419 discovered / 1,207 expanded states and 2,212 frontier states;
- 11,384 rows, 29,135 transitions, and 478,002 reforge work;
- 419,303,854 live/peak owned bytes at the plateau; and
- no final compiled strategy.

The first positive lower appears during the early focused solve, but neither
it nor the exact Restart correction changes the later refinement boundary.
The from-scratch cover is only about 21.8 Chaos, far below the 3,745.7
executable incumbent, so the claim that Restart alone should prune the clean
branch is falsified on this primary. The remaining stop is exact
alternative/refinement work and weak rare-carrier proof strength, not a
missing lower publication.

## Controls and qualification

- `powershell -File scripts/build.ps1`: pass.
- `powershell -File scripts/dev-engine.ps1 -Task Test -Suite solve`: 96,516
  checks, zero failures.
- Automatic Eldritch exact compiled-policy control: exact `6.630492`, 10,000
  simulation runs, empirical `6.820563`.
- Warlord non-Eldritch control: exact lower/upper
  `224.1238588972487`; 10,000 runs, empirical `223.95892804999892`.
- Release `powershell -File scripts/build-wasm.ps1`: pass.
- `npx tsc --noEmit`: pass.
- Complete web tests: pass, including 28/28 release-WASM engine smoke checks.
- `git diff --check`: pass before the checkpoints.

The regenerated current Bow corpus control did not supply the planned positive
non-Eldritch A/B: it stopped at the root reforge-work cap with lower zero. That
secondary assumption is rejected rather than promoted as evidence; the exact
Warlord run remains the non-Eldritch control.

The full repository acceptance pipeline and a release-WASM five-minute primary
were intentionally not run. The native primary already characterized the
unchanged finalization boundary, while release WASM and its non-visual product
tests qualified the changed engine artifact.

## Possible successors

No implementation boundary is active. Two separate owners remain:

1. strengthen rare-carrier/destructive-operator completion relaxations using
   a proved union of possible successor goal masks and optimistic post-action
   carrier shapes; do not interpret may-destroy selectors as must-destroy; or
2. continue the measured exact alternative/refinement owner that holds the
   primary at the 419 MB plateau after the executable upper is already found.

The delayed Essence/Harvest/Fossil executable-upper scheduler and broader
goal-slot row equivalence remain separate findings.
