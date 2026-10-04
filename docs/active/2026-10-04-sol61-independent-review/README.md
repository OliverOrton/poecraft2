# Independent solver review checkpoint (2026-10-04)

**CANONICAL BUILD PASSED; TWO ACCOUNTING DEFECTS DEMONSTRATED.** Weighted
unit controls pass23/23; Finder generation and unowned reached-entry accounting
fail their retained caps. Four native processes are proved absent; LOCAL released.
Exact finite-r3 receipts are retained. Production remains2ce0a75b; last-reviewed
owner recoverybd3c5b3 was source-only. No production correction, data refresh,
main merge or push was made. The review worktree starts at main
`29d9e666b9d11dbe9ba58959f2598df9495dc49d` and advances only its isolated branch
to requested query checkpoint `2ce0a75b4e98b2c5fff59be60f269df745bd63bc`.
Candidate `f038aa6f26f33adbd9c93669d3291cc86ad3b4da` was inspected through Git:
it changes fixtures/evidence only and has no `engine/src` difference.
Production ownership stays with the solver owner. No competing repair is made.

## Severity-ranked findings

### R1 / P2: every nonzero query cache key is rejected on checkpoint load

**Confidence: source-proven. Consumer: native development checkpoint/replay;
not public WASM replay. Introduced in the query delta, still present at f038aa6f.**

At `engine/src/solver_calc_types.hpp:650`, `automatic_admission_key` places
`query` at bit 33 and `cheap_only` at bit 32. Admission publishes these keys
(`solver_options_automatic.cpp:773`). Save retains the complete uint64 map key.
However `solver_development_checkpoint.cpp:988` still requires `(state >> 32) <= 1`.
For every query q >= 1, the high word is `2*q + cheap`, so load throws
`solver development checkpoint carrier admission mismatch` before reaching the
new query restore loop at lines 1027-1040. Even an empty completed query batch
writes a key and exhibits the same problem. Original unrestricted full/cheap
keys still satisfy this guard.

Minimal reproduction: complete `EldritchPrefixExaltDirect` admission, save a
complete compatible development graph, then load it into a fresh calculator.
The owner's f038aa6f `run_solver_admission_query_tests` adds this round trip at
`engine/tests/test_solver_compile.cpp:4683` onward; it is unrun and should expose
this refusal once save preconditions are satisfied. The earlier 2584-query
receipt contains no save/load invocation in that selector.

Correction gate: validate the low-word carrier and defined cheap/query bit shape
before mutation, then retain exact membership and rebuild the hint mask. Keep
unknown query/high-bit rejection and unrestricted-envelope completion separate.
Run the prepared round trip plus malformed-key rejection under parent LOCAL.
Do not silently weaken the existing assertion or claim restored query coverage
from the previous finite receipt.

### R2 / P2: Finder construction and entry validation lack a pre-execution aggregate work owner

**Confidence: high for source path; no dynamic over-budget witness yet.
Consumer: Finder native held-side grammar, including product conditional
continuations. Inherited at main29d9e666; not introduced by finite query.**

`solver_finder.cpp:794-804` charges only the delta in
`problem_.telemetry().reforge_logical_work_v1`. Native automatic admission records
its separate `automatic_admission_reforge_logical_work_v1` ledger instead
(`solver_calc.cpp:2239-2251`), and no shared owner is installed by the Finder
constructor. This work is therefore absent from that Finder cumulative charge.

At `solver_finder.cpp:650-656`, the reached-entry validator gets the original
SolveOptions (only its memory allowance is adjusted), with no shared budget
attachment. It creates a new calculator and copies
`problem_.reforge_work_budget_owner()` (`solver_selective_completion.cpp:796-803`),
which is null in the ordinary Finder path. It never calls `set_solve_resource_caps`
for that child. Work is added only when releasing validation, clamped to the
remaining allowance (`solver_finder.cpp:626-631`). The completed graph check's
work is itself charged later at `solver_finder.cpp:1285`. Clamping recorded work
cannot prevent native work beyond the remaining cross-stage allowance.

A minimal falsification gate is a finite native-programme census with positive
entries requiring admission work, Finder's remaining work set below that native
unit, and an incumbent already present. Require refusal before the unit executes,
unchanged committed debit, preserved incumbent, and cleanup. Also assert the
ordinary plus automatic generation work is charged once. Current's service uses
an explicit parent owner before generation and validation
(`solver_solve_selective_completion.cpp:110-114`); that qualification does not
transfer to Finder. Coordinate any repair through the parent; no Finder source
is edited by this review.

### R3 / diagnostic: uniform terminal shortcuts presume unit entry mass

**Confidence: source-proven algebraic precondition; no demonstrated production
bug. Consumer: internal attempt helper, whose inspected callers preserve whole
unit-mass support.** This supersedes the original P3 contract-defect wording.

The terminal simplification at solver_options_helpers.hpp:1645-1652 explicitly
uses the identity that a probability distribution composed with a uniform kernel
K gives K. That identity presumes unit entry mass. The one-step shared-reforge
shortcut similarly returns canonical K at1451-1476. Weighted resources retain
entry weights, which can be unequal while summing to one; that does not establish
an API promise for arbitrary subprobability or occupancy measures.

Current production callers are the fixed wrapper, initialized with {state,1},
and Imprint extensions receiving the whole prior native exit distribution at2548.
Unsupported, illegal, observed-choice or empty extensions are rejected as a whole
at2568-2573. Retained supports are moved intact at2604-2609; no conditioning or
partial mass extraction was found. Thus a single input of0.125 or1e-18 is an
out-of-domain diagnostic under this inferred unit-distribution precondition,
not a production probability defect. No production caller with intentional
nonunit support has been established; floating-law normalization remains unrun.
The canonical module tests valid unit controls independently of the attempt
recurrence and only records nonunit probe outputs. No production fix is proposed.

## Partial-held lead and candidate fixture review

The precise bounded-producer cut is static: `solver_selective_completion.cpp:248`
binds all chosen held-side goals; every failed held-slot branch routes to
acquisition at lines 623-627 (protected variant: lines 553-558). Growth retains
partial target progress only after that complete-held gate. This supports a
missing composition lead, not a claim that Current lacks Eldritch Exalt.
`solver_options_helpers.hpp:695-729` already synthesizes Eldritch Exalt when a
nonempty opposite-side subset is satisfied and an unmet target remains rollable.
`solver_compile.cpp:462-476` permits a nonempty same-side held subset.

The f038aa6f Conquest witness constructs one positive native Chaos ordering
under the frozen 8:3:1 count law, using each carrier's actual weighted pool. It
then routes the two saved graphs with native compiled predicates. This is a
useful independent reachability/routing fixture, but it is unrun. It does not
execute the Ember/Exalt word or enumerate that word's full native outcomes, and
it does not compare original-root evaluated economic results. Before calling
this causal, retain actual witness key/mass, assert native Ember1 and Exalt
legality/applicability, complete mass and held-affix preservation, and run the
original-root candidate with every positive programme entry validated. Preserve
the below-tier hybrid modifier's canonical blocking; a target family label alone
cannot permit rolling a blocked higher tier.

The mixed-family and unequal-price fixtures compare against filtered full
admission; both paths share native recurrence, so they are admission/cache
regressions rather than independent probability-law enumeration. Cancellation
and refusal fixtures explicitly preserve committed work and existing query
membership. Their f038aa6f results are all unrun.

## Independently checked provenance

Named preserved artifacts were hashed read-only in the prior follow-up checkout:

