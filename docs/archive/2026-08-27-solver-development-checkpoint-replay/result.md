# Solver Development Checkpoint/Replay — Result

**Status: complete.**

Parent: [Plan](plan.md)

Source checkpoints: `f15f590` (format/API/replay), `952524b` (final cache
ownership), and `5f49b4e` (release-WASM compiler workaround/rebuild).

## Delivered

- A versioned binary checkpoint with magic, byte-order/layout guards, exact
  caller identity, payload length, and checksum.
- Faithful serialization of ordered abstract states, generated planner
  operators, candidate/dependency order, state-local automatic admission,
  sparse rows and transition/choice/variant arenas, automatic evidence,
  Fracture witnesses, and the typed action-envelope ledger.
- Native C ABI save/load entry points and single-case benchmark switches:
  `--save-development-checkpoint` and `--load-development-checkpoint`.
- Required reuse: a loaded checkpoint may not silently rebuild when the start,
  action order, limits, or graph-affecting options differ.
- Refusal of pre-closure saves, focused partial graphs, proof-carrying quotient
  graphs, active row/admission cursors, non-fresh loads, identity mismatch,
  option mismatch, truncation, checksum corruption, and binary-layout mismatch.
- Correct outer-cache ownership across nested certification. This also repairs
  ordinary same-context price-only reuse: an internal verifier can no longer
  replace the requested solve's retained graph.
- Complete cache compatibility for the product action scope. The exact outer
  quotient now carries goal-progress gating, Imprint scope, and economic
  Restart scope rather than spuriously invalidating reuse.

## Cross-Process Control

The `eldritch-annul-exalt-side-intent-forced-winner` case was saved and loaded
in separate native benchmark processes. Both runs converged and matched:

| Measure | Ordinary build | Replayed build |
| --- | ---: | ---: |
| Start/lower/upper/exact evaluated cost | `0.0103` | `0.0103` |
| Transition hash | `0f4bbf35042d2517` | `0f4bbf35042d2517` |
| Policy hash | `cd2bdb9db48843ae` | `cd2bdb9db48843ae` |
| Compiled nodes / edges | 6 / 7 | 6 / 7 |
| Strategy SHA-256 | `E494EA2F...E874A33` | `E494EA2F...E874A33` |
| Exact success / off-policy mass | about 1 / 0 | about 1 / 0 |
| Transition graph reused | no | yes |
| Measured expansion time | 44.69 ms | 0.07 ms |
| Total case wall | 719.12 ms | 1315.29 ms |

The total wall result is intentionally not presented as a speedup: on this
small control, process-local loading/setup and downstream certification cost
more than the graph construction that replay removes. The checkpoint's value
is avoiding expensive coarse construction on large development cases while
leaving the downstream code under test live.

The checkpoint file for this control was 256,995 bytes. It is a disposable
build artifact and is not committed as evidence or accepted as correctness
authority.

## Boundary

This completes the original graph-replay debt item at the smallest faithful
high-value seam. It does not snapshot an in-progress strict partition. Such a
format would additionally own the persistent exact oracle, partition and
dependency generations, proof obligations, row kernels, resumable cursors,
and verified incumbent. That is a distinct extension, not hidden unfinished
work in this coarse-graph format.

## Acceptance

Passed:

- fresh release native build;
- solver API checkpoint/refusal gate: 2,686 checks, zero failures;
- solve suite: 86,220 checks, zero failures;
- quotient-proof suite: 616 checks, zero failures;
- policy-refinement suite: 2,083 checks, zero failures;
- separate-process dynamic-action parity with byte-identical compiled JSON;
- release WASM rebuild, with the coroutine-only translation unit isolated at
  `-O1` around an LLVM 23 optimizer crash while hot kernels remain O3/LTO;
- the complete repository pipeline: ingest/economy/data/artifact/bindings,
  3,417,290 native checks, benchmark specification validation, 28/28 release-
  WASM smoke controls, and nonvisual web tests;
- `npx tsc --noEmit`;
- 1,759 local Markdown targets with zero missing and all 62 current documents
  reachable from the documentation map; and
- `git diff --check`.

The release-WASM export audit confirms that neither checkpoint function is
exported. No rendered browser review or broad hard-case benchmark matrix was
run or claimed.
