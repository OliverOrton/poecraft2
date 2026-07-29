# Upper-Cap Sensitivity And Zero-Progress Live Renewal

**Status: active implementation and measurement boundary.**

Owner: Oliver

Branch: `codex/upper-cap-zero-progress-renewal`

Starting source: `19cd3e11e3a2397c2ad76e2fd8c1ae75273882d2`

## Objective

Determine whether the rejected high-impact executable-upper candidate stopped
before enough policy coverage was materialized because of the 200,000
discovered-state cap, measure where that state growth comes from, and retain a
sound zero-goal-progress live-renewal canonicalization if it independently
improves the accepted product/gated search.

This boundary does not equate a live carrier with `Restart`. Same-carrier
renewal consumes the selected crafting currency while retaining the owned
base. `Restart` discards the carrier, consumes the `base` price, and returns to
the original start. A certified goal-dead carrier may require Restart; a live
carrier may select an optional economic Restart only through the ordinary
priced Bellman action.

The milestone answers:

1. how high-impact policy coverage and upper-Q contribution scale at 200,000,
   400,000, and 600,000 discovered states;
2. which exact boundary stops every run;
3. how many discovered zero-progress states are live-renewable, goal-dead, or
   distinguished by persistent/observable state;
4. whether exact renewal authority proves a safe shared representative for a
   useful subset; and
5. whether increased caps or retained canonicalization produce nonzero
   executable-upper movement, strict admission, or the frozen 10% reduction.

An incomplete policy remains non-certifying. Zero movement at one cap or one
policy pass does not stop the investigation.

## Recovered Candidate Identity

The immediately preceding milestone preserved its strongest rejected
experimental behavior patch at
`build/experimental-behavior.diff`. Before any new implementation, this
boundary retains an exact tracked copy as `recovered-candidate.diff`.

The recovered patch identity is:

- SHA-256:
  `3bc21c3fc5cf5cf1b91607cb6c89102eaf7ffd651ed94c602b585bd7143f3501`;
- five source files;
- 1,016 insertions and 39 deletions; and
- scheduler/focused-upper behavior only. The run-local opt-in plumbing is
  reconstructed separately and remains disabled by product defaults.

The recovered 200,000-state control must reproduce or precisely explain any
difference from:

| Field | Rejected candidate |
| --- | ---: |
| Root lower / upper | `432.4068529534326 / 60341416.98784247` |
| Harvest Attack upper Q | `60341418.63180959` |
| Focused passes requested / proper / rejected | `5 / 5 / 0` |
| Scheduled high-impact states | `144` |
| Rows / transitions | `2228 / 842726` |
| Reforge work | `16746695` |
| Completed rows recomputed | `0` |
| Cap | `max_discovered_states` |
| Transition / policy hash | `d4346e90f923332c / 8b2a568f3c9cfd35` |

Recovery must preserve the exact shared graph, strict state IDs, completed
probability rows, carrier-kernel reuse, exceptional support, Eldritch delta
states, executable fringe fallback, and existing proper-policy verifier.

## Frozen Qualification

The primary case is `natural-t1-full-four-47d8b909aa88`.

The implementation qualifies its executable-upper work only through either:

1. strict admission of a completed Fossil or Harvest upper Q; or
2. at least 10% deterministic upper-Q reduction of a completed qualifying
   Fossil or Harvest row through one compatible proper executable policy.

The frozen root and qualifying upper-Q values remain those in the
[preceding plan](../archive/2026-07-29-high-impact-executable-uppers/plan.md).
Movement below 10% is measured evidence, not qualification.

An independently sound and measurable live-renewal optimization may be
retained without upper-policy qualification. Product default caps do not
change.

## Phase 1 - Recover And Pin

1. preserve the rejected patch and its hashes;
2. restore the opt-in scheduler/focused-upper candidate without changing
   product defaults;
3. build a diagnostic native runner;
4. reproduce the 200,000-state candidate; and
5. record every explained source or measurement difference before proceeding.

No new scheduling idea replaces this control.

## Phase 2 - Discovered-State Cap Sensitivity

Run the recovered candidate with run-local discovered-state caps:

- 200,000;
- 400,000; and
- 600,000.

Hold fixed:

- `max_reforge_work = 100000000`;
- `max_transitions = 10000000`;
- `max_solver_owned_bytes = 536870912`;
- `max_expanded_states = 25000`;
- `max_state_action_rows = 300000`;
- cooperative stepping and partial snapshots; and
- a bounded native watchdog sufficient for each run.

An 800,000-state run is allowed only when 600,000 remains comfortably below
512 MiB, measured scaling supports it, and it resolves a live trend.

Every cap reports:

- discovered/expanded states and all cap hits;
- scheduled high-impact states and completed state-action rows;
- proper focused-upper passes;
- root and qualifying Fossil/Harvest intervals, absolute movement, and
  percentage movement;
