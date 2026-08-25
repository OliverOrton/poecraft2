# Transition Carrier Gate 1 Decision

Date: 2026-08-16

Gate 0's 24-byte raw transition carrier passed the frozen Witness A control.
The independently evaluated published product remains proper at
`624800.951911854`, with success probability one and zero off-policy mass.

The priced five-T1 witness was then run at its checked 10-million-transition
cap. It still stopped in `pair_discovery`, but the compact carrier reduced the
evaluator peak to `858,541,852` bytes. At the stop it retained `9,998,209` raw
pairs, `9,974,258` stored transitions, and `239,382,200` row-payload bytes.
The transition-via sidecar remained empty, as required before contraction.

That measured headroom justified one temporary 16-million cap probe. It also
stopped in `pair_discovery`, at `15,998,209` raw pairs and `15,994,538` stored
transitions. Evaluator-owned bytes were `1,008,800,700`, peak bytes were
`1,011,645,812`, and the evaluator budget was `1,050,981,759`. The row payload
alone was `383,868,920` bytes. No quotient evidence exists because refinement
was not reached.

The 16-million setting was a measurement only and was restored to the checked
10-million value. Another cap-only increase is not useful: discovery remains
open, only about 39.3 MB of peak headroom remains, and the contiguous pair and
pair-link vectors are at their next capacity-doubling boundary. Gate 2 owns a
compact segmented pair carrier; it must eliminate that allocation cliff and
remove only pair metadata that is provably derivable from the compiled node.
It may not weaken raw identity, probability precision, or the byte cap.

Artifacts:

- `build/qualification/transition-carrier-gate1-witness-a.json`
- `build/qualification/transition-carrier-gate1-witness-b-10m.json`
- `build/qualification/transition-carrier-gate1-witness-b-16m.json`

No WASM build, web suite, or full acceptance pipeline was run.
