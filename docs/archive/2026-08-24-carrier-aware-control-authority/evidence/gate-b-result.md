# Gate B — Corrected Ordering Qualification Stop

**Status: failed; candidate restored (2026-08-24).**

The exact previously measured carrier/action ordering candidate was recovered
from the local Codex session log and rebuilt from source. Its clean five-T1
run reproduced the historical candidate exactly:

- independently exact-evaluated upper `1562083.15196689`;
- 12,848 expanded states;
- 328 nodes / 862 edges; and
- `71364.5019` ms total wall.

This confirms that the earlier 9.25x five-T1 upper improvement was caused by
the ordering candidate rather than an untracked source or build artifact. The
four-of-five workhorse also retained its independently evaluated
`2698.87479601436` upper in `21614.5606` ms.

## Corrected control comparison

| Control | Gate A | Gate B candidate | Decision |
| --- | --- | --- | --- |
| Warlord bounded current semantics | `307.556312036793`, 21 / 45, `29939.5031` ms | same exact-evaluated upper and graph, `28007.9971` ms | pass |
| Non-armour partial-five, 10-second request | proper `6026985788.49406`, 154 / 387, `58808.6924` ms | `watchdog_expired`, no policy or exact evaluation, `127328.9181` ms | **fail** |
| Tri-elemental Bow | `79273.3250308337`, 83 / 239, `43599.1833` ms | same exact-evaluated upper and graph, `40312.8922` ms | pass |

The non-armour control is a hard Gate B failure: it loses compiled properness
and takes about 2.17x its Gate A wall. That exceeds the allowed 5% regression
and cannot be offset by the candidate's five-T1 quality improvement.

The candidate 1,000-expansion fixed-work run retained lower `0.01165`, upper
`59810.9537769745`, graph 51 / 149, and final policy hash
`1b98ca41e69ad1b1`. The ordering was reached: its transition hash changed from
`fb8dc170b29920df` to `a7f9f5677126bcc6`, so the plan's conditional
not-reached hash clause does not apply.

## Stop and restoration

Per Gate B, the carrier/action ordering implementation and both scheduling
hooks were removed. The native tree rebuilds with the Gate 1 shared legacy
ordering only. A post-restore 1,000-expansion run reproduces Gate 1 exactly:

- lower `0.01165`, upper `59810.9537769745`;
- 1,000 expanded states and 51 / 149 compiled graph;
- transition hash `fb8dc170b29920df`; and
- final policy hash `1b98ca41e69ad1b1`.

The corrected controls remain valid authority, but this ordering candidate is
not retained. The predecessor's Gates 3-5 were not entered. No simulation,
release WASM rebuild, web suite, rendered review, or full repository pipeline
was run.

Evidence:

- `gate-b-five-t1-session-recovered.json`
- `gate-b-workhorse.json`
- `gate-b-corrected-controls.json`
- `gate-b-fixed-work.json`
- `gate-b-post-restore-fixed-work.json`

