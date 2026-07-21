# Real Three-T1 From-Scratch Diagnostic — Completed Plan

## Objective And Boundaries

Measure an item-level-86 empty rare Dire Pelt targeting T1 rarity, T1 life,
and T1 crafted cold resistance under the existing complete product envelope.
The seeded carrier remained separate. Mechanics, actions, prices, production
caps, and exact state behavior were fixed; approximate compaction was
forbidden.

## Completed Phases

- **D0:** pinned a separate diagnostic manifest and two reproducible cases.
- **D1:** bounded the first expansion at one carrier and recorded its complete
  action/state telemetry.
- **D2:** ran the exact case under unchanged production caps with progress
  output and stopped at the named state cap.
- **D3:** retained raw/concise evidence and promoted the result to the fixture
  guide, evidence index, roadmap, and handoff.

## Result

The first expansion discovered 74,563 states in 582 ms. The production run
stopped in 40.7 seconds at 200,000 discovered and 55,088 expanded states,
leaving 144,912 frontier states. It had consumed 1,152,570 of 1,215,000 rows,
9,304,122 of 11,000,000 reforge work, and 725,411,658 selected-owned bytes.

No constructive state certificate or exact state merge applied. Focused lower
reached `5.3503139241737685`, while the executable upper remained infinite.
Temporary-bench synthesis was the largest measured action-space cost, with
659,762 candidates and 5,133,587 variants.
