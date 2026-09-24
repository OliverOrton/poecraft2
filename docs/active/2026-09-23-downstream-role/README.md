# Downstream role computation investigation — 2026-09-23

## Disposition

Oliver selected execution of the attached [D0–D4 packet](research-inputs/downstream-role-plan-9b1fb8e/README.md). The ZIP SHA-256 was
`bd2270ff6d4217922dc3df9db5c9a313a6ab8593fbdf22f40b9241c9fb596804`;
its 14 payloads and manifest were checked by path, byte count and SHA-256 before
verbatim import. The reviewed source base was `9b1fb8ee36a41eab84ea9c6501d5e0266d209ab7`.
The packet is research input, not authority over native mechanics or published
policy.

**D1 stopped at the native reuse gate.** Full-duration A4 has abundant coarse
role-pattern recurrence, but the three measured downstream operations did not
provide an expensive repeated structure with a pre-work native guard and a
plausible all-in saving of 5% of the 240-second solve. The largest late phase is
strict carrier discovery, yet its actual selected-row cold builds total only
0.47 seconds. The remaining phase time cannot be credited to a role template.
No D2 source pilot or D3 reuse qualification was justified. Two temporary
diagnostic runs and all diagnostic C++ changes were removed from the runtime;
there is no retained solver, ABI, WASM, web, scope, cap, lower, exact or policy
change. The broader heterogeneous continuation hypothesis remains open.

## D0 — actual workload and timing domains

The [six-case corpus](../2026-09-23-role-parametric/corpus/manifest.json) fixes
the A4 Conquest Lamellar empty level-86 Rare root, four exact goal slots, the
current Allflame prices, goal-relevant action envelope, eight solve-step work
items, native retention `reuse`, and generated Imprint/economic Restart off.
The existing ordinary C3 A4 report at
`out/ordinary-capability/C3-fracture-native/cases/cb02-cross-base-product8-long240.json`
has the independently evaluated C3746.1319409485764 controller, 7,213
expanded states and 135,519 rows. Its 240-second Finish is bounded feasible,
not exact closure. The prior R2 pair-stop and R3 same-entry memo results keep
their [own scopes](../2026-09-23-role-parametric/README.md). C3's saved source
ledger includes an unrelated dirty pilot, so its timing is historical context,
not a clean causal control for this diagnostic.

Two additional A4 diagnostic runs used the same 240-second requested Finish,
native watchdog 300 seconds, host cleanup 315 seconds, 1 GiB aggregate memory,
inherited 50M logical-work and other caps. Each returned the same evaluated
C3746.1319409485764, 7,213 expanded states, 135,519 rows and 811-node/
2,200-edge graph. They measured native work while preserving the original
action scope; their ignored local reports are
`out/downstream-role/D1/a4-native-census/cases/cb02-cross-base-product8-long240.json`
and `out/downstream-role/D1/a4b-native-census/cases/cb02-cross-base-product8-long240.json`.
The instrumented binaries have different hashes and overhead, so elapsed
differences between A and B are not treatment gains. Raw per-work rows and
their hashes are indexed in [compact evidence](evidence/d1-native-census.json).

| Owner, run A | Measured time | Interpretation |
|---|---:|---|
| Automatic admission within expansion preparation | 44.878 s | Inclusive parent; protected child time is inside it. |
| Protected-side kernel evaluation across completed carrier batches | 18.502 s | Child subset, not additive to automatic admission. |
| Final publication/extraction | 79.048 s | Inclusive of strict lift and independent checks. |
| Final sparse row selection | 0.020 s | 135,519 row evaluations; cannot explain finalization cost. |
| Strict lift | 70.624 s | Inside publication; includes 65.873 s labelled carrier discovery. |
| Direct certification | 7.333 s | Inside publication; separately evaluates the compiled candidate. |

The final strict-owned peak estimate is 924,995,117 bytes, not the
321,635,456-byte post-release live estimate. Timers in different owner scopes
are not summed into solver wall time. The D0 cost map therefore identifies
automatic admission and finalization as material, but does not assign all of
their time to a reusable structural computation.

<a id="d1-native-gate"></a>
## D1 — served work, heterogeneous roles and the gate

