# Gate −1 Documentation And Authority Preflight

**Status: complete on 2026-08-25.** This was a documentation-only source audit.
It does not activate the proposed solver plan or authorize implementation.

Parent: [Solver Anytime Planning, Proof Patterns, And Debt Retirement](plan.md)

Source checkpoint inspected: `a1449fa` (`Record Imprint scope acceptance`)

Documentation checkpoints already present when the audit began:

- `0238753` — proposed the successor plan;
- `f2bee5b` — archived every completed/stopped active boundary and rebuilt the
  active/archive indexes.

## Scope And Method

The audit compared current stable references and the proposed plan with the
present source owners for:

- exact terminal recognition;
- Calculator, benchmark, WASM, and low-level solve options;
- incremental action-envelope and public-lower authority;
- carrier ordering versus admissible proof types;
- verified executable publication;
- solve phases, source ownership, and cross-layer rebuild boundaries; and
- current versus historical documentation lifecycle.

Inspected stable references included the documentation map, HANDOFF,
foundation/solver internals, change impact, engine, solver, end-to-end flow,
Calculator, mechanics index, decisions, evidence, and solver roadmap.
Inspected source included `solver_calc.cpp`, `solver_solve_types.hpp`,
`solver_solve.cpp`, `solver_solve_incremental.cpp`,
`solver_solve_constructive.cpp`, `solver_solve_bounds.cpp`,
`solver_solve_priority.cpp`, `solver_solve_finish.cpp`,
`solver_solve_telemetry.cpp`, the action-family and public solver contracts,
the WASM facade, and Calculator solve request construction.

No native build, WASM rebuild, test suite, benchmark, simulation, rendered
review, or external mechanic research was run. Historical measurements were
checked only for authority/provenance consistency, not reproduced.

## Source-Confirmed Current Contract

| Boundary | Current authority | Consequence for the proposed plan |
| --- | --- | --- |
| Exact success | `CalcContext::is_goal_state` requires requested rarity, slot threshold, and `explicit_affixes == satisfied` | No gate may restore junk-tolerant success. Junk is intermediate carrier debt only. |
| Product scope | Calculator requests goal-progress gating, disables economic Restart by default, and uses the user-visible Imprint control whose default is off | Gate 0 fixtures must record all three fields. Fracture replacement remains mechanic-owned and separate from voluntary abandonment. |
| Compatibility scope | Low-level native/WASM omission defaults retain Imprint enabled when the option is absent | General product/benchmark fixtures must set Imprint explicitly; dedicated controls opt in. |
| Public lower | `certified_global_lower_bound()` publishes the independent goal-cover/pattern floor while the incremental envelope is open; the restricted focused value remains diagnostic | Gates 5–7 must strengthen global patterns or close obligations. Reordering a restricted search cannot raise the certified public lower by itself. |
| Proof versus ordering | `ProofLowerValue` and `CarrierOrderingScore` are non-convertible, and current carrier sorting lives in `solver_solve_priority.cpp` | Gates 3–4 may construct/schedule uppers only. Gates 5–7 own separate admissibility and pruning proof. |
| Finite upper | Publication candidates compete only after emitted JSON independently exact-evaluates as proper, fully priced, zero-off-policy, and finite | Gate 2's portfolio must preserve the cheapest verified artifact across later coarse/direct/strict candidates. |
| Current scheduling | Focused and incremental legacy bucket orders differ; the delayed-family branch still has identical `high_impact_executable_uppers` ternary arms and manual family waves | Gate 3's cooperative scheduler/debt work is grounded in current source, not only historical notes. |
| Final phases | Native stepped Solve owns expanding, iterating, refining, compiling, and certifying before `Done`; finish is result transfer | Gate 8 responsiveness must cover finalization and cannot attribute long native slices only to expansion. |

## Documentation Findings And Corrections

1. **Top-level execution history obscured current authority.**
   `docs/README.md` had grown to 589 lines, mostly completed/stopped milestone
   narratives already owned by the archive. It is now a concise authority map,
   current checkpoint, lifecycle policy, and repository entry-point page.

2. **HANDOFF retained multiple obsolete boundaries.**
   It is now limited to the current source/docs checkpoints, current product
   and proof contract, retained five-T1 result, open WASM qualification, and
   the one possible next action. Full history remains linked rather than
   duplicated.

3. **Solver source maps lagged the implementation split.**
   Foundation, engine, solver, and flow references now name incremental action
   generation, bounds, priority, audit, sparse policy, action-family contract,
   and split exact-evaluation owners. They explicitly separate the open-
   envelope restricted value from public lower authority.

4. **The stable solver/flow references mixed current contracts with old
   qualification stamps.**
   A bounded 2026-08-25 source-audit stamp now identifies which current
   contracts were checked. Older performance numbers and acceptance addenda
   remain historical and are not represented as rerun evidence.

5. **The mechanics index stamp was too broad and old.**
   The current audit confirms the complete 26-value primitive vocabulary,
   native enum parity, WASM/simulator names, synthetic operations, fixed-option
   names, and open-ruling routing. Family transition pages keep their own
   stamps. No mechanic behavior changed and no new Oliver ruling is required.

6. **The roadmap called a 2026-07 record current.**
   Its current section now points to the proposed plan and current source
   contract. The long retained lineage is explicitly a historical closed-
   premise record, not executable sequence.

7. **Small index defects remained.**
   The evidence index's “Active” five-T1 heading is now historical, one
   duplicated word was removed, and two malformed decision-heading dashes were
   repaired.

Archived milestone documents were not rewritten. Their dates, failures,
measurements, and historical “next” language remain point-in-time evidence.

## Plan Check

The proposed gates still align with current source after the audit:

- Gate 0 owns current-semantic benchmark and trajectory authority.
- Gate 1 owns complete action-family/obligation coverage.
- Gate 2 owns monotone verified incumbent truth.
- Gate 3 replaces manual scheduling only after fairness/control coverage.
- Gate 4 is executable-upper planning and does not receive proof authority.
- Gate 5 first reproduces present proof values behind typed ownership.
- Gate 6 strengthens independent carrier/action proof patterns.
- Gate 7 requires a measured pruning/work/envelope consumer.
- Gate 8 owns cooperative finalization, release-WASM responsiveness, and
  established-authority refactoring.

No gate needs resequencing from this audit. Gate −1 is complete; if Oliver
activates the plan, implementation begins at Gate 0.

## Remaining Documentation Constraints

- `docs/solver/README.md` and `docs/evidence.md` remain over the 500-line review
  trigger. They are cohesive but history-heavy. The plan may split them only
  along established current-reference versus measured-evidence ownership; a
  cosmetic split should not interrupt Gate 0.
- Mechanic family pages retain their individual verification dates. Recheck a
  family when implementation changes its action contract or a Gate 1 forced-
  winner witness exposes a concrete contradiction. Do not preemptively infer
  mechanic changes.
- The full repository pipeline and rendered review remain unclaimed.

## Gate −1 Result

**Pass.** Current source, proposed plan, and stable cross-layer authority agree
on the contracts that constrain implementation. Documentation contradictions
that could misdirect the plan were corrected without changing code, derived
artifacts, action scope, mechanics, caps, prices, or acceptance evidence.