- cumulative root-row probability mass with non-Restart continuations;
- cumulative upper-Q contribution coverage at ranks 32, 128, 512, and 2,048;
- unfinished exits by Restart, Chaos, and other fallback;
- producing action family and per-state discovery fanout;
- rows, transitions, reforge work, kernel evaluation/reuse;
- live/peak solver-owned memory;
- completed-row recomputation count; and
- transition/policy hashes.

Scheduling estimates remain observational and non-certifying.

## Phase 3 - Zero-Goal-Progress Audit

Audit every discovered state with an empty satisfied-goal set. Break it down
by:

- producing action family;
- live-renewable versus goal-dead/irreversible;
- rarity and prefix/suffix occupancy;
- fractured, crafted, metamod, and protection state;
- influence, corruption, and item flags;
- Eldritch tiers/dominance;
- persistent setup/resource state;
- filtered action legality and pool differences;
- whether any remaining non-renewal action observes or exploits explicit junk;
- exact renewal signature; and
- transition/memory fanout.

Measure how many states differ only by explicit modifiers that the next
permitted renewal discards.

The audit is diagnostic until an exact equivalence proof is implemented.

## Live-Renewal Equivalence Contract

A state may enter a canonical live-renewal basin only when it has no goal
progress, remains usable, and exact engine authority proves that discarded
explicit junk is unobservable to every action retained from that basin.

Equivalent states agree on every feature affecting:

- filtered action legality and exact costs/resources;
- exact result distributions and retry/self-loop identity;
- capacity, side legality, pools, weights, and exclusion groups;
- preserved/fractured/crafted modifiers, metamods, and protection;
- influence, corruption, rarity, level, base, and item flags;
- Eldritch state;
- persistent setup/resource state; and
- compilation and fallback behavior.

The preferred proof compares every permitted renewal action by legality,
resource identity, and `exact_reforge_kernel_signature`, then requires that no
retained non-renewal action can observe or exploit the discarded affixes.

Never merge:

- a goal-progress state;
- a live state with a goal-dead state;
- different persistent setup;
- states distinguished by a retained non-renewal action; or
- states merely because both have zero goals.

This optimization is confined to the accepted
`goal_progress_gated_reforges`/goal-relevant product search unless
unrestricted equivalence is independently proved. Unrestricted all-actions
behavior must remain unchanged.

If Harvest Augment, Essence, bench/capacity setup, Fracture, or another
retained action produces a real mechanic ambiguity rather than a code-level
counterexample, stop and ask Oliver instead of guessing.

## Phase 4 - Integrate And Rerun

After implementing only proved live-renewal sharing:

1. rerun 200,000 and compare state/fanout/coverage/work/memory/Q movement;
2. rerun 400,000 and 600,000 as needed for the new scaling curve;
3. continue proper focused-upper improvement after any nonzero movement;
4. retain completed probability rows with
   `completed_rows_recomputed = 0`;
5. install only one compatible proper policy witness at a time; and
6. admit only through strict upper-Q separation.

Continue while measured coverage/movement is increasing, until qualification
or the 512 MiB, 100M-reforge-work, or 10M-transition boundary.

If 600,000 plus the canonical model still yields exactly zero movement, the
conclusion is limited to strict carrier-by-carrier policy materialization.
The report will use contribution coverage to define the next
feature-conditioned/compositional prefix-side and suffix-side upper boundary.

## Native Controls

Focused native cases must prove:

1. discarded-junk live carriers share only the correct renewal representative;
2. same-carrier renewal does not charge `base`;
3. a certified goal-dead carrier uses mandatory Restart;
4. optional economic Restart remains distinct;
5. goal-progress states never enter the basin;
6. relevant fracture, crafted/metamod/protection, influence, corruption, and
   Eldritch differences prevent merging;
7. legality, costs, exact probabilities, retry/self-loop behavior, and
   renewal signatures match an uncollapsed control;
8. a non-renewal observer prevents unsafe canonicalization;
9. completed distributions are not recomputed;
10. repeated hashes are deterministic;
11. properness holds from every published upper state; and
12. unrestricted all-actions behavior is unchanged.

A materially changed compiled strategy requires exact evaluation where
supported and 10,000 simulator runs.

## Acceptance And Closure

Run one appropriate complete native acceptance after all retained source work.
If engine behavior is retained, rebuild release WASM, then run `npm test` and
`npx tsc --noEmit`. Validate both benchmark manifests, tracked JSON evidence,
changed Markdown links, `git diff --check`, and final commit scope. No rendered
or screenshot review is run.

Archive this plan, write a full report and tracked evidence summary, update the
stable solver reference, evidence and archive indexes, and leave HANDOFF with
no active boundary.

This milestone produces exactly two local commits after `19cd3e1`:

1. this active plan/HANDOFF/index boundary plus the recovered candidate patch;
2. final retained implementation or measured rejection, generated artifacts,
   tests, evidence, archive, indexes, and no-active HANDOFF.

Both commits end with `Co-authored-by: Codex <codex@openai.com>`. Nothing is
pushed.
