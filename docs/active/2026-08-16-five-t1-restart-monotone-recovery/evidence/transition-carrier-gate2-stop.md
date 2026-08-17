# Transition Carrier Gate 2 Stop

Date: 2026-08-16

## Implemented Representation

Raw evaluator pairs and their discovery hash-chain links now use fixed-size
segmented storage. Stable numeric ids and checked constant-time access are
unchanged, but crossing 16,777,216 entries no longer reallocates either entire
carrier. The pair record retains node, state, checkpoint state, Unveil offer,
row, and the state-dependent material-consumption bit. Operation kind and
action descriptor were removed from the record because both are exact
functions of the retained compiled node. A compile-time assertion fixes the
record at 24 bytes, down from 28.

Full, fast, discovery-index, refinement-conversion, attribution, and failure
diagnostic ledgers account for actual segment capacity and segment-pointer
storage. Pair insertion checks the projected next pair/link segments against
the owned-byte cap before allocation.

The focused segmented-container boundary test, evaluator raw/reference
oracles, native build, evaluator suite (16,852 checks), and solver suite
(96,113 checks) pass with zero failures.

## Frozen Cases

Witness A remains independently evaluated at `624800.951911854`, success one,
zero off-policy mass, 184 nodes / 666 edges, and a 227.24 ms largest native
solve step.

At the checked 10-million-transition cap, Witness B still stops during raw
pair discovery, but evaluator peak falls from Gate 1's `858,541,852` bytes to
`600,881,764`. Its measured retained owners are:

- 9,998,209 raw pairs;
- 24-byte pair records occupying 240,263,168 bytes;
- discovery links occupying 40,050,688 bytes;
- discovery index peak 48,439,296 bytes; and
- row payload 239,382,200 bytes.

This headroom justified one temporary 18-million cap probe. It crossed the old
contiguous-vector cliff without a reallocation spike, proving the segmented
carrier works as intended, but discovery remained open:

- 17,998,209 raw pairs and 17,965,463 stored transitions;
- pair carrier 432,160,768 bytes and links 72,040,448 bytes;
- row payload 431,171,120 bytes;
- evaluator peak 1,026,151,572 bytes against 1,050,981,759; and
- stable stop `exact_eval_pair_discovery_transition_cap` / `max_transitions`.

Only 24,830,187 bytes of peak headroom remain. The temporary fixture setting
was restored to 10 million. Another cap-only increase is not useful, and no
closed-partition class count exists.

## Stop Decision

Gate 2's explicit broader-architecture stop fires. Pair storage is no longer
the owning avoidable cliff. Reaching closure now requires a new independently
planned authority that reduces or streams raw transition targets before the
full graph is retained and also makes the subsequent partition replay fit.
Merely shaving another field from the pair record or increasing a count cap
cannot meet that requirement.

The priced five-T1 strategy therefore remains the independently evaluated
six-node Chaos renewal at `37279857.73995944`; it is not recovered. Parent
successor Gates 4-8 were not resumed. No release WASM, web suite, or full
acceptance pipeline was run.

Artifacts:

- `build/qualification/transition-carrier-gate2-witness-a.json`
- `build/qualification/transition-carrier-gate2-witness-b-10m.json`
- `build/qualification/transition-carrier-gate2-witness-b-18m.json`
