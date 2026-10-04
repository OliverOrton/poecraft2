# Independent solver review checkpoint (2026-10-04)

**SOURCE REVIEW ONLY; HOLD QUERY REPLAY QUALIFICATION.** No native build,
test, solve, browser, WASM, data refresh, merge to main or push was performed.
No LOCAL slot was granted. The review worktree starts at main
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

### R3 / P3: the general attempt helper's uniform-terminal shortcut loses nonunit entry mass

**Confidence: source-proven local algebra; production nonunit reachability not
established. Consumer: internal weighted attempt helper; current ordinary
Imprint discovery supplies unit-mass support. Inherited, not a query regression.**

`execute_attempt_cooperatively` accepts weighted `entry_support`. A one-step
supported/legal deterministic word with entry weight 0.125 returns weighted
resource/action rewards 0.125, but `solver_options_helpers.hpp:1645-1652` replaces
its already weighted exits with the unscaled unit kernel. The expected exit mass
0.125 becomes 1. The shared single-action reforge shortcut similarly assigns
unscaled exits at lines 1451-1476. The newly retained growth fixture checks
weighted rewards only (`test_solver_compile.cpp:4790-4798`), so it cannot detect
this mismatch.

Small witness for a future parent-LOCAL fixture: a one-step legal Bench from one
entry weighted 0.125; compare every returned exit to direct native K(s,t)*0.125,
not another helper using the same shortcut. Also exercise two compatible entries
whose total is 0.125 for the shared reforge path. Either enforce an explicit
unit-mass-only helper precondition (and separate the weighted contract), or scale
exits consistently. This is not evidence of a reachable production solver error
or a reason to re-open prior Imprint results without a production counterexample.

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
