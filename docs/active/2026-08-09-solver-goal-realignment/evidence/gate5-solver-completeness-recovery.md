# Gate 5 solver-completeness recovery

Date: 2026-08-09

This evidence records focused native runs only. The killed partial runs below
are diagnostic history, not acceptance evidence. The final Warlord control and
primary case both close exactly, compile, independently evaluate, and complete
10,000 native Simulator runs. The runs assign each obstruction to the resource
or lifecycle that actually owns it before recording the final Gate 5 result.

## Retained graph versus automatic-admission work

The first repaired primary rerun used benchmark executable SHA-256
`53196b249b070ad5fcfb0d7b355962931408358fcc6e9bc51cefe9338f3088a4`.
Its external 300-second watchdog expired with no final report or survivor. The
last durable partial had expanded all 3,621 parent states while retaining only
35,866 state/action rows and 52,758 transitions, far below the declared
retained-graph caps. It had completed 153 solver calls; p95 was 4.090 seconds
and the largest returned call was 12.774 seconds. The next call did not return
before the watchdog. This disproved the earlier `max_transitions` diagnosis:
the cap had combined retained graph storage with transient child work.

The repair now keeps retained parent rows, transitions, discovered states, and
reforge work separate from observational automatic-admission counters.
Incomplete automatic batches are transactional and never cached as complete.
The carrier-local admission coroutine has deterministic resume/no-replay,
cancellation rollback, and owned-byte accounting; its initial focused control
used six resumes and five suspensions with a 0.210 ms maximum measured leaf.

Primary artifacts:

- ledger SHA-256:
  `75949b4a54577132d54ea51fee1a990a9fd892014a16fae435e6e653f4f3b9b7`;
- final partial SHA-256:
  `49a336502db3cab7750abfb1d1fbde06e007f1067de038827426f8da7c5d5e8a`.

The killed partial has no final `solver_telemetry`, so it cannot support an
exact completed/open-obligation count. The priced product scope contains 24
primitive actions. Eleven are delayed static operators per eligible non-goal
carrier: nine Fossils and two Harvest Reforges. There are no retained Essences
for this goal. If every discovered state were a non-goal carrier, the static
ceiling would be 39,831 pairs; the real count is lower and remains unmeasured.
The current scheduler is carrier-major and dynamic-first, so one carrier
finishes automatic preparation, its dynamic rows, and then those eleven static
rows before advancing. Cross-carrier fairness is therefore a likely next
runtime owner after the measured Imprint leaf, but it is not yet the cause of a
captured stop. A focused fairness repair is warranted only if the next complete
Warlord/primary telemetry confirms carrier starvation or repeated preparation.

## Warlord control and honest next boundaries

After separating child discovered states, the Warlord-only product control
retained 698 parent states while reporting 644,462 transient automatic child
states. Its old next stop was the shared `max_reforge_work` boundary. Automatic
child reforge work is now separate as well, and a focused resource-stop control
proves that a start-reachable proper policy can be independently certified and
published while the action envelope remains open. That control publishes a
74.625-chaos Transmute/Restart upper with lower bound zero and an authoritative
`cheapest_independently_evaluated_selected` signal.

The post-split Warlord rerun used benchmark executable SHA-256
`b241fd29857cc8133274f581696f4aed9725f1a3eac8a78dccffb5ddddd9c160`.
The 60.151-second external watchdog expired with no final report or survivor.
Its last returned sample was at 1.046 seconds: 102 solver calls, 698 discovered
and expanded parent states, 4,825 rows, 3,701 transitions, 2,555,093 retained
reforge-work units, and no incumbent. A build-local probe changed only
`solve_step_work_items` from 1,024 to 1; it returned 20,030 calls in 0.997
seconds (18.684 ms maximum) and then reproduced the same unreturned leaf. This
rules out aggregate work-item batching.

Warlord artifacts:

- rerun ledger SHA-256:
  `3fdad83411fffb3cea4feffe7d901ef50864f1447334ee91117a4ddc38117c85`;