- Probe source `conquest-blocker-service-probe.cpp`: SHA256
  `17132b1af765fa971e23689995a93936b317c304a68d2d436ba0b7d4e2986acb`.
- Probe executable: SHA256
  `2fc51469c02bbdc97d4d8acd3778a4070fd7e2663615e740d935533357807644`.
- `conquest5-blocker-service-r2.strategy.json`: SHA256
  `c9335356b9e8450b514dc9786f693b95277c0df7ae654eae89904698ecc51886`.

These match f038aa6f's request/audit and the published graph summary. The probe
source sets `owner.selective_service_orientation=3`, calls the native producer
service, writes its generated graph, and reads no historical strategy. Its raw
log records normal proposal count3, diagnostic proposal3, cost
99095.409160403797, 190 positive/validated entries, retained status and work
15570088. This independently supports the generated forced-proposal provenance
correction. It does not rerun or independently certify the historical native
check. Raw peak0 remains unmeasured: the probe bypasses outer-step peak updates.

The old request lacks the newer `product_action_envelope`. The graph/source,
compiler support objects, forced dispatch and request scope differences remain
comparison gates. No finite-query speedup, normal enumeration, public activation,
matched economics or historical target recovery follows from this evidence.

## Inspected and unrun boundaries

Inspected: all query production deltas, fixed-unit resource wrapper and weighted
helper, producer/controller and per-entry validator, compiler binding guards,
Current/Finder consumer lifetimes and work paths, checkpoint save/load validation,
census construction and positive visit assignment, and the f038aa6f fixture diff.
The validator checks `root_expected_visits > 0` without an epsilon cutoff;
source still binds exact item, occurrence, programme, resources and held exits.
The fixed-unit wrapper only canonicalizes a fully supported/legal fixed word;
it does not replace cooperative weighted reward accounting.

Current currently destroys the validator before moving/resetting the checker's
result. Finder also releases validation before its checker. No dangling lifetime
is established at these checkpoints. Any proposed earlier checker release needs
stable ownership of the complete result, including census/provenance/cost/output,
then measured aggregate overlap and byte accounting before qualification.

No independent native test result is claimed. Existing 2584 query +171 growth
+81 entry receipts are reused only as their owner's historical evidence. Bow4,
Bow5, Ring, Amulet and armour economic controls are unrun on this review source;
WASM and authored evaluation changes are not qualified. No restored full-envelope
closure, positive lower, exact optimum or broader consumer activation is granted.

Release recommendation: hold query checkpoint/replay qualification until R1 is
corrected and the expanded finite selector passes. f038aa6f is test/evidence
preparation, not a completed recovery repair. R2 requires separate Finder budget
falsification/qualification; R3 is a scoped latent helper contract. Current
recovery release additionally requires the actual partial-held implementation,
original-root and every-positive-entry checks, preserved controls and matching
WASM evidence from the owning integrator. Available for correction re-review;
parent sends the next candidate and controls LOCAL. No production commit exists
from this review.

## Correction re-review: deb65256

Parent supplied `deb65256ca687bfbf9d2c43bb39c0074ea363772` while the solver owner
held LOCAL for its corrected qualification batch. This review inspected the
actual `f038aa6f..deb65256` production and fixture diff read-only. No duplicate
build or selector was run.

The decoder correction at `solver_development_checkpoint.cpp:984-1025` addresses
R1 at source level: low-word carrier, cheap bit and the full remaining query word
are decoded separately. IDs >=13, reserved high bits, invalid carriers, out-of-range
operators and duplicate cache keys are rejected before planner import. Queried
membership must be a native Eldritch intent with the matching side/final action;
a direct query permits only a one-action word, and nonempty cheap Eldritch
membership is rejected. Empty legitimate cheap queries remain possible. This
matches synthesis/query filtering. Query keys remain distinct from query0;
restoring the query hint mask at line1072 cannot itself publish unrestricted
membership. Existing coarse-graph validation is still required before import.
No construction, debit, probability/resource recurrence, root binding or consumer
activation code is changed by this correction.

The committed-debit witness changes Exalt to Chaos (`test_solver_compile.cpp:4633`)
after Exalt measured zero native reforge work. Assertions for positive debit,
staged interruption, rollback without refund, retained prior membership,
uncached retry and refusal before a new unit all remain. The checkpoint fixture
adds exact modifier identity to both save/load calculators, retaining the same
root/goal/action/prices, and now asserts no admission state growth at line4705.
Its previous run failed the save cardinality precondition; it never measured
loader refusal. R1's original diagnosis was an independent source proof.
These fixture changes address preconditions rather than weakening acceptance.

Checksummed malformed-key negatives at lines4743-4778 retain exact refusal
reasons, unchanged fresh state/operator counts and successful valid-load retry.
The fixed header checksum offset agrees with format7's magic, version/endian,
nine uint32 layout sizes, payload length and checksum. Full native partial-held
Ember1/Exalt checks at lines4983-5007 additionally bind two paid actions/resources,
positive complete exit mass, preservation of all four incoming affixes and
canonical below-tier blocking. Existing 1e-12 mass tolerance remains unchanged.
This is native-word evidence only, not an original-root economic comparison.

No new correctness finding was established in this narrow correction diff.
Source recommendation: accept the narrow R1 correction for the owner's declared
finite qualification; hold runtime/replay release until that batch passes.
The owner-published f038aa6f routing25/25 receipt proves a positive native Chaos
ordering (mass2.4457291281364614e-9, exact goal mask9) and native routing to
historical s3 Ember/Exalt versus Current c5 Chaos. The query2701 receipt preserves
six failures, not a passed gate. New word/decoder/negative tests at deb65256
remain unrun by this reviewer and await the owner's result. Recovery construction,
performance, full envelope, original-root economics, controls and WASM remain
outside this correction qualification.

## Isolated falsification fixtures prepared; request next review LOCAL

The parent separately authorized source preparation for R2/R3. The only new code
is [review-native-witness.cpp](review-native-witness.cpp), outside engine source,
CMake and other owners' test selectors. It includes the existing finite
`test_solver_compile.cpp` session factory and defines its own standalone main.
Unused test entry points are discarded at link time; none is invoked. The
[source-pinned batch request](review-batch-request.json) binds the compiler input
bytes, unchanged production base2ce0a75b, three modes and exact command arrays.
The source is **uncompiled and all three modes are unrun**. A parent LOCAL grant
is required after the solver owner's active batch releases it.

- `weighted`: enumerate `CalcContext::outcomes` separately for each exact positive
  input, then directly sum entry_probability times every native exit. Compare
  the cooperative helper's entire exit map, expected primitive count and resource
  vector. Cases: unit deterministic control; weight0.125 deterministic Ember;
  positive1e-18 Ember; two identical-signature Chaos entries totaling1 as control;
  and two totaling0.125. The fixture-only Ember descriptor repeats a cost key to
  verify multiplicity. Prices/data/mechanics are not edited. Unit controls and
  dyadic scaling permit exact comparison; no production tolerance is changed.
  Predicted baseline failure: nonunit exit mass becomes1 while rewards retain
  entry mass. This tests a latent helper contract, not established production EV.
- `finder-generation`: ordinary product `PolicyFinderWork` with its default
  conditional grammar/eight-attempt promotion, original clean Rare root, exact
  four-goal2+2 request and declared prices. Cap50; compare the actual calculator's
  ordinary plus automatic committed work to Finder telemetry on each native
  continuation until completion or the first overrun. Record full native telemetry,
  then bounded-finish and cancel admission while borrowed prices remain alive.
  Cancellation must not refund either ledger. This reaches the actual product
  constructor; it does not fabricate the omitted-work counter or private owner.