Temporary opt-in sinks recorded completed automatic carrier batches, final
selection work and strict exact-carrier service. These are work populations,
not all represented reforge outcomes or expected future crafting visits. The
coarse diagnostic key sorts the three prefix slot statuses and retains suffix
status, prefix/suffix occupancy, flags and fractured/crafted counts. It omits
literal members, four junk vectors, observation context and complete native
operator identity, so it is weaker than the proposed H0 and cannot serve as
H1/H2. Different prefix permutations and coarse IDs within a key are only
heterogeneous *candidates*.

| Screened operation | Served population and recurrence | Optimistic measured opportunity | Gate result |
|---|---|---:|---|
| Protected-side kernel work during automatic admission | 7,204 completed batches; 656 coarse keys, 490 with more than one prefix-status permutation. | Top three cost-ranked keys total 3.881 s of **all** protected kernel time. This even credits the first build and all binding-specific work. | Each key is below the 12 s (5%) whole-run priority screen; existing exact/batch reuse, member and stop-set binding still require H1/H2. |
| Final sparse policy-row selection | 7,212 expanded non-goal states; 135,519 rows and 9,315,595 transition visits; no new large-value cache entries. | Entire selection is 0.020 s. | Even eliminating it cannot be material. Numeric successor values and chosen actions remain binding-specific. |
| Strict selected-row construction | 8,453 served strict locators, 893 coarse IDs and 178 exact-kernel hits; 175 coarse keys, 121 with multiple prefix permutations. | All cold selected-row builds total 0.466 s; top three keys 0.122 s. Pre-row identity checks total 0.567 s. | The expensive 66.655 s carrier-discovery phase is not this repeated selected-row construction. Native exact identity is already checked before build; no role-level pre-work guard exists. |

The strict cursor's measured whole-loop spans sum to 16.004 seconds, including
materialization, native identity checks, alternatives, row publication, byte
accounting and output. Its top three coarse keys total 4.368 seconds of that
inclusive span, still below 5% of the full solve even under impossible complete
elimination. The other approximately 50.7 seconds of run B's 66.655-second
carrier-discovery phase includes one-time oracle initialization and later
partition-node preparation as well as unseparated work. It is **unattributed**,
not a saving estimate or proof of a universal negative. Existing native
strict-row identity, exact-kernel and later proof-payload reuse remain active;
the latter reports 8,278 reuses in this run. The [G0 reforge candidate](../2026-09-23-cross-binding/README.md#g0-native-discovery)
still lacks its separate preconstruction guard.

The top three screened coarse families contain genuine distinct state IDs and
prefix permutations, but equal descriptive masks do not establish legal action
maps, complete support, outside continuation ports or a cheap guard. A bounded
closed-with-ports region inspection would only be meaningful after an expensive
candidate operation survived this first cost screen. D1 therefore makes no H1
or H2 witness claim. A future experiment would need a newly measured native
operation and guard before a template, not a larger histogram of masks.

## D2–D4 — implementation boundary and knowledge

D2 and D3 were conditional on D1. No cold-versus-reuse implementation, finite
binding timing comparison, A3/bow root qualification, 20% root-cost test or new
capacity claim was run; the proposed thresholds were not treated as failed
runtime comparisons. The diagnostic's two successful A4 reports retained the
same bounded verified controller. No new policy or exact/lower authority follows.

The received [mathematics](research-inputs/downstream-role-plan-9b1fb8e/MATHEMATICS.md)
is incorporated conditionally in [representations](../../solver/mathematics/representations.md#downstream-role-signatures),
[policies](../../solver/mathematics/policies.md#bound-role-ports) and
[search/resumption](../../solver/mathematics/search-and-resumption.md#work-weighted-role-opportunity).
The [scheduling owner](../../solver/scheduling-bellman.md#search-flow) now
states its nonfocused/focused fringe guards. [RQ-003](../../solver/research.md#rq-003)
and the existing CLM-0004/0005 histories record the scoped application without
a new theorem or native correspondence claim. The supplied
[advisory](research-inputs/downstream-role-plan-9b1fb8e/ADVISORY_INPUT.md)
is retained as attributed input: its warning about G0's limited scope is
accepted, its universal enqueue implication is qualified by source, and its
large downstream reuse premise remains unproved.

Validation: packet path/hash/size verification; both full-duration A4 native
diagnostics with independently matched compiled costs; 18/18 abstract packet
checks (illustrative mathematics only); native builds before diagnostics and
after removal; solver knowledge lint, edited links and diff review. No broad
suite, WASM rebuild, Simulator, UI review or push is implied by a diagnostic-only
outcome.