- rerun partial SHA-256:
  `9f6407b51506fba7ff5d54bf582fdf38f66fa5e2691210e7fbdbfbbe27afca1d`;
- one-item probe partial SHA-256:
  `04789aa95a9799e12f53360c109e2b380fbf99ebb0e3320d92d295cad0e789d0`.

## Measured leaf ownership

A non-destructive GDB attach to the released one-item probe captured the live
main-thread stack inside `discover_automatic_imprint_options`, called by the
state-local automatic-admission coroutine. It was not in Bellman initial
selection. The coroutine's next checkpoint occurs only after the whole Imprint
program search. That search caps program depth/count at 3/256, but each program
synchronously executes every step and active successor; a destructive reforge
then walks its frontier, buckets, and terminal/nonterminal edges without an
inner yield.

The old capped same-case run independently supports this attribution: policy
optimization completed in 290.428 ms with 369 policy-evaluation calls and a
largest component of two, then 47,425,929 of its final 49,999,966 logical-work
units were charged to nested automatic children. Removing that false child cap
exposed the same work as elapsed time.

The Imprint discovery and attempt traversal is now a cooperative continuation.
It checkpoints before and after each atomic `calc.outcomes` call and during
large merge/state scans, publishes only a complete transaction, and preserves
rollback/retry determinism. A focused finite Augment/Regal grammar also proves
that a goal-capable prefix remains extensible: Augment followed by Regal is not
discarded merely because Augment can already reach a goal. The focused gate
passes 518 checks. It measured 82 resumes, a 0.072 ms maximum returned slice,
and a 0.004 ms maximum atomic outcome call in the synthetic control.

Product-parent exchangeable-junk compression was tested independently for the
automatic local layout and deliberately not enabled: strict modifier-exclusion
signatures kept the candidate families distinct, so the proposed inheritance
produced no work reduction. No compression benefit is claimed.

## Completed Warlord control after the Imprint cursor

The rebuilt benchmark executable has SHA-256
`03dcfae771cf1bf9a738e9ecdea97f1a16f65339dbaf53fe751b1be37fd73802`.
The isolated Warlord run returned normally in 29.308 seconds with no watchdog
expiry or survivor. The solve itself used 27.962 seconds and 4,610 cooperative
steps; its largest returned step was 967.412 ms. The retained graph remained
small at 698 expanded parent states, 5,081 rows, and 3,719 transitions. Native
live/peak owned memory was 348,849,605/404,552,623 bytes.

The run published a start-reachable bounded policy at
224.12385889724871 chaos. Its lower bound remains zero because the action
envelope is open. The canonical fallback portfolio reports one successful
publication and the unsampled
`cheapest_independently_evaluated_selected = true` authority. Compilation
produced nine nodes and fourteen edges. The compiled contract found exactly one
`influence_exalt` node with `influence = warlord`. Independent exact evaluation
matched the published cost bit-for-bit, with success probability one and zero
off-policy mass.

The run nevertheless and correctly returned `refused_resource_cap`, so its
strict expectation failed. Its only reported cap is
`max_imprint_program_depth`. Of 262 Imprint carrier decisions, 261 were
rejected normally and one remained deferred. Automatic admission observed
644,462 transient child states, 40,402 rows, 71,878,081 transitions, and
100,546,636 logical reforge-work units. Cooperative Imprint discovery evaluated
137 programs, pruned 493, evaluated 5,324 action/state pairs, and merged
143,586,670 outcomes; its largest real-case atomic outcome call was 960.771 ms.
The unresolved incremental envelope contains 3,148 actions: 966 unevaluated
and 2,182 unresolved. This is an honest exact-closure refusal, not a named
retained graph, memory, parent reforge, or watchdog failure.

Latest Warlord artifacts:

- ledger SHA-256:
  `b7f0896ab04ed31e02222edad2cd1461ed3bacee486577967f92850902af81a8`;
- report SHA-256:
  `cb6a39f06d8590c1a659be02cb8a759b2b3ae935568889dbdee2513e879ca8df`;
- compiled strategy SHA-256:
  `3ad95b13cbffeb7dbd030568af33ddee934ebfca81cc3eb491d31c7d7bfa61fd`;
