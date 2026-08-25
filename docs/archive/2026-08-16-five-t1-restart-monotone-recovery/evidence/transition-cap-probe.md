# Transition-Cap Utility Probe

**Status: a cap-only increase is not useful under the unchanged byte budget.**

Oliver authorized raising the transition cap if evidence showed that doing so
would help. On 2026-08-16 the priced five-T1 witness was run natively with
scoped 12,000,000- and 13,000,000-transition fixture probes. The checked
fixture was restored to 10,000,000 afterward. No engine default, release WASM,
or other case cap changed.

| Transition cap | Raw pairs at stop | Evaluator owned | Evaluator peak | Byte-cap headroom | Result |
| ---: | ---: | ---: | ---: | ---: | --- |
| 10,000,000 | 9,998,209 | 935,469,660 | 938,125,764 | 112,856,027 | transition cap |
| 12,000,000 | 11,998,209 | 999,739,860 | 1,002,298,796 | 48,682,995 | transition cap |
| 13,000,000 | 12,998,209 | 1,040,258,204 | 1,042,815,196 | 8,166,595 | transition cap |

All three runs stop during raw pair discovery; none reaches pair refinement.
The compact index itself grows from 75,497,472 bytes to 83,886,080 bytes by
13 million pairs. The published artifact remains the 37,279,857.73995944
Chaos renewal.

The 13-million probe proves raw closure needs more than 13 million transitions,
while only 8.2 MB of the exact evaluator budget remains. The next million in
this capacity band cost about 40.5 MB at peak. A further cap increase therefore
cannot reach closure under the existing byte cap; it would merely change the
reported owner from `max_transitions` to `max_owned_bytes`.

The 10-million default is a configurable safety policy, not a mathematical
limit, but retaining it is correct for this milestone because a larger value
does not buy a certifiable strategy. Recovery still requires a pre-closure
quotient, compact/streamed transition representation, or another architectural
reduction in the raw graph.

Diagnostic reports are
`build/qualification/successor-cap-probe-12m-witness-b.json` and
`build/qualification/successor-cap-probe-13m-witness-b.json`.
