# Gate 4 — Executable Carrier Planner Qualification and Fallback

**Decision:** the executable carrier/subgoal planner did not qualify. Its
product path was removed under the gate's explicit fallback. Only the reusable,
behavior-neutral carrier and action-effect projections remain.

## Prototype boundary

The prototype searched completed exact sparse rows over a compact carrier
state retaining rarity, exact goal subset, side occupancy/capacity,
`blocked_mask`, junk/crafted/fractured debt, fractured goals and metamods,
protection flags, influence, veiled side, and Eldritch identity. Candidate row
evidence retained action-local preserved, destroyed, and created properties.
The universal-cover predecessor/action/subset chain was used only as an
ordering hint.

No prototype estimate was allowed to act as a lower, prune an action, close an
envelope, or claim exactness. Candidate policies still crossed the existing
strict fixed-policy, compilation, and independent exact-evaluation boundary.

## Rejected qualification

The prototype did find materially cheaper coarse compositions, including
direct multi-goal and preservation-bearing rows. They did not survive the
independent publication boundary:

| Case | Prototype observation | Independent result | Decision |
| --- | --- | --- | --- |
| owner four-to-five | coarse fixed-policy estimate `12365.392875058` | compiled evaluation rejected mass conservation | not admitted |
| clean zero-to-five | coarse reachable-policy estimate `917951.064550363` | no planner incumbent independently certified; later certification work changed the bounded trajectory | not admitted |
| fixed four-of-five | coarse fixed-policy estimate `12197.277488393` | compiled evaluation rejected mass conservation | not admitted |

The clean run eventually published an independently evaluated
`1441094.30889015` policy, but it was a later core-policy result after rejected
candidate certification had enlarged the discovered graph. It is not credited
to the planner and the behavior-changing path is not retained.

The attempted planner therefore failed the required condition that an admitted
planner incumbent compile and independently exact-evaluate. It also could not
claim the required product improvement without changing a control trajectory.
Focused direct-jump, preservation, and no-unnecessary-cleanup observations
remain prototype diagnostics rather than qualification evidence.

## Applied fallback

All planner scheduling, temporary values, universal-cover reconstruction
storage, candidate telemetry, and candidate-certification calls were removed.
The surviving `ExecutableCarrierProjection` and
`ExecutableCarrierActionProjection` are non-convertible structural types only.
They retain the state and action facts needed by a future planner without
participating in scheduling, Bellman values, pruning, policy selection,
compilation, or publication.

Post-fallback native controls reproduce Gate 3 authority:

| Control | Lower | Verified upper | Graph | Wall ms | Max slice ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| fixed four-of-five | 3.47245 | 59,810.9537769745 | 51 / 149 | 41,605.8 | 4,865.8 |
| owner four-to-five | 3.47245 | 2,698.87479601436 | 215 / 563 | 21,945.9 | 5,024.9 |

Both strategies independently exact-evaluated with complete prices, success
probability one, and zero off-policy mass. The fixed case retained its named
1,000-state cap classification; the owner case retained requested bounded
finish. Wall and maximum-slice differences from Gate 3 are runtime variance,
not responsiveness claims.

## Evidence files and cadence

Raw prototype and fallback reports are retained outside tracked documentation
at `build/performance/gate4-carrier-planner-experiments/`:

- `gate4-owner-initial.json`;
- `gate4-owner-cert-boundary.json`;
- `gate4-clean-cert-boundary.json`;
- `gate4-fixed-cert-boundary.json`;
- `gate4-fallback-fixed.json`; and
- `gate4-fallback-owner.json`.

The native engine was rebuilt after the projection fallback. Projection unit
coverage was added, but the routine native suite remains deferred to the final
acceptance pipeline under the plan cadence. No sampled simulation, WASM build,
web suite, rendered review, or full repository pipeline was run at this gate.
