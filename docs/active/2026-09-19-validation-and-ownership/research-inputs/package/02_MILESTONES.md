# 2. Selected milestone plan

**Programme:** Reliable validation and retained-artifact ownership  
**Baseline:** `d13c9b835186ce35cb51aef0de6d0027f8b8a1a7`  
**Default authorized execution when Oliver dispatches this package:** M0–M3 only.

## Milestone map

| Milestone | Outcome | Gate to proceed |
|---|---|---|
| M0 — Freeze the starting contract | Exact baseline, applicable local changes, failure classification, prerequisite and retained-pool writer map | No ambiguity about which source, data, runtime and lifecycle will change |
| M1 — Repair the change-validation loop | Correct interpreter/dependencies/collection; stable evidence bytes and report identities; reproducible selected prerequisite routes | Intended required validation actually runs; no required absence disguised as pass |
| M2 — Close one retained-artifact boundary | Existing retained pool has exclusive mutation control and a passive availability view, with compatible transfer to publication | Selected bypasses retired; role/order/cap/authority/ownership invariants preserved |
| M3 — Qualify and close | Focused negative controls, source-matched downstream qualification, canonical documentation and truthful receipt | Report precisely what passed, failed, remains open; stop rather than select the next programme automatically |

This is not a promise that all debt can be removed in one session. It is a finite selection with explicit stops. Evidence-driven implementation choices within a milestone are allowed; widening the algorithmic or ownership scope is not the default escape from a failed gate.

## M0 — Freeze the baseline and the actual affected boundary

### Work

