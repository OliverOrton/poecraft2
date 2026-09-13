# Pinned evidence and primary research

Reviewed remote main: `d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d`. Source reads are read-only. Historical report values remain observations from their recorded builds; presence at this commit does not turn them into fresh runs. Code search was used for navigation only; material source conclusions were checked by pinned fetches.

## Repository evidence

| ID | Source | Use |
|---|---|---|
| R1 | [Commit](https://github.com/OliverOrton/poecraft2/commit/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d) and [HANDOFF](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/HANDOFF.md) | Current delivered checkpoint, partial status, no new exact closure, limits |
| R2 | [Latest living record](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/active/2026-09-13-policy-economics-checked-potentials/README.md) | Native blocker repair, matched 400M results, lower pilot ceilings and actual qualification |
| R3 | [Latest qualification](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/active/2026-09-13-policy-economics-checked-potentials/qualification.json) | Existing receipt to be reused/reconciled by the implementation session; this research did not independently rerun or rehash native artifacts |
| R4 | [Bow evaluation, lines 1–445](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/active/2026-09-12-adaptive-dirty-guidance/final-bow.json#L1-L445) | Expected operations, priced accounting, fractured-Magic region; later current record confirms saved-policy re-evaluation |
| R5 | [Current nonempty candidate selector, around lines 755–935](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/engine/src/solver_solve_return_bridge.cpp#L755-L935) | Requested operation types, Rare/progress guards, existing candidate owner, no case-global action exclusion inferred |
| R6 | [OptionKernel, around lines 70–130](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/engine/src/solver_calc_types.hpp#L70-L130) | Existing `expected_primitive_actions`, expected resources and first-exit convention |
| R7 | [Checked estimate rays](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/mathematics/lower-bounds.md#checked-estimate-rays) | Feasible base, final native re-minimization, direction limitation versus model ceiling |
| R8 | [Numerical acceptance](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/mathematics/numerical-closure.md) | Four layers, independent acceptance, residual amplification, scoped native versus formal arithmetic |
| R9 | [Upper authority, around lines 140–180](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/upper-authority.md#L140-L180) | Bench group-conflict counterexample and physical witness/fresh namespace |
| R10 | [Policy mathematics](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/mathematics/policies.md) | Occupancy, fixed-policy equations, entry domains, properness and retained falsifications |
| R11 | [Claims](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/claims.md) | Conditional statements/history, scoped acceptance; latest CLM-0002 application precedes repaired blocker |
| R12 | [Generated research state](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/research-state.md) | Series input declaration and observations ending at selective-growth results in inspected current view |
| R13 | [Research workflow and narrative](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/research.md) | Later blocker/potential application already narrated; existing series and handoff responsibilities |
| R14 | [Resource owner](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/resources-resume-replay.md#candidate-checker-and-native-headroom) | Real typed checker caps, aggregate memory, host reservation and cumulative work |
| R15 | [Series input](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/experiments/solver-research/backbone-pilot-v1.json) | Existing generator input named by R12; inspect actual schema before appending observations, not independently parsed here |
| R16 | [Benchmarking](https://github.com/OliverOrton/poecraft2/blob/d2b706a02b7b3bfa8e1695d2c4d88a726ccdba3d/docs/solver/benchmarking.md#research-series) | Existing regeneration route named by R12; no new reporting system proposed |

## Primary external research consulted

**E1. Johannes Schmalz and Felipe Trevizan.** [Solving Constrained Stochastic Shortest Path Problems with Scalarisation](https://arxiv.org/html/2508.17446v1), 2025. Read the scalarisation, policy, and warm-start sections. It provides a close reference for cost vectors and multiple objectives. Our proposal is a small candidate generator, not an implementation of CARL or its constrained-optimality guarantee. Its positive-cost/reachability assumptions and possible stochastic policy mixtures must not be imported silently.

**E2. Krishnendu Chatterjee, Tim Quatmann, Maximilian Schäffeler, Maximilian Weininger, Tobias Winkler and Daniel Zilken.** [Fixed Point Certificates for Reachability and Expected Rewards in MDPs](https://arxiv.org/html/2501.11467v1), TACAS 2025. Read certificate scope and implementation. It supports independent checking of proposals but does not remove the need to establish that the finite model represents the native problem. The paper itself distinguishes verified checking from unverified model construction/export.

**E3. PRISM manual.** [Reward-based properties](https://www.prismmodelchecker.org/manual/PropertySpecification/Reward-basedProperties), checked September 13, 2026. Expected reward to a target and bounded cumulative reward answer different questions; multiple reward structures are explicit. Used for terminology and scope, not a recommendation to add PRISM as a dependency.

**E4. Anders Hansson and Bo Wahlberg.** [Performance Bounds for Rollout Policies in Stochastic Shortest Path Problems](https://arxiv.org/html/2605.22965v1), May 2026 preprint. Read the assumptions, hitting-time interpretation and conclusions. The conditional error analysis emphasizes occupation/hitting-time amplification. No uniform value-error bound, rollout guarantee or native improvement follows for this project's present estimates.

The elementary two-reward and scalarization calculations in this packet are derived directly and checked synthetically. No novelty claim or external-method speedup prediction is made.
