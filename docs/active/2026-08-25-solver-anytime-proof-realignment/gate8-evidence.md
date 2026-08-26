# Gate 8 — Cooperative Ownership, Responsiveness, And Acceptance

**Status:** implementation and responsiveness qualification passed on
2026-08-25 at `aa34a7b`; final acceptance is pending the plan's single final
run. No mechanic, action scope, proof boundary, cap, or public ABI changed.

## Measured owners

Profiling separated setup, expansion preparation, transition calculation,
optimization, publication/extraction, compilation, exact evaluation,
diagnostic overhead, memory, and cooperative step latency. Before Gate 8, the
clean owner spent about 73.6 seconds natively with a 4.416-second maximum
solve step; the fractured four-to-five owner spent about 21.76 seconds with a
4.854-second maximum. Automatic admission/preparation and transition work
were the largest cumulative owners. The long individual slice was not a
reforge row: it was the first lazy `completion_proof_lower(0)` call compiling
the universal/clean goal-cover model during a focused continuation.

The ordinary release-WASM one-mod control had a 238.1 ms maximum worker slice
at Gate 7. The owner hard case also exposed multi-second native/WASM steps, so
finalization-only batching could not explain the responsiveness debt.

The final matrix then exposed a separate Bow owner. The solver published the
same bounded direct policy, but release WASM spent 41.5 seconds independently
replaying its exact graph after a 42.5-second solve. Native attribution showed
that 10.8 of roughly 12.4 exact-refinement seconds rescanned modifier members
to recover observation facts already proved uniform by the strict layout.
This was evaluator debt, not a weaker policy or a watchdog-profile problem.

## Retained implementation

- `SolveScheduler` now names the typed lane/fairness owner.
- `PublicationPipeline` owns direct assertion, strict repair,
  classification, packaging, progress, result transfer, and the retained
  cooperative task. Compatibility references keep the established coroutine
  behavior while consolidating lifetime ownership.
- `ActionEnvelopeLedger`, `IncumbentPortfolio`, `ProofPatternManager`, and
  `SolveTelemetrySnapshot` retain their Gate 1, Gate 2, Gate 5, and Gate 0
  authorities. None is convertible into another authority.
- The public step request is an upper bound with an internal 32-logical-unit
  ceiling. Dynamic automatic preparation, state-local automatic admission,
  focused proof preparation, post-upper proof/classification, focused policy
  work, and publication return at retained continuation boundaries.
- Focused proof initialization retains prior proof-only values separately
  from restricted-policy values. Newly interned support computes its full
  admissible proof one state per continuation. Post-upper row classification
  likewise processes one row per continuation.
- High-impact anytime mode builds the immutable universal/clean goal-cover
  model during measured solve setup, before the first public step. This moves
  its one-time cost out of a host-visible continuation without changing the
  produced proof values. Default solves retain lazy construction so a root-row
  cap reached before proof work keeps its established attribution. A setup cap
  in high-impact mode crosses the ordinary stepped refusal/publication path.
- Automatic-admission telemetry now reports continuation resumes,
  suspensions, and maximum slice; the new retained vectors are included in
  both fast and audited owned-memory estimates.
- Direct compiled-policy assertion yields before exact evaluation, and exact
  evaluation derives only the compiled Fossil descriptors it consumes rather
  than repeating whole-registry validation already owned by solve setup.
- Nested final-graph assertion, carrier-bound attribution, portfolio
  verification, and publication hashes now resume through the retained
  publication task. Final cap reconciliation uses the already-maintained fast
  ledger and performs the full audit once at publication.
- Strict goal-member and junk classes retain the Veiled role, metamod role,
  observed required level, classification-tag bits, exclusion signature, and
  count-membership facts proved uniform by layout construction. Exact
  observation extraction consumes those facts only when feature-specific
  completeness is present; coarse, hand-built, or incomplete classes keep the
  original collision-safe member scan.
- Worker stepping gives incoming messages an immediate timer-task turn and at
  least one every 100 ms of native work, using low-latency MessageChannel
  yields between them. The native and release-WASM bounded-finish clocks now
  both include synchronous solve setup.
- Two measured quadratic scans were retired: automatic carrier ordering now
  uses membership identity, and upper-pass temporary-row promotion restores
  rows before one stable promotion pass.

## Responsiveness qualification

The final proof-preserving native owner report is
`build/performance/native-solver-gate8-cooperative-proof-preserving-final-case-conquest-lamellar-allflame-partial-five-last-mile-current-v1.json`.
It records:

| Measurement | Result |
| --- | ---: |
| Total case wall | 20,824.30 ms |
| Measured solve setup | 4,328.7781 ms |
| Maximum host-visible solve step | 301.4132 ms |
| Certified lower | 36.4286171891044 |
| Verified/exact-evaluated upper | 2698.87479601436 |
| Compiled graph | 215 / 563 |
| Expectation / exact evaluation | pass / matched |

The former 4.4–4.9 second solve step is absent. The remaining roughly 301 ms
maximum belongs to one publication continuation, not an unbounded batch, and
the hard case contains no multi-second uncancellable native solve step.

The final ordinary worker report is
`build/performance/wasm-worker-solver-gate8-proof-preserving-final-case-oracle-real-one-mod-v1.json`.
It reproduces exact value `23.79`, graph 6 / 7, and all case expectations with
a 21.983 ms maximum release-WASM worker slice. This passes the unchanged 50 ms
qualification and improves the Gate 7 238.1 ms baseline without weakening the
cap.

The release module was rebuilt after the final engine change. Raw profiles and
temporary attribution reports remain under ignored `build/performance/` paths;
no temporary timing probes remain in source.

The unchanged non-armour release-WASM report is
`build/performance/wasm-worker-solver-gate8-final-strict-observation-metadata-case-spine-bow-partial-five-current-10s-v1.json`.
It records:

| Measurement | Result |
| --- | ---: |
| Total case wall | 38,608.754 ms |
| Solve wall | 26,750.404 ms |
| Independent exact replay | 8,927.030 ms |
| Maximum host-visible solve step | 1,345.613 ms |
| Verified / independently reconciled upper | 13,143,533,994.4086 |
| Watchdog | unchanged 60 s / not expired |
| Expectation / exact evaluation | pass / matched |

The corresponding final native diagnostic completed in 24,960.07 ms, down
from roughly 43 seconds before strict-metadata reuse. The release result keeps
the same three-state bounded policy and exact cost; the improvement changes no
observation signature, quotient, or publication proof.

## Final acceptance

Pending. The complete native, release-WASM, nonvisual web, TypeScript,
10,000-run verification, and one full repository pipeline will be recorded
here after the plan's final acceptance gate. Oliver-owned rendered Calculator
review will remain explicitly unclaimed.