- final partial SHA-256:
  `61ba1df12ebab5439afbf356467e18a1a6609d92e597463c31284a03a8b32a20`.

The next boundary is to replace the arbitrary depth-limited Imprint search
with a finite, mechanically complete supported-program grammar or an equivalent
proof of exact closure. Raising the depth cap or suppressing the deferred
candidate would be unsound. The primary remains queued until the Warlord
control closes this boundary.

## Price-bounded finite Imprint-program closure

The interrupted follow-up implemented both exact distribution/resource
dominance and a carrier-local price certificate. The soundness audit confirmed
the following authorities:

- hashes only shortlist exact terminal entry-vector equality; componentwise
  primitive-action multiplicities then prove that the retained representative
  consumes no more of any resource under the engine's nonnegative economy
  contract;
- the price certificate accepts only a finite carrier-local upper from a
  certified incremental route, a proper independently evaluated incumbent, or
  the focused fallback/restart route at the actual carrier involved; it never
  reuses a start-state upper as a global carrier upper;
- checkpoint and primitive prices must be finite and nonnegative, every grammar
  step must be strictly positive, mandatory sums are rounded downward, and the
  integer maximum useful depth is rounded outward before pruning;
- carrier influence is included in conservative producer reachability, while
  static reach masks supply suffix-cost lower bounds only. Exact terminal
  outcomes decide goal exits, so a nonproducing final step cannot erase goal
  progress made by an earlier prefix; and
- missing, nonfinite, negative, or zero-step prices, an absent certified upper,
  and `max_imprint_program_work` do not claim closure. The legacy depth boundary
  remains an honest refusal whenever the price proof is unavailable.

The original build interruption in `solver_options_automatic.cpp` was repaired
by moving the coroutine suspension outside its exception handler while
preserving rollback and deferred publication. Native production and test
targets then built successfully. The final focused runs were:

- Solve: 98,783 checks, zero failures;
- dedicated Imprint compile/evaluate: 60 checks, zero failures. The depth-one
  control honestly refused `max_imprint_program_depth`, while the mechanically
  closed depth-three fixture compiled and exactly evaluated the selected
  Imprint route; and
- final rebuilt benchmark executable SHA-256:
  `e5f04b031285a79f8e4455d21bfcbad4d37849cafc2e70f84c4c4ceac1eec501`.

## Warlord after the price proof

The isolated control used benchmark executable SHA-256
`e0df154b9f4044250143aed854abbe3c823a5df59119f9b3d590d7c12a1bb3ff`.
Its 60.354-second watchdog expired, the worker was terminated cleanly, and no
survivor remained. There is no final report, compiled strategy, solve summary,
or final solver telemetry, so this run is diagnostic evidence rather than an
acceptance result.

The durable partial contains 8,943 returned cooperative solve steps. Median,
p95, and maximum returned step times were 5.628 ms, 7.901 ms, and 1,051.099 ms,
respectively. The final sample at 58.946 seconds remained in `Iterating`, round
zero, with zero completed sweeps, no incumbent, and no finite upper. Expansion
had already exhausted the 698-state frontier. Retained work remained 5,081
rows, 3,719 transitions, and 2,555,093 reforge-work units; the solver live-byte
estimate reached 414,639,888 bytes, below the selected 1 GiB boundary.

This result is narrower than the former arbitrary-depth refusal. The
cooperative scheduler is returning normally and retained graph caps are not the
owner. Warlord never installs the finite carrier-local incumbent required by
the price certificate, so price-bounded closure is correctly dormant; the
no-incumbent Imprint fallback does not finish the first Bellman sweep before the
watchdog. The next Gate 5 work is to characterize and close that fallback or
bootstrap an independently certified carrier-local upper without weakening
closure. The mixed-action seven-step follow-up and primary case remain queued.

Post-proof Warlord artifacts:

- ledger SHA-256:
  `540c4c032d7584375d9bc977dfd7f6a8fabcf70aff23c61bac4ca880a3f0ef53`;