Confirm local HEAD and the remote-reviewed pin. Preserve uncommitted and unrelated work; never reset or clean to manufacture a baseline. Exclude owner-protected root path `0` from inspection, staging and any broad command. If HEAD differs, inspect the relevant intervening changes and reconcile this plan before proceeding; do not force a checkout over current work. Read applicable operating rules and the current handoff only as needed. [R01–R03](SOURCES.md#r01).

Retain the A7 references and identities. Distinguish source commit, worktree source-byte hash, native executable, WASM wrapper, WASM binary, compiled manifest, worker-prepared data and request/treatment identities. Their hashes need not be equal because they identify different objects. Do not overwrite a saved run or revise its identities to make reuse pass. [R11](SOURCES.md#r11).

Inventory the actual consumers for the selected retained-pool boundary: admission, validation, pruning, selection, const observation, move-out, memory accounting and publication. Include current private/strict complete-assertion transfer; do not stop at public getters. Record a compact function-level mutation table in the living programme note. This is a local boundary audit, not an all-repository class census. [R12–R15](SOURCES.md#r12).

Classify all initial failures: hosted environment/collection/data, byte identities, positional research test, startup/cancel/ordinary-step latency, total delivery, Conquest-four WASM quality. Establish which test selectors are fast, which import native bindings, which need real data, and which run simulations. [R20–R28](SOURCES.md#r20).

### Output and gate

One brief baseline/coverage note and the existing evidence paths are sufficient. Identify the local preserved binary/data availability rather than assume it. M0 passes when intended test collection, relevant artifact provenance and writers are known well enough to avoid a blind edit. A missing byte object remains unavailable. No long solve is required to rediscover a failure already established by source or CI.

## M1 — Make validation reproducible and truthful

### M1a: interpreter, dependency and collection ownership

Use one explicit project Python resolution for project scripts, dependency installation and tests. Bind the Windows workflow to its selected interpreter through the existing override, rather than let `py -3` silently choose another installed version. Consolidate only the resolver usages encountered here, including the project Harvest generator call in the WASM script. Emscripten's own SDK interpreter remains SDK-owned. Do not change global configuration. [R20](SOURCES.md#r20), [R21](SOURCES.md#r21), [R29](SOURCES.md#r29).

Declare the test dependency through the existing package/development setup; do not make the optional GUI a prerequisite for every test. Establish actual collection of both TestCase and top-level pytest tests, inspect `load_tests` and package import collisions, and preserve explicit expensive selectors. A count alone is insufficient: inspect representative test IDs and use one deliberate negative collection canary. Do not edit production test bodies merely to make a collector accept them. [R25](SOURCES.md#r25), [P01](SOURCES.md#p01).

### M1b: evidence portability and stable report identity

Repair the failing research test by selecting the `support` observation by stable ID, verifying its kind and evidence role. Test prepending and reordering valid records. The actual conditional failure history remains first if that is its authored order; no history rewrite or fabricated `sources` array is needed. [R23](SOURCES.md#r23), [R24](SOURCES.md#r24).

For hash-bound fixtures and evidence, inspect stored Git bytes and applicable attributes. Preserve recorded bytes across checkouts. Use scoped no-conversion rules for immutable-byte artifacts where appropriate; fixed LF rules are suitable only when that is the established canonical byte form. Some current records are stored with CRLF, so a global JSON normalization is specifically disallowed. No mass renormalization, changed historical expected hashes, or global Git-setting workaround. Validate the actual selected paths on Windows and a non-Windows checkout. [R11](SOURCES.md#r11), [R33](SOURCES.md#r33), [P02](SOURCES.md#p02).

### M1c: prerequisite ordering and clean environments

Prepare the required native library and data before tests that import/use them. Classify genuinely self-contained tooling tests separately from real-data integration. The temporary fixture pipeline in `test_compiled_data.py` is an existing useful owner, but its small dataset is not a replacement for `test_natural_t1_corpus.py` and the production corpus. Marking a test does not by itself avoid import-time native dependencies. [R26](SOURCES.md#r26), [R27](SOURCES.md#r27).

Resolve the pinned full-data provisioning route from actual available canonical sources, provenance and permissions. Reuse the owning ingest/compiler. Never fetch the latest game snapshot or current economy to satisfy a historical fixture, invent the source of the recorded manifest hash, or publish local data as a new public artifact without authorization. The reviewed repository does not by itself establish a complete clean-host full-data route. The executor must establish it or return that specific dependency as blocked.

In a selected required lane, missing compiler, artifact, binding, test dependency or runtime is a failure/unavailable result, not a successful header-only fallback. Optional local modes may stay optional, but must state their coverage. Use existing script/CTest/workflow owners; CTest fixtures can express an actual setup dependency without a new scheduler. [R19–R22](SOURCES.md#r19), [P04](SOURCES.md#p04).

Prepare web dependencies when that layer is selected and keep typechecking explicit. A committed WASM module can test its own surface, but cannot qualify changed native semantics without a matching rebuild/provenance. Do not add a required native/WASM build to a pure research-report edit. Preserve conservative unknown-change handling rather than develop a complicated new CI classifier in this programme. [R29](SOURCES.md#r29), [R30](SOURCES.md#r30).

### Acceptance

The repaired commands must demonstrably collect the intended styles, detect the canary failure, preserve original hash-bound evidence, pass the corrected report fixtures, and run the declared prerequisite-dependent validation without hidden warmed-machine state. Record collected IDs/counts, interpreter/version and actual dependency inputs.

The existing hosted workflows should be capable of completing their declared checks. Local reproduction is not an observed hosted green run: commits remain local until Oliver requests a push. If a required full-data route cannot be established without an owner decision, retain safe M1 repairs and report M1 incomplete; do not skip the lane to claim green. **Do not begin M2 while its affected required qualification remains unavailable.** An unrelated optional GUI lane does not become a new prerequisite.

### Stop / rollback

Stop on unresolved data origin/permission or a necessary collection semantic change. Do not spend the session building a dataset registry, remote cache service, general test framework or all-purpose CLI. Revert only the selected failed experiment, preserving user work and immutable evidence. One narrowly named blocker is better than a misleading full-suite pass.

## M2 — Close the retained-artifact mutation/read boundary

### Scope

Finish the selected part of the existing `IncumbentPortfolio`: retained-pool admission, deduplication, bounded replacement, explicit pruning, passive reading and controlled transfer to publication. The implementation design is in [03_ARTIFACT_OWNERSHIP_DESIGN.md](03_ARTIFACT_OWNERSHIP_DESIGN.md).

This is not a wholesale conversion of every `SolveWork::Impl` method. Initial proposal construction, full scheduler ownership, all output storage and every coroutine may remain where they are. However, every writer to the **selected retained pool** must migrate: a new facade over an externally mutable vector does not pass.

### Work and invariants

Keep materialized-but-unverified and fully verified roles distinct. Preserve the existing four-entry limit, deduplication identity, storage comparator, admission opportunities and proposal priority. Retention returning “handled” without storing a noncompetitive fifth candidate is an existing behaviour; a clearer internal disposition must not accidentally reinterpret it as a resource failure. [R13](SOURCES.md#r13).

Make invalidation/pruning an explicit mutating operation at the existing algorithmic service boundaries. Provide a const owner view for availability and evidence identity. It must not erase entries, trigger verification, hash entire graphs on every progress read, or change scheduling. Any cached eligibility must state the source generation and invalidation conditions; an unknown/stale verdict is not a valid certificate. Avoid a second selection implementation in JSON emission.

Route the direct publication min/move/erase sequence through the same pool owner, retaining its existing ordering and transfer semantics. Continue to use the current complete private/strict assertion handoff. The publication pipeline still owns final normalization, cap classification, invariant checking and sealing; the pool must not acquire a second result-publication authority. [R14](SOURCES.md#r14), [R16](SOURCES.md#r16).

Preserve failure safety under candidate-copy/transfer overlap, allocation refusal, cancellation, graph-prefix invalidation and source-generation change. No new cheap scalar may replace a graph/certificate/context/value tuple. A root-only private result must not become a parent-state value table.

Retire the mutable retained-vector alias and the external mutation paths it enabled. Reconcile all matching memory accounting, particularly the four-pointer compatibility subtraction. Remove only compensation for reference shells actually removed, and audit both live and full estimates. No double-counted sharing, uncharged temporary copy, work refund, or arbitrary tolerance/cap increase is permitted. [R12](SOURCES.md#r12), [R15](SOURCES.md#r15).

### Acceptance

A small focused fixture can construct, retain, invalidate, observe and transfer the relevant artifact roles without requiring a whole solve. Existing callers cannot directly mutate the retained pool. Negative tests demonstrate that a malformed/stale/partial bundle cannot be advertised as usable. Repeated passive reads do not change pool contents, cursor/order, invalidation counters or mathematical authority. Admission/selection produces the same decisions on the deterministic fixture sequence. Memory near-cap tests preserve the old valid fallback on failed replacement.

The retained pool may move to a narrow private file/header if that reduces dependencies, but no exact filename or class size is a goal. Do not pass unrestricted `Impl&` into another giant manager and call the invariant localized. Narrow context values, existing validation helpers and explicit outputs are preferable. The complete mutation map from M0 is the coverage check.

### Stop / rollback

If isolation requires changing action scheduling, candidate eligibility, the native checker, comparison tolerances, all of `Impl`, or the public ABI, stop and present that boundary. Do not widen the programme without a new decision. A passive-view-only partial change must be labelled partial, not “portfolio ownership completed.” Do not launch the queued setup conversion as a substitute.

## M3 — Qualify the retained change, update the canonical record and stop

Use the focused existing selectors, negative cases, runtime/profile distinctions and receipts in [04_VALIDATION_AND_EXPERIMENTS.md](04_VALIDATION_AND_EXPERIMENTS.md). Build the affected native targets and actual release WASM as required by changed semantics; preserve the current finish-TU O1/non-LTO exception and all other flags.

Check the graph/certificate/context/value bundle, retained role transitions, cancellation/Finish precedence, pending complete private evidence, final seal immutability and resource accounting. Keep actual source/runtime identities. Evaluate fixed retained graphs where that settles the change; do not pretend fixed-graph checks alone demonstrate unchanged discovery. Use a small justified same-request search comparison for the changed retention decision path and the actual Calculator Finish probe for delivery. Timed arms run serially through existing supervision.

Preserve the currently passing 10-second Finish contract. Report the already failing 250-ms initialization, 1-second setup cancellation, 250-ms step and 65-second delivery gates honestly; they are not waived or relabelled as passing merely because this is a structural programme. Known baseline failures need not force an unplanned setup rewrite, but new regressions in the affected lifecycle cannot be dismissed as pre-existing. Use a matched baseline where attribution matters.

The actual Calculator probe currently asserts status/usability, **not those timing thresholds**. Explicitly evaluate the corresponding trace intervals; exit code zero alone is insufficient. Use the correct clock origin and expose missing timestamps. [R36](SOURCES.md#r36).

Run final web tests/typechecking and relevant native contracts by impact, not after every intermediate edit. No unchanged retained-policy simulation. If a changed executable graph genuinely needs new sampled qualification, use the owner-approved 1000 trials and retain censored runs as censored. No broad overnight solver campaign is selected.

Update the existing mathematical/mechanism/source owners as described in [06_CANONICAL_DOC_MAINTENANCE.md](06_CANONICAL_DOC_MAINTENANCE.md). Keep one living programme record and a short HANDOFF. Report actual changed boundaries, removed bypasses, tests, unresolved lanes, before/after qualitative dependency surface, and the next decision. Do not allocate a new claim for ordinary encapsulation or mark a broad open claim proved by fixtures.

## End condition and later priorities

After M3, stop. Recommend the queued [two-owner startup programme](07_NEXT_PERFORMANCE_PROGRAMME.md) if no newly found correctness blocker outranks it. Do not automatically implement caches, observer adapters, an action-budget optimizer, universal JSON/timing helpers, header relocation, LTO changes or the action-ledger scheduler migration.

The practical budget is governed by decisions, not a fabricated duration estimate: no fresh solver campaign for M1, no repeat whole-suite tests for every step, and no broad source cleanup after the selected owner passes. Failed or unavailable gates produce a precise handoff; they do not justify guessing.