- `validator-budget`: construct the real retention controller, compile within
  that original request, and obtain the complete accepted original-root native
  result/census with actual positive visits and exact occurrence identities.
  No visit scaling, fabricated census or predicate substitution. Revalidate every
  positive entry with max_reforge_work1 and no owner (Finder's component invocation),
  then compare the same immutable census/control/prices with an exhausted shared
  owner whose single committed unit must stay charged and block the next unit.
  Save the graph/census and report work/refusal separately. This is a component
  witness using real reachable entries; it does not establish a constrained whole
  Finder's incumbent or final accepted-policy behavior.

The broader helper's production call-site review found ordinary Imprint extension
seeded with unit mass; no production nonunit caller was established. The producer,
Current and Finder tests have separate qualification. These fixtures do not
re-open resolved completed-dead-end sentinels or the prior wrong-budget-owner
fixture diagnosis. They do not run frozen economic cases or alter their semantics.

Exact commands are recorded below and in the request; run only after parent LOCAL.
Build is the existing canonical Engine target with two compiler jobs, followed by
one standalone fixture compile. No full acceptance, Simulator, browser or WASM.
Native modes run serially under existing `run_isolated_process`, not a new runner.
Each mode has a45-second native deadline,40000 continuation bound and75-second host
watchdog; build/compile host watchdogs are600/180 seconds. Persist raw output and
existing process/survivor receipts. Exit0 means contract assertions passed,
exit1 means a measured falsification needing inspection, and exit2 means setup
or native refusal (no intended witness claim). The three independent modes each
run once; an anticipated contract-negative is preserved and does not authorize
an unchanged rerun. Stop the batch for setup/identity failure, timeout, survivor,
or build/compiler failure. Read every failed assertion before disposition.

```powershell
powershell -NoProfile -File scripts/dev-engine.ps1 -Task Engine -Jobs 2
C:/msys64/ucrt64/bin/g++.exe -O0 -std=c++20 -ffp-contract=off -ffunction-sections -fdata-sections -static-libstdc++ -static-libgcc -Iengine/include -Iengine/src -Ibuild/engine/generated docs/active/2026-10-04-sol61-independent-review/review-native-witness.cpp build/engine/libpoecraft_engine.a -Wl,--gc-sections -lPsapi -o out/sol61-independent-review/review-native-witness.exe
out/sol61-independent-review/review-native-witness.exe weighted out/sol61-independent-review/weighted
out/sol61-independent-review/review-native-witness.exe finder-generation out/sol61-independent-review/finder-generation
out/sol61-independent-review/review-native-witness.exe validator-budget out/sol61-independent-review/validator-budget
```

Those are command vectors for the existing supervisor; do not launch them bare.
From the isolated review worktree, set `PYTHONPATH=tools/ingest;bindings/python`,
load the request, verify every listed SHA256 and call
`run_isolated_process(command, watchdog_seconds=stage['host_watchdog_seconds'], cwd=Path.cwd())`
for each stage in order. Save `result['output']` as a log and all other returned
fields as its receipt under `out/sol61-independent-review`. Hash the resulting
library/executable and persist source commit/compiler identity before running
modes. The reviewed static link flags mirror native tests; `--gc-sections` drops
uninvoked external test-suite references. Any link/setup failure is new evidence
to diagnose, not permission to link fake test stubs or broaden the native batch.

Request to parent: assign the next finite review-test LOCAL batch when available.
Until that grant, continue candidate/evidence review source-only. Source-ready
fixtures are not a qualification receipt or a production repair. Initial review
findings and release limits above remain, except R1 is now corrected at source
in deb65256 and pending the owner's corrected runtime gates.

## Narrow production reachability follow-up while LOCAL is queued

At the parent's request, a source-only caller trace covered engine/src plus
engine/include, bindings, apps and tools. Production direct cooperative calls
are only the fixed-unit wrapper (`solver_options_helpers.hpp:1689`) and Imprint
frontier extension (line2548). `solver_options.cpp:251,294,324,566,1125` all invoke
the fixed-unit wrapper, which supplies `{entry_state,1.0}`. The bindings/UI/tools
search found no additional direct caller. These two production files have no
2ce0a75b..deb65256 delta.

Imprint starts from `{state_id,1.0}` at lines2377-2410. The extension receives the
entire frontier support at lines2548-2550. It rejects incomplete/illegal/choice
results at lines2568-2573; retained support is the complete moved entry vector
at lines2604-2609. Goal-mask inspection and pruning do not condition that vector:
next prefixes share the unchanged support at lines2686-2701. Illegal positive
entries reject the whole extension; Pareto/depth/economic pruning removes whole
prefixes. There is no surviving-mass selection or rescaling at these callers.
Thus, under the existing full native unit-kernel contract, unit mass is an
inductive invariant in exact arithmetic. This rules out an intentional0.125-style
production support path in the inspected source; it does not certify bitwise
normalization, floating drift, underflow or every native primitive law. R3 stays
a latent general-helper contract finding, not a demonstrated production EV bug.
No fixture/request pin changed, and no native command was launched. Pause for
parent LOCAL or the next candidate/evidence; do not recreate the queued batch.

## Targeted finite-r2 premise review (source-only)

Reviewed the solver owner's uncommitted test diff on deb65256, with inspected
`engine/tests/test_solver_compile.cpp` workspace SHA256
`76036f7cf098d4f14d22c2d897a330d5ed1f315d0db69ade4ffd4770c1016c2d`.
No production diff exists in that owner checkout. Native logs were read as owner
evidence: partial-held293/293, native word33 exits/mass1.0000000000000002;
query2714 with five failures. Four budget assertions followed a cached kernel;
the checkpoint still failed save cardinality before exercising load. The owner
stopped dependent checkpoint/metamod gates. No independent native run occurred.

Fresh `CalcContext exhausted` at owner test lines4666-4684 has independent empty
distribution/reforge caches (`solver_calc_types.hpp:1221,1260`). Interning the
unchanged root does not evaluate a row. It reuses the exact committed Chaos query,
goal, registry and prices, and borrows the same already-debited budget owner.
Refusal therefore requires new native work, rather than demanding artificial
work for a valid cache hit. Owner cap at its committed ledger denies the next
positive unit before debit (`solver_calc.cpp:2236-2271`). Reforge memo hits return
without new native work (`solver_reforge.cpp:809-849`), explaining the previous
fixture's invalid premise. Existing same-context cancellation/rollback witness
remains; fresh-context exhaustion is a separate aggregate-owner obligation.
The new refusal, unchanged operators/debit, uncached correctly charged retry,
and prior query/full membership assertions are retained. Optional direct
strengthening: also assert both fresh child's committed work ledgers remain0
on the refused unit; do not clear a warm cache or fabricate a debit to force it.

The synthetic checkpoint at owner lines4691-4727 now holds suffix5/6, prefix3/0,
and Exarch tier1. Its goal adds the already-present family100 flat prefix to the
prior four families, leaving only prefix4 missing (mask29 of required31).
Factory normal-roll prefix0/1/2 are blocked by held group10; prefix3 is held,
and prefix4/group13 is the sole rollable prefix. Veiled prefix8 is outside the
normal random mask. Thus the native direct Eldritch Exalt law has a proper one-step
completion with one goal exit. This describes the primitive law; ordinary
automatic admission may enlarge the retained coarse graph. Save, query, replay and
malformed-load contexts all use this same five-goal fixture request. This is a
changed synthetic checkpoint premise, not a changed frozen economic case.
Calling `checkpoint_work.finish()` at line4714 performs the completed cache
publication (`solver_solve.cpp:803-818`); policy availability and all three cache
cardinalities are checked before admission, and no state growth is checked after
it. This is a valid source-level setup for the existing coarse checkpoint
contract. Runtime must still establish the actual retained closure; if its
preflight fails, stop there rather than resizing arrays or weakening save checks.
The typed-membership/full-envelope and malformed-key negatives remain intact.

Recommendation: the observed premise corrections are suitable for the next
owner finite qualification once LOCAL is assigned; no source blocker found.
Runtime and skipped dependent selectors remain unqualified. This review does
not repeat the decoder audit or reopen the repaired dead-end sentinel. Queued
review fixture/source pins are unchanged; CI holds LOCAL, and this task launches
no build, test or solve.

## Final paired fixture review: 862bd1b

Inspected commit `862bd1b30eedcab61bfb1be5e01055005f9b7c16`, its actual test diff
and proper-checkpoint-gate-request.json. Production source has no deb65256..862bd1b
delta. All final fixture corrections are unrun; no LOCAL grant or heavy command
for this reviewer. Native source still owns the save/cardinality/closure checks.

The fresh-budget negative now uses a measured successful native Chaos control
with exactly the same root, session, scope, prices and intent. It checks the
explicit shared owner, pristine child state/operator/work counts, zero refused
child work, unchanged owner debit/operators, preserved old query/full membership,
and exact retry work/owner delta plus full native semantic equality. This retains
and strengthens the prior negative, without charging valid warm-cache hits.

At test lines4723-4778, the nonterminal checkpoint requires direct native
applicability, one unit-probability goal exit, finished policy availability,
matching cache dimensions and an actual successful native save before adding a
query. Ordinary nonterminal expansion calls prepare_state_expansion with full
automatic admission (`solver_solve_expand.cpp:593,729,3104`). The fixture therefore
requires independently pre-existing full membership at lines4767-4769. Its typed
query must be uncached, nonempty and add no states. Restore at lines4791-4801
requires the exact nonempty typed vector, candidate availability, complete native
semantic/resource/exit snapshots and the unchanged pre-save full vector/law.
This is real programme membership coverage, not just matching a cached flag.
Malformed checksummed nonempty members and fresh-context refusal/retry remain.

At lines4860-4904, the query-only fixture starts from that actual native goal exit,
with its own caller identity, fresh calculator, finished native coarse graph,
cardinality and save prerequisites. Terminal expansion stops before admission
(`solver_solve_expand.cpp:3084`). The empty typed query is explicitly published
then saved/restored. Cache lookup occurs BEFORE goal short-circuit
(`solver_options_automatic.cpp:793` versus822). A restore that manufactures q0
would make the first unrestricted request cached=true, failing line4901 even at
the terminal goal. Dropping the empty typed key fails line4898. An absent q0
returns cached=false, publishes its own empty completion, and only its repeat
returns cached=true. Thus the terminal case is a meaningful cache-completion
namespace negative, not a vacuous check of an empty action vector.

Boundary: these two fixtures do not establish nonempty query-only/nonterminal
checkpoint restore separation; the nonterminal case already owns full membership.
A hypothetical restore bug promoting ONLY nonempty queried keys to q0 could
escape this paired negative. Existing nonterminal live query-only cases still
require unrestricted cached=false before full admission (lines4514-4570), but
that does not substitute for a nonterminal restore witness. Also, terminal goal
closure is already trivial and cannot qualify broader nonterminal full-envelope
or global exact authority. The owner's documentation explicitly limits this
coverage to nonempty query/full replay plus empty query-only completion, which
matches the actual assertions. No source production defect is established here.

Recommendation: source premises are suitable for the declared bounded owner
build/query/checkpoint/metamod batch after parent LOCAL. Successful runtime
acceptance/refusal remains unqualified until that batch passes. Reusing the
unchanged deb65256 native partial-held293 receipt is proportionate: its function,
production source and frozen inputs are unchanged; this is not ordinary controller
or every-entry/original-root economic qualification. Queued review fixtures,
source hashes and commands remain unchanged.


## Native save boundary and first independent LOCAL batch

Owner runtime at862bd1b reports the query gate2728 with one failure: native save
refuses `nonterminal representative state is not expanded`, despite matching
60-state cache dimensions. Fresh-owner accounting now passes (fresh60,
spent-owner124, retry60). These are owner-reported results; no checkpoint replay
or malformed-load runtime was reached in that stopped selector.

Native validate_cache (`solver_development_checkpoint.cpp:608-674`) requires
EVERY interned nonterminal representative to be expanded. It does not restrict
that requirement to decision-reachable states. Native save takes const inputs
and validates before creating a temporary file (`:756-780`); the refusal does not
consume admission work or alter an accepted incumbent. An available policy and
matching vectors therefore do not establish checkpoint-export closure.
Automatic programme evaluation can intern intermediate or subsequently rejected
states (`solver_options.cpp:897-1003`, `solver_options_automatic.cpp:1430-1530`),
whereas expansion schedules admitted row exits and choices
(`solver_solve_expand.cpp:2837-2863`). That is a plausible source explanation,
not an established cause for this fixture: the actual first blocking state has
not been supplied. Before calling this merely an export restriction, identify
that exact state and test its reachability from the original cache start through
ALL admitted positive-probability row and choice transitions, with no epsilon
cutoff. A positive-reachable unexpanded representative needs closure review.
Do not resize, fabricate expanded flags, erase cache states, or relax the guard.

Current native acceptance/every-entry validation does not call development
save/load (`solver_solve_selective_completion.cpp:268-333`); checkpoint APIs and
benchmark flags are optional. Thus this refusal alone does not block preparing
recovery source behind a disabled gate. It does not qualify public recovery,
nonempty query replay, lower/exact closure or any broader consumer. Retain the
unsupported nonempty Eldritch checkpoint boundary separately.
An existing ordinary closed witness is
`engine/tests/test_solver_api.cpp:3124 run_development_checkpoint_replay_gate`:
Normal BodyInt17 level86 to a one-slot Magic goal using Transmute/Alteration/
Restart, successful native save/load and replay/value/strategy equality. The
existing selector is `--solver-checkpoint-only data/compiled/current`; it was not
run by this reviewer. Existing terminal query-only completion remains a separate
empty-namespace witness, not nonempty recovery checkpoint qualification.

Parent granted LOCAL for the originally pinned batch. All14 source byte pins and
request SHA256bb456edb08636d8a9217c4566060410ef32ae88d38f1027108f5c0072a71732e
matched before launch. The canonical Engine-only build used maximum2 compiler
jobs and exited0 in187.3677s. Isolated compile exited1 in14.2189s with the unresolved
real test symbol `run_solver_return_bridge_lifecycle_tests()`, called by the
included test_solver_compile.cpp translation unit. Both supervised processes
had no timeout, cancellation or survivor. The explicit stop condition applied:
weighted, finder-generation and validator-budget are all UNRUN. There is no
native Finder accounting result or production weighted-support counterexample.
LOCAL was released promptly before documentation; no unchanged-failure rerun or
production/owner source edit occurred.

Raw first-batch logs/process receipts remain in `out/sol61-independent-review`;
compact process metadata is retained in `finite-r1-processes.json`. The successful
Engine archive SHA256 is
179a3cfbeaa1ba8d9a542b1728d05b39333c231bb6fe558e86c9e5d6c30e2ab9.

## Link correction and bounded recheck request

Parent authorized a source-only linkage correction while CI holds LOCAL.
Canonical engine/CMakeLists.txt links test_solver_compile.cpp together with
`engine/tests/test_solver_solve.cpp`, whose line15691 defines the real lifecycle
symbol. The new `review-recheck-request.json` adds that unchanged translation unit
to the standalone g++ command. No stub, alternate implementation, test selector,
production edit or witness assertion change is used. The corrected command is
SOURCE PREPARED, UNCOMPILED; static dependency inspection is not successful link
evidence. Fixture cpp and all14 original pins remain unchanged.

The recheck has exactly4 serial stages: compile (1 compiler,180s host watchdog),
then weighted, finder-generation and validator-budget (75s host/45s native each).
It reuses the existing successful Engine archive; no build is authorized. It pins
17 source files, the original production engine tree, archive/configuration/
generated-header/build-receipt bytes, and requires an empty tracked production
source diff against2ce0a75b. Any changed identity stops rather than silently
rebuilding/substituting. Outputs use `out/sol61-independent-review/recheck-r2` to
preserve the failed first receipt. Contract-negative mode exits may continue to
the next independent mode; compile/setup failure, unexpected exit, timeout,
cancellation or survivor stop the remainder. Each mode is attempted once.
Additional timed policy runs, whole Finder/Current qualification, WASM and all
broader suites remain excluded. LOCAL is requested; not held or consumed.


## Canonical target repair, offline reconciliation and request

Second standalone link recheck at4d45875 also stopped before any mode: exit1,
54.2139844s, no timeout/cancellation/survivor. The full log starts with PE
IMAGE_REL_AMD64_REL32 relocation overflows, before undefined template/refptr
symbols. Canonical Tests settings differ from the failed command: Release
-O3/-DNDEBUG/-std=gnu++20, ordinary object sections without --gc-sections,
complete CMake-owned test translation units and native library/Windows inputs.
Both use static libstdc++/libgcc. These mismatches are established; the precise
linker failure mechanism is not. Stop the standalone link loop rather than
inventing symbols or changing production/toolchain flags.

Parent authorized canonical test-only registration in this review worktree.
engine/tests/test_solver_independent_review.cpp is now a normal module in the
existing poecraft_engine_tests target. It includes headers instead of an entire
test translation unit and does not define another main or test counters.
The focused --solver-independent-review-only MODE OUTPUT_DIRECTORY selector
returns before all other suites. A real exported test-only adapter beside
make_compile_session reuses that unchanged synthetic factory. No CTest suite,
production consumer, action or frozen input is changed. The previous standalone
source and both link receipts are preserved; finite-r2-processes.json retains
second-batch metadata. All modes remain UNRUN.

On KIDS reconnect, all20 review source pins,147 native production input pins and
4 failed-link evidence hashes matched the saved canonical request. Engine archive
remained179a3cfbeaa1ba8d9a542b1728d05b39333c231bb6fe558e86c9e5d6c30e2ab9.
Existing supervisor process_identity_token reads found no matching identity for
build54232, failed link73880 or failed link64752; no new process was started or
killed. No canonical-r3 runtime receipt existed. CI owns LOCAL; this reconciliation
and all subsequent preparation are source-only.

Header audit: solver_internal.hpp includes solver_compile_contracts.hpp, which
includes solver_solve_contracts.hpp; apply_solve_profile_defaults is declared/
defined there at294. The explicit Finder, selective completion, option-helper,
handles, JSON and item headers supply the remaining fixture types and functions.
Only canonical test_main owns pctest globals and main. The wrapper/selector
signatures match and each registration is unique. Source audit is not a compile.

review-canonical-request.json is the complete build/link/fixture plan: existing
scripts/dev-engine.ps1 -Task Tests -Jobs2,600s watchdog, then3 serial invocations
of build/engine/poecraft_engine_tests.exe --solver-independent-review-only using
the same75s host/45s native/40000-continuation bounds. CMake regenerates its target
registration and owns compiler definitions, all object inputs and compatibility.
Reuse unchanged native objects/archive where compatible; permit only dependencies
that canonical Tests requires rather than suppressing a correct rebuild. Record
the resulting archive/executable/config/generated/compile-command identities.
No manual linker command, Engine-only stage, broad suite, Simulator, benchmark,
WASM or browser is requested. First build/setup/identity failure, unexpected exit,
timeout, cancellation or survivor stops the remainder. Preserve genuine contract
failures, attempt each mode once and release LOCAL before documentation.

Weighted preconditions: qualified controls are unit distributions within the
existing1e-12 normalization tolerance: deterministic unit, unequal.125/.875,
tiny1e-18 distinct native exit with companion1.0 (rounded unit mass), and shared
Chaos.5/.5. Dyadic/deterministic controls retain exact comparisons. Independent
composition oracle enumerates native primitive leaves per exact entry and
applies entry_probability*exit_probability directly; it does not validate the
primitive mechanic independently of that native leaf. Duplicate Ember cost keys
are explicit synthetic descriptor probes, not production currency changes.
Nonunit supports are labeled nonunit_diagnostic_only and do not trigger helper
contract-failure assertions. No tolerance or resource-weight acceptance changes.

Finder mode must actually reach automatic admission using the ordinary product
constructor/eight attempts/conditional-protected-scour grammar and original
four-goal root. It compares native ordinary+automatic debit to cap50 and the
Finder report, then cancels while borrowed Finder prices remain alive and checks
no committed debit refund. Validator mode requires an actually accepted native
original-root RetentionControl evaluation and immutable complete positive-entry
census with exact occurrence/item identities, not fabricated or scaled masses.
The same census/request/word/prices compare unowned cap1 with a genuinely spent
shared owner cap1. Setup refusal gives no intended accounting claim. Neither
mode qualifies the whole Finder service, incumbent, aggregate peak or Current.

## Default-off recovery review: bd3c5b3

Inspected actual commit bd3c5b3c1b54805d9469c41f75f2befa5cccb430, source and
finite selector; owner HEAD remained that commit on reconnect. No production
edit or native run by this reviewer. Optional development checkpoint remains a
separate held capability, never acceptance or envelope-closure evidence.

P2 qualification gap, high source confidence: native Exalt requires carrier-local
missing-goal rollability, not progress plus free capacity
(solver_options_helpers.hpp:695-716). New dispatch at
solver_selective_completion.cpp:690-700 can choose Exalt below capacity based only
on goal presence. The existing validator remains fail-closed: all reached entries
must be available and strictly positive with no epsilon skip at1122-1126; it uses
the original goal, registry, actions and exact item, rederives typed membership,
then throws if there is no matching admitted native intent. No widening or entry
mass deletion was found. This is a coverage gap, not a proved acceptance defect.

Minimal proposed negative for fixture0: give native modifier2 both groups13 and10
(current primary13), use prefixes{3,2}, suffixes{5,7}, chosen anchor5. Modifier3 is
one satisfied target goal; modifier2 blocks missing goals0 and4 while target count
is only2. The anchor is present and the other held goal is below-tier blocked.
Dispatch selects growth Exalt, but native rollable_missing is empty and admission
refuses it. Preserve native groups/masks, establish a positive original-root Chaos
entry with native enumeration, and require whole-candidate refusal with the same
incumbent and committed debit. Reachability/mass and refusal are UNRUN here.
The four current fixtures use one primary group per modifier and therefore do
not exercise this compound-blocker boundary. Feedback was sent to parent.

P3 capability limit, high source confidence: final stages2/3 are existing
RerollVersusRepair templates. With the three-goal side complete and one useful
small-side goal below capacity, their inherited route at711-716 selects Chaos
rather than available progress-preserving Exalt. This bounds the private policy;
it does not mean the native primitive or broader Current/Finder search is absent.
No original-root economic benefit or regression is established from this review.

Construction is structurally bounded: two singleton anchor identities, four
sequential existing stage producers, one active admission cursor, node/program
reservation bound and retained+assembly memory checks. Original root/goal/action/
price identities remain bound to the composed graph and full native evaluation;
stage3's tier copy is an admission proposal context. Singleton versus both-goal
held identities are dispatched separately; every native stage returns through
original-goal dispatch. Exact tier setup/direct words and native resource keys
are retained. The producer has no incumbent/publication API and is default-off;
Current/Finder proposal count and vocabulary selection are unchanged. Candidate
construction alone confers no properness, checked upper, lower or exact authority.

Recommendation: source is suitable for the declared finite falsification batch,
with compound-blocker refusal still untested and all build/runtime results pending.
No public activation, frozen Conquest controller/economic result, wider closure,
whole-consumer lifetime/peak accounting or WASM qualification is recommended.


## Canonical finite results and source-only correction plan

Actual f442b4d8 canonical Tests build passed in83.971774s with28 test translation
units, verified Release compiler/object/link inputs and the unchanged native
archive179a3c... . Tests executable SHA256de666d4ebe0a2dcf640c56135bf968b379376952d22d32210528c4c629044dbe.
The three serial modes then finished in0.295171s,0.025426s and0.031209s.
All four processes exited with no timeout, cancellation or supervisor survivor;
the existing identity observer proved them absent. The original build receipt
retains an extra creation-token check=true, which was incorrectly treated as
liveness in the wrapper. Existing observe_process_identity handles exited
Windows processes with retained handles; reconciliation proved absence before
resuming only the modes. No build or mode was repeated. LOCAL was released before
documentation. Compact exact receipts and summary are committed as finite-r3-*;
raw build log and all prior failures remain under out/sol61-independent-review.

R2/P2 is now demonstrated for ordinary Finder generation: native work88 (40
ordinary+48 automatic) exceeded cap50 while progress reported40. Its actual
public-product constructor used eight attempts and conditional-protected-scour,
and automatic admission was reached. Four checks had two expected contract
failures; cancellation retained both committed native ledgers. The reached-entry
component separately completed all six actual native positive entries, consuming
48 despite cap1 when unowned. Its original-root graph succeeded with native
probability1 and cost2397.8834314550018. The same immutable census with a genuinely
exhausted shared owner refused before any child work and preserved owner debit1.
Three checks had one contract failure. The committed census JSON is a projection
of those actual entries; validation used the complete native object, never this
projection, a scaled entry or synthetic visitation count.

R3 unit controls pass23/23 checks, including unequal weights, positive1e-18
distinct exit, repeated synthetic descriptor cost keys and shared native renewal.
Shared native exit mass1.0000000000000004 meets the retained1e-12 contract.
Nonunit probes reproduce normalization toK with weighted rewards but are labeled
diagnostics, not contract failures. No production weighted-helper defect remains
established. Do not propose a probability/resource-law change from those probes.

### Proposed correction: explicit existing resource owners, no mechanics edits

This is a source-only plan, not an implemented or qualified repair. Production
files remain unchanged and the solver owner holds LOCAL for recovery. Minimal
consumer changes are solver_finder.cpp/.hpp and solver_selective_completion.cpp/
.hpp; the narrowly necessary owner-attachment/lifetime helper is in
solver_calc_types.hpp and solver_calc.cpp. Tests use the existing canonical review
module. No change is proposed to solver_solve_selective_completion.cpp, the
recovery grammar, automatic-candidate/query eligibility, native kernels, prices,
checkpoint authority, or weighted helper.

1. Finder owns an explicit request budget using the existing CalcContext owner
   mechanism, capped at limits.max_reforge_work. This ordinary budget context
   performs no native rows, so its automatic-scope depth stays zero and existing
   consume_reforge_work checks the cap before every forwarded native unit.
   Chain any prior external owner rather than replacing/refunding it; maintain
   the stricter local and parent ceilings. Do not attach a context to itself or
   create a cycle. Root, registry, goal, actions, programme indices and caches in
   problem_ stay unchanged. The request budget starts at zero for new Finder
   work; existing caller/parent ledgers are never reset or overwritten.

2. Attach that budget to generation before native work begins. Automatic child
   and comparison contexts already inherit the owner at
   solver_options_automatic.cpp:1063,2085; their wide local caps therefore do not
   authorize work beyond the explicit owner. Parent automatic-scope forwarding
   at solver_calc.cpp:2239-2251 likewise debits it first. Keep separate native
   ordinary/automatic diagnostics; sum them for independent measurement, not a
   second budget charge. Reconcile Finder progress/telemetry to the authoritative
   cumulative request-owner debit on every returned slice and all exits. Remove
   ordinary-only accounting at finder:794-805 and post-hoc min-to-cap masking.

3. Once the evaluator is created, give it only the genuine remaining allowance,
   including any stricter pre-existing owner. The existing evaluator has no
   owner hook; it uses its own max_reforge_work (solver_eval_types.hpp:391ff).
   Reuse Current's exclusive-phase pattern: reserve/bound that allowance and
   transfer diagnostic committed deltas with a per-checker watermark, after each
   slice and BEFORE reached-entry validation begins. Restore unused reservation
   only if an explicit reservation is used; never refund consumed work. This
   avoids current full-amount charging at finder:1260-1267 and the window where
   validation begins before checker work is charged. Repeated completion,
   exception and finish cleanup must transfer only the not-yet-charged delta.
   Budget/report/remaining authority is the versioned committed logical ledger;
   legacy active-ledger saturation remains refusal-proximity diagnostics. Never
   infer spent work from that active field or structural primitive counts.
   Effective remaining is cap-min(cap,spent), never unsigned cap-spent after an
   overrun. A small read-only effective-remainder accessor on the existing
   owner chain is preferable to resetting parent caps or changing evaluator
   mechanics. Because phases are exclusive, generation/validation cannot spend
   the checker allowance concurrently; preserve that invariant explicitly.

4. SelectiveProgrammeEntryValidator keeps forwarding each native unit to any
   existing shared owner before execution. Reuse that owner directly when its
   effective remainder is already no larger than this validator's local ceiling;
   do not replace an exhausted shared owner with a fresh allowance. For an
   unowned caller, lazily create a bounded fallback owner capped at
   limits.max_reforge_work and attach the exact child before native admission.
   If an owned caller supplies a stricter local ceiling, use a bounded private
   owner that forwards to the existing owner, so both ceilings apply. Setting only
   the child's ordinary cap is insufficient: automatic admission uses separate
   ledgers and local UINT64_MAX caps. The fallback persists across ALL entries,
   resumes and retries; never restart its allowance for another positive entry.
   Include its bytes in estimated_owned_bytes and memory headroom. Destroy/cancel
   admission children before the fallback owner. Preserve the existing positive
   entry, item/scope, semantic-key, held-goal and resource comparisons exactly.

5. Finder validators inherit the same request owner, so their native units are
   already debited and release_validation must not add child work again or clamp
   it away (finder:626-632). Synchronize reporting after success, unsupported
   entry, capacity refusal, bounded finish and destructor cleanup; leave best_
   and already checked artifacts intact. In particular, the generation
   length_error catch at872-880 currently skips charge(): owner-synchronized
   cleanup must retain every committed unit even on that path.

6. Owner lifetime is part of the fix. Current's owner setter stores only the root
   pointer; cached/comparison children can retain a previous pointer. A scoped
   Finder attachment must restore the prior owner through existing retained
   children after canceling any active cursor, without erasing their caches or
   query/full membership. Extend only that private setter/restore helper to
   propagate owner changes (same traversal pattern as accounting at calc:2196ff);
   future contexts already inherit it. Use constructor-safe RAII so exceptions
   restore pointers before request-owner destruction. Include the owner context
   in Finder live/peak bytes. Do not clear native caches or reset telemetry to
   avoid a dangling pointer or to manufacture budget headroom.

### Precise retained regressions for the implementing owner

- Same f442b4d8 Finder root/goal/actions/prices/grammar and cap50: actual committed
  request work must stay <=50, progress must equal that debit after every returned
  slice and cleanup, and refusal must precede the next unaffordable native unit.
  It need not spend exactly50: native units may not fit the last remainder.
  Update ONLY the review fixture's old diagnostic premise that the constructor
  leaves problem_.owner==nullptr; a correct scoped owner intentionally invalidates
  that premise. Retain the actual product caller and independent debit equality.
- Same native six-entry validator census and cap1: unowned validation must refuse
  max_reforge_work before exceeding1; logical work records only executed units.
  Same exhausted shared owner stays refused, child0 and owner1, without fallback
  substitution. Budget48 should complete all six with debit48; budget47 should
  refuse before its disallowed unit, preserve its actual debit<=47 and remain
  unqualified. These boundary expectations derive from measured native work48.
  Also retain a live shared owner with headroom but a stricter validator ceiling:
  both ceilings apply and the parent receives each executed unit exactly once.
- Report cumulative generation+evaluator+validation work exactly once. Advance
  repeatedly, then finish/cancel/destroy on an actual capacity refusal; committed
  debit must not shrink or be charged again. Include evaluator-to-validator
  handoff with evaluator work already visible in remaining allowance. A genuine
  completed cache hit costs zero; force no artificial work by clearing a cache.
- A cold retry needing new work under the genuinely spent owner must refuse and
  leave incomplete query work unpublished; cancellation rolls back staged state/
  operator/cache mutations but does not refund consumed work. Preserve previous
  completed query/full membership and exact native operator identity.
- Pre-existing caller work and an attached stricter external owner remain intact;
  the new request reports only its own additional debit. Constructor failure,
  bounded finish and destructor restore the prior attachment and leave no child
  pointing at a freed owner. Capped proposals preserve any checked incumbent.
- Reuse weighted23/23 unchanged; no probability/resource law fix. After parent
  assigns LOCAL, use the canonical target and focused accounting cases first.
  Recovery's existing owned-path finite gates then check cross-consumer impact;
  public Finder/Current/WASM qualification remains separate. No test is run now.


## Recovery finite-r6 review at 8984f7c8

This is independent source/receipt review, not another native execution. The
owner's actual commit8984f7c8076ea152fdd38857602c1ad97ab9fde3 changes only the
prepared fixture, request and record; engine/src is byte-unchanged frombd3c5b3.
All seven recorded source hashes match the working files and pinned commit.
The two-job Tests build passed20.773s. Three serial native selectors passed:
151 compound-refusal +233 four-positive-fixture +84 legacy-growth =468 checks,
zero failures. All four supervisor receipts record exit0, no timeout/cancellation
or survivor. Exact compact receipts are owner-finite-r6-*; original raw evidence
is out/sol61-armour/finite-r6 in the owner's worktree. No heavy command ran here.

R4/P2 capability/routing defect, high confidence, disabled private producer:
solver_selective_completion.cpp:690-700 selects below-capacity Exalt from goal
presence/count predicates without carrier-local native eligibility. The canonical
compound carrier prefixes3/2, suffixes5/7 has modifier2's primary group13 and
additional group10. A native per-pick positive ordering has mass
0.00021919330543368763; independent complete original-root Chaos enumeration
finds exact carrier mass0.017031484841614736 (test_solver_compile.cpp:5344-5386).
Neither missing prefix goal is rollable. The composed controller nevertheless
routes c20 to held-mask8 Exalt, with zero native intent members.

The native operation is LEGAL and paid, not an action-not-applied failure:
solver_calc.cpp:3139-3144 self-loops when its dominant-side add pool is empty.
The actual setup+Exalt word has action/resource count2 and exit mass1, and every
exact exit equals the setup-only item (test_solver_compile.cpp:5440-5484).
Original-root evaluation has success0.79346875709748854 and unresolved
0.20653124290250982, covering the carrier mass; not-applied probability is0.
Root acceptance correctly refuses. The actual census contains99 entries and14
refusals; validator construction correctly rejects it at
solver_selective_completion.cpp:1065-1069. This does NOT establish complete
entry-by-entry validation of the compound controller. No invented complete
census, epsilon suppression, query widening or permission exception was used.

The four positive private cases pass original-root evaluation and actual every-
positive-entry validation, with145/145/145/79 entries. Their original-root native
costs are282.224787,282.189717985,274.525596464,274.390726972. This qualifies those
synthetic cases only, not the frozen Conquest request or whole consumer. The
selected-owned audit change is valid: solver_calc.cpp:2858-2862 rejects only
undercount; conservative excess8 is retained and printed. It is no measured
memory saving or aggregate peak qualification. The earlier142-check/3-failure
receipt remains preserved in the owner tree; the three premise corrections
changed no production semantics or numerical thresholds.

### Next candidate review obligations

No generic routing correction exists at this commit; review its actual diff when
parent supplies it. A repair must route the actual blocked positive carrier to a
native-valid paid continuation and retain the original root, goal, scope, price,
resource and exact occurrence identity. Do not reject every root whose families
can conflict or remove only the unwanted reachable mass. Carrier-local eligibility
must use canonical native blocking, including secondary groups and the exact
post-setup item, rather than counts or family-presence proxies. Do not globally
ban a no-immediate-goal-progress primitive: an independently admitted finite paid
continuation may use such a step. Its complete native law and properness decide.

Retain the exact compound native reachability and no-op law as controls. For a
claimed repaired controller, require original-root acceptance, a genuinely
complete actual census, and the SAME native validation for every positive entry,
including tiny visits; no synthetic census or altered tolerance. Preserve full
weighted entry/retry/conditional resource and cost expectations instead of
substituting a fixed word's structural counts. All paid setup, cleanup, recovery
and retries belong in that original-root evaluation. A retained native-programme
word must be admitted at its own reached carrier. Local query completion never
grants full-envelope closure. Cancellation and failed branches must preserve
committed work and earlier checked incumbents.

The two measured R2 accounting defects remain open. The prior correction plan
and exact regressions remain the implementation handoff; they do not authorize
recovery-owner production edits here. Current owned-path controls, Finder and
unowned/authored entry validation remain distinct. Public producer activation,
whole Current/Finder economics, frozen Bow4/Bow5/Ring/Amulet/armour controls,
checkpoint replay/full closure, aggregate peak accounting and WASM remain held
or separately unqualified. Recommend retaining this default-off research commit
and its finite evidence; do not release the producer while this positive routing
gap exists. Parent controls the next LOCAL grant; CI currently owns it.


## Guarded recovery source review at e669ddd7

Parent supplied e669ddd7249a6ea7638f7b51a9dfed3a99ee9f8b for review before its
native gate. All eight prepared source pins match the owner files and commit.
Read the actual diff, native pool/weight/group construction, exact condition
compiler/evaluator, side-preservation law and new synthetic/Conquest diagnostics.
No native build/test or production edit ran; CI owns LOCAL. The prepared request
and compact source review are owner-guarded-e669-request.json and
guarded-e669-source-review.json. No source-only statement here is a passing test.

R5/P2 capability blocker, high STATIC confidence, unexecuted native witness:
solver_selective_completion.cpp:680-687 sends the false rollability guard to the
same target-side EldritchChaos. This can never remove an obstruction retained on
the opposite side. Copy fixture4, REMOVE secondary group10 from modifier2
(primary13 remains), ADD secondary10 to modifier7 (primary21 remains), rebuild
the native group payloads exactly as the existing fixture does. Keep root,
weights, family/tier, prices, actions, goal, carrier prefixes3/2 + suffixes5/7 and
anchor5/held-mask8 unchanged. Ordering3,2,5,7 has positive native weights and no
chosen group intersection; obtain its exact root mass by fresh native pool and
full Chaos enumeration, not the old compound witness's numerical mass.

Modifier7 is a lower-tier, unsatisfied suffix goal-family member with group10.
All satisfying prefix-goal0 members are excluded while it survives. Prefix
EldritchChaos preserves the ENTIRE opposite side, including7, by
solver_reforge.cpp:313-329; prefix Annul cannot remove it either
(solver_calc.cpp:3332-3344). The singleton stage does not reject held junk when
requested_held_mask_ is nonzero (selective_completion:601-605). Its three target
goals cannot all complete, and suffix7 never upgrades, so neither final-large nor
full-small dispatch can escape (1040-1041). The current Exalt guard correctly
observes the blockage; its selected paid reroll fails to break this closed class.
The six prepared fixtures only put secondary10 on target-side2 (both mirrored
orientations), so they do not falsify this held-side obstruction. Correct root/
census refusal is expected; this is a capability gap, not an acceptance bypass.
No actual root mass, failure count or frozen Conquest counterexample is claimed.

Small exact correction for the implementing owner: BEFORE partial-stage count/
progress routing, determine whether some target goal has no positive native
member compatible with the retained side even after the target side is cleared.
For the existing immutable-tag domain, reuse the native empty-frame positive
members; OR full group conflict masks for each member and intersect those blocker
masks with the held-side native mask before existing exact count0 predicates.
Require some unblocked member for EACH target goal. Do not exempt currently
satisfied target goals: the prospective reroll destroys them. This is a necessary
escape observation, not proof that a joint completion law exists. If it fails,
route to the existing paid acquisition node (or separately admitted cleanup that
can remove the obstruction), retaining all resource and retry expectations.
Place this escape before progress tests: an obstruction may block every target
goal, so a guard reachable only after some target progress is insufficient.
Preserve EldritchChaos for the original target-side blocker, whose obstruction is
removed by that reroll; do not make every false guard discard the held anchor.

Add native witnesses for both obstruction locations and mirrored sides. Retain
original-root positive acquisition enumeration, legal paid words, complete root
acceptance and actual complete-census validation of EVERY positive entry without
an epsilon or permission change. Preserve the old unguarded rejection and six
positive controls. Parent must assign ownership/LOCAL; no new C++ fixture was
added here. Native gate is unrun and generic recovery qualification remains held.

The new weight/projection source itself has no demonstrated missing positive-
weight predicate in the reviewed domain: compile:38-46 takes actual native
pool members and final_weight>0, matching temporary:246-250. Static tag/influence
weights are immutable; full group masks give canonical symmetric conflict tests,
including secondary groups. Exact mod_count is compiled without family widening
and the evaluator observes its membership. Rarity/capacity are explicit. The
private clean root and closed setup/add/annul/reroll actions preserve the other
native legality premises; the guard is not authority for arbitrary foreign items.
Added tags conservatively choose paid reroll and remain unqualified. Original
prices, query membership, all-outcome dispatch and native entry validation remain
unchanged. Private outer gate is still false; public proposals/ABI/vocabulary are
not activated. The Conquest diagnostic remains construction-only and imports no
policy; its source-only route/resource plan grants no economics or whole-root
acceptance. Earlier R2 accounting findings and all release holds still apply.


## Persistent-blocker correction and native R8 review

Reviewed1cc4f8ffa48d2bfdb2f99116120300a9455e2905 actual diff; parent supplied the
completed owner receipt d452cef2 while review was active. No reviewer native run
or production edit. Eight runtime source hashes match the immutable tested commit
(and matched working bytes before the owner resumed accounting edits). All four
selector log hashes, complete Conquest graph/report hashes and three frozen case/
economy/manifest hashes/byte counts verify. Seven receipts report exit0, no timeout,
cancellation or survivor. Owner released LOCAL; CI owns it. Exact receipts and
source review are owner-*-r8-* and correction-1cc-source-review.json.

R5 disposition: corrected and qualified for the requested finite cuts. Compiler:
41-75 requires positive native membership for EVERY target goal after clearing,
including currently satisfied goals. Full canonical blocker masks are restricted
to the retained side; no current capacity or unsatisfied-slot exemption survives.
Selective_completion:637-647 tests compatibility before counts/progress. Compose:
997-1001 routes that specific false edge to original PAID Chaos in all stages,
including final stages, while retaining other refusal ports. Chaos preserves tier
identity; unsupported frames still face existing ports. Full original-root checking
remains mandatory. Every acquisition outcome returns to ordinary dispatch, with
costs/retries in original-root rewards. No probability/resource epsilon, native
law, price, root, goal, scope or public proposal changes. Removable target-side
blockers retain the narrower paid EldritchChaos route and useful held progress.

Fresh native R5 confirmation: exact original-root carrier mass0.018899252609993832,
one-ordering0.00018729344839751061. With only the new escape disabled, both old
mirrored controllers leave0.84001882298508312 unresolved and refuse the actual
incomplete census. Enabled carriers select requested Chaos at synthetic price100,
full mass1.0000000000000002 within unchanged1e-12. Both original roots accept and
validate77 actual positive entries each. Fully paid costs1058.41163574 and
1058.40531371 preserve the expense of lost progress; fixed structural word counts
do not replace weighted original-root rewards.

R8 passes5944 checks:151 original rejection +322 retained-side old-routing
rejection +5387 corrected recovery +84 legacy growth. Eight accepted roots cover
878 programme entries (145/145/145/79/105/105/77/77). All four compatibility nodes
are compared with the actual native pool AFTER clearing the target side on every
positive census entry, with positive final weights and every target goal included.
The unchanged validator accepts the complete actual census; identity tamper
negatives remain. Prior six costs/counts are unchanged. These are scoped finite
results: per-slot compatibility is necessary, not joint-completion authority;
dynamic added tags remain conservative and unqualified.

Frozen Conquest ordinary construction passes: generated held-mask8 c21 executes
Ember1 plus EldritchExalt from a positive original-root goal-mask9 carrier, with
native ordering2.4457291281364614e-9. Two paid actions/resources,33 exits, mass
1.0000000000000002, all retain held progress. Complete generated graph36069 bytes,
SHA-2567f2c12941904762444da1ecf95ef77f8456e2ab170cd74465c5aba50bbd4461e;
source/report/frozen inputs/binaries are pinned. No graph import or broad-search
permission. This proves construction/word behavior only: Conquest full-root,
every-entry and economics remain UNRUN. Reserved peak283828260 includes201589248
transient reservation and proves no audited peak or memory release saving.

No new blocker found in this correction's reviewed finite cuts. Retain default-off
source and evidence; hold public activation, Conquest retention/economics, Current/
Finder qualification, checkpoint/full closure, final frozen control cohort and
WASM. R2 accounting defects remain open; the parent's separate accounting/lifetime
candidate needs actual-diff review. Nonblocking cleanup: unrelated old em dashes
at test_solver_compile.cpp:2362-2363 became mojibake; restore before integration.
No review-owned production fix or new fixture was made.