- final partial SHA-256:
  `13175fdf8e6babe326d0f57e52f340e4a0d862c71bc21b110754fc69dafef744`.

## Final exact closure

The no-incumbent boundary was closed without weakening the Imprint proof.
Automatic admission now installs a finite carrier-local renewal-plus-finisher
upper before price-bounded program enumeration. The enumerator checks extension
legality before charging program work, caches child-context and action-diagnostic
owned-byte accounting, and retains exact resource-vector dominance. A focused
mixed-action seven-step program proves that the finite grammar can extend beyond
the former depth boundary and that a renewal/finisher certificate closes it.

The final Warlord control is at
`build/solver-goal-realignment/gate5/warlord-final-10000`. It used benchmark
executable SHA-256
`24495734c0f54368a38bb1323129fcbc91ae53c424640be744e26019c67ba043`
and returned exact lower/upper equality at `224.12385889724871` chaos. The
compiled policy independently evaluated to the same value and completed 10,000
of 10,000 Simulator runs with zero action-limit, graph-step-limit, or off-policy
failures. Total wall time was 2.778 seconds, including 2.179 seconds of solve
time. Its artifacts are:

- ledger SHA-256:
  `8b36164b5c1c5b702aa15d1085932090eb6df4d60ccd0b2a0f2273476666df5a`;
- report SHA-256:
  `430322c053ecd197f74cf66f1962807c9efa6eac58c1481a72da05564f0347b6`;
  and
- compiled strategy SHA-256:
  `234feb0b2dbc7145fd5be766722cdebc395920749c7509aa47dc10334aec331b`.

The primary exposed two further proof-boundary defects. First, the coarse
product Fracture abstraction could not safely retain physical acceptable hits
while quotienting all misses back to a fresh product state. A shared product
Fracture kernel now makes that quotient explicit in discovery, strict lifting,
compilation, and evaluation. Second, a complete audited strict alternative
action envelope was not being promoted to global lower-bound authority. Exact
refinement now closes that certificate only when every alternative obligation
is resolved; the finish path consumes that certificate instead of requiring a
stale coarse value to equal the strict value.

The final primary result is at
`build/solver-goal-realignment/gate5/primary-final-10000-v2`. The 1,207-state
coarse solve and 5,820-state strict lift closed exactly at
`3745.7309340083884` chaos. The coarse value
`3759.5969190423507` is deliberately recorded as unreconciled; it supplies no
strict lower-bound authority. The strict action-envelope certificate instead
reports zero unresolved obligations and closes the global lower bound. The
compiled policy contains 1,813 nodes and 3,832 edges, independently evaluates
to the exact solve value, and has success probability one and zero off-policy
mass.

The first 10,000-run primary verification had six graph-step-limit failures,
with zero action-limit or off-policy failures. This was a Simulator harness
limit rather than a policy defect. The benchmark contract now exposes the
engine's existing `max_graph_steps_per_run` option in both native and WASM
drivers, and the primary fixture pins the engine maximum of 4,000,000 while
retaining its 100,000-action cap. The final rerun completed 10,000 of 10,000
runs with no failures; its sampled mean cost was `3738.9538811199136` chaos.
Total wall time was 139.631 seconds, including 0.625 seconds for the coarse
solve and 6.115 seconds for independent exact evaluation. Peak evaluator-owned
memory was 843,290,832 bytes.

Final primary artifacts:

- ledger SHA-256:
  `422b70401ebf66ad3d1f2e9eff128146e2702952a68f37af4149026da80f6357`;
- report SHA-256:
  `912ca313893a18ca176dece878d2674a9da9169d01ceae5654c90cf25dc44b6c`;
  and
- compiled strategy SHA-256:
  `908a617ca0f704b39d644a09e71c88d8f6e5724eaea6178a27ac17f87041325c`.

The final focused native qualification was green: Solve 98,796 checks,
dedicated Imprint 60, automatic Eldritch 545, quotient proof 357, and compiler
804, all with zero failures. The native build, TypeScript no-emit check, and
diff whitespace check also pass. Gate 5 is closed. The full acceptance pipeline
is intentionally deferred to Gate 8.
