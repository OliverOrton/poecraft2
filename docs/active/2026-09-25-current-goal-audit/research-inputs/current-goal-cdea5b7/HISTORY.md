# Historical model audit: what is and is not comparable

## Confirmed boundary

- Parent: `82224d0b5075d32cb920fa889c6536a7121ffc70`.
- Change: `2b8d5acd8b7cd3598dca1756f7b5d53ab914c692`, authored/committed 23 August 2026 at 09:13:32 UTC; “Complete exact-goal carrier ladder.”
- Current reviewed head: `cdea5b7f9556ae30c43f9a74249d64b3dcf0f029`.

At the parent, the inspected CalcContext predicate returns `satisfied >= required_satisfied_slots()` after the rarity test. The inspected compiled goal is the conjunction of rarity and `all_of`/`at_least` requested predicates. At the change, native and compiled terminal handling require no unmatched explicit affixes. The current native predicate and compiled condition retain that requirement. [R4–R8,R10]

Both historical and current requests use the string `version: v1`. That string alone does NOT identify the historical terminal meaning. A replay/comparison needs the source epoch or an explicit resolved terminal fingerprint; do not infer the old goal from today's parser defaults. Existing compiled graphs carry their explicit predicates and must not be silently rewritten.

The commit is a **model boundary plus multiple implementation changes**, not a pure terminal toggle. “Exact-goal” in its title concerns target shape; it is not synonymous with proving an optimum.

## Initial evidence ledger

| Evidence | Terminal model in the inspected revision | Actual result class | Comparability caveat |
|---|---|---|---|
| Pre-change owner four-of-five Conquest, 2,083.88214353439 Chaos | Coverage-only | Independently evaluated bounded upper; lower 0.01165 and open obligations | Advanced start, older prices/scope; not an earlier exact five-goal solve |
| Pre-change four-goal 2,823.050846721888 controller | Coverage-only | Bounded; coarse 2,889.7687877196995 does not reconcile, public lower zero | Do not retrospectively promote to exact |
| Pre-change Warlord control, 224.123858897249 | Coverage-only algorithm/compiled contract | Actual report confirms converged/exact and matched cost | Empty Rare Vaal Regalia, one T1 PhysTakenAsFireInfluence2 goal; influence Exalt plus Harvest Reforge Fire. Reached-terminal equivalence to a clean target is not established |
| Pre-change Imprint control, 252.653520212745 | Coverage-only | Reported exact; actual report confirms `exact` and matched cost | Starts Magic but TARGET IS RARE with two requested families; synthetic retry-price overrides and Imprint scope |
| Boundary clean zero-to-five Conquest, 87,361.1690420501 | Clean explicit target | Bounded proper upper, lower 36.4286171890906 | Imprint enabled; requested 60 seconds, older data/build; not matched to current 240-second scope |
| Current A5, 85,558.70618560436 | Clean | Bounded verified controller | Not an optimality claim |
| Post-clean Regalia 65.60036144971359 reference | Clean in its recorded post-boundary profile | Retained exact control | Reuse its exact request/artifact and record source; do not infer universal clean-task ease |
| Current retention Finder A5, 524,079.6986482172 | Clean | Checked policy, experimental search; not exact closure | Different search lane, worse than Current |

The old Warlord report has approximately 1.874 seconds of solve time, one expected influence Exalt and 12.43962 expected Harvest Reforge Fire applications, with no cleanup operation in its reported material/action accounting. That is a concrete earlier coverage-only exact result; it is not a current clean one-mod benchmark. The old Imprint report starts Magic but explicitly targets Rare, so the phrase “Magic Imprint” does not make its final occupancy automatically equivalent to a Magic two-affix target.

This is an initial ledger, not an exhaustive history. Read at most the linked reports needed to resolve the Warlord/Imprint distinctions and the remembered comparison. Do not run every archived programme.

## Required per-result record

Record the source/build hash; native data and economy identities; original item including rarity/fractures/influence; ordered requested predicates and threshold; terminal semantics as used by BOTH search and compiled execution; primitive and programme scope (including voluntary Restart, mechanic-owned replacement, Imprint, disabled families and goal-progress restrictions); limits; actual status; lower/upper; proof closure basis; graph hash and independent evaluation. Preserve synthetic price overrides and numerical mismatch/refusal statuses.

A result's commit date alone cannot establish comparability. A historical graph may finish only at clean states even under a loose terminal. Conversely, a graph may rely on dirty successes; inspect the effective compiled predicates or an appropriately complete reached-terminal census, not its display name.

## Classification procedure

Use these labels:

- **different_target_confirmed**: terminal sets differ and no reached-domain equivalence has been established;
- **same_target_confirmed**: original goal, physical constraints and native domain imply the same terminal set;
- **policy_already_clean_only**: this checked policy's reachable loose successes are clean, but the competitor/optimality relationship still needs its own evidence;
- **same_target_other_inputs_differ**: terminal equivalent, but scope/prices/mechanics/start/build treatments prevent a direct performance comparison;
- **bounded_misremembered_as_exact**: source explicitly records an upper without closure;
- **unknown**: evidence unavailable or insufficient.

Only an unchanged-model optimality certificate plus an executable clean policy can support transfer of an old optimum. See the sandwich condition in [MATHEMATICS.md](MATHEMATICS.md). Mere post-hoc graph cleanliness is not enough to transfer historical cost, proof, or runtime across changed data and actions.

## Do not rewrite history

Keep original archives immutable. Add an interpretation/index entry and links in the current living record. The old 10,000-trial tests are historical evidence, not an authorization to use that trial count now. Do not rebuild old toolchains or data merely for this audit unless one missing comparison cannot be answered from source/report evidence and is explicitly selected.

The parent already had expensive refinement and failed coarse-value reconciliation. The boundary already had a strong clean controller. Both facts must appear in the final historical conclusion, even if the new controlled diagnostic shows a large clean-versus-loose difference.

All source references are in [SOURCES.md](SOURCES.md).
