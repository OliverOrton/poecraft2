# Approved ordinary reforge count law

Oliver selected this global correction on Sol 6.1 at 18:43 UTC on 2026-10-02:
“also fixing chaos is good, that should be for all reforges 8:3:1”.
The parent confirmed ordinary variable 4/5/6 reforges use 8/3/1 throughout
crafting, Calculator and solver transitions; explicit fixed counts remain fixed.
This implements the owner-approved model, not empirical proof of a game law.

Base: dce327101d73117146b09908baea3cb0690c046f. Isolated local branch:
`dot/reforge-law-20261002`, sibling `poecraft2-reforge-law`.
Local main had advanced to b04f359 at execution start; no unrelated change imported.
Reviewed-status snapshot ae24e0b is historical; relevant source was read at dce3271.

## Ownership and validation map

- `reforge_count_law.hpp`, `engine_internal.hpp`, and the session count-kind
  initialization own a shared integer law. Equipment: 4/5/6 with 8/3/1 over 12.
  Cluster interface: 3/4 with 65/35 over 100; its separate owner must select
  `RareReforgeCountKind::ClusterJewel` in configured sessions with side cap 2.
  Ordinary/Abyss jewels preserve their unresolved legacy law and require an
  independent owner ruling. This branch does not activate cluster crafting.
- `actions_basic.cpp` uses the shared integer selector for every ordinary
  variable rare path: Alchemy, Chaos, Essence, Fossil, Harvest Reforge,
  Veiled Chaos, tied/absent-dominance Eldritch Chaos and Awakener refill.
- `solver_reforge.cpp` owns the weighted mixture for raw/projected/factored
  exact rows and concrete retained refill. Calculator, Current, Finder,
  authored evaluation, compound operators, retained/forced kernels, lower
  row consumers and final verification consume that owner. Fixed-six Vaal
  remains explicit; dominant Eldritch 2/3 and magic 1/2 remain unchanged.
- Veiled Chaos clamps total capacity before reserving one placeholder, matching
  native order. Count probabilities coalesce on capacity/retention bounds;
  exhausted pools absorb their complete mass without conditioning on attainability.
- Law version 2 enters reforge memo/signature, phase-lower, quotient and
  continuation/artifact identities. Refinement contract 4, entry/continuation
  evaluator 2 and development checkpoint format 5 invalidate old-law authority.
  Saved authored graph vocabulary remains v1 and can be freshly evaluated.

Shared files overlap cluster, Foulborn and Dominance work: engine_internal.hpp,
session_builder.cpp, actions_basic.cpp, solver_reforge.cpp, test_main.cpp and
solver tests. Integrator must reconcile the count-kind field rather than add
another sampler/evaluator law. External thread messaging is unavailable in
this executor; this record is the early coordination handoff.

Finite validation uses independent literal-weight ordered enumeration, all
three evaluator modes, clean/extra goals, forced Essence/Fossil, native Harvest
first draw, fractures, locks, capacity clamping, early exhaustion, fixed-six
refill/cache isolation, native count-frequency support and saved graph migration.
Two-job builds only. No long solve was launched; heavy output slot remains the
Foulborn owner's. Frozen data/economy and protected root 0 are untouched.

## Tested native checkpoint

[Finite qualification](qualification.json) records **1,352,300 checks, 0 failures**:
count law 209,934; independent gated rows 1,111,656; checkpoint 51; phase/lower
5,940; retained metamods 685; uniform removal 23,202; protected Finder 66;
Finder Essence 293; assertion service 424; Current proof handoff 49.
The final two-job Tests build passes. Engine/test file hashes and the actual
executable hash identify the checked source independently of its local commit.
The receipt also records a subsequent header-only CRLF-to-LF normalization;
the checked header hash is retained and semantic validation is reused.
The final checkpoint and occupancy fixture changes are test-only; preceding compatible tests
are reused without repeating unchanged production checks.

Native frequencies use 12,000 draws for each of six ordinary paths, plus 1,000
Veiled-clamp draws. These support native sampling parity and do not establish
empirical game probabilities. Independent saved-graph evaluation retains the
same v1 Chaos-six retry graph, with properness/success one under both mixtures:
old uniform cost 3c, approved law cost 12c. Its success event is all six
independent modifiers (three prefixes plus three suffixes), so per-roll success
falls from 1/3 to 1/12 and the geometric mean at 1c per roll increases from
3 to 12. This does not predict a universal 4x real-item cost change. No graph
vocabulary migration needed.
Old-law format 4 and a mismatched class-law payload are refused; a new compatible
checkpoint round trip passes. Fixed-six concrete refill cannot contaminate the
ordinary row cache. Clean and extra goals remain separate tested contracts.

Initial fixture syntax/carrier, missing bench metadata, compile errors and
checkpoint setup/copy failures are preserved in `out/reforge-law/validation`.
The ignored compiled artifact is not present in the fresh worktree; real-data
finite checks read the existing frozen main-workspace artifact. A dedicated
TEMP/TMP namespace avoids other owners' temporary checkpoint files. All three
artifact hashes are recorded; no data/economy mutation occurred. No owned solver
or test process remains. Full acceptance, DLL/Python, WASM/worker and timed
real-data qualification are unrun; historical acceptance is not transferred.

## Retained occupancy and recovery impact

Native side locks preserve the pre-action side; the lock itself, if on the other
unlocked side and not fractured, is wiped. Refill uses the new total, preserved
mods and guarantees. With two held prefixes an open third prefix slot can still
receive a new modifier. The tested guaranteed-other-side path is Harvest Fire:
its sole eligible Fire suffix is added before count/fill. Essence and Fossil do
not preserve ordinary side locks under their existing native contracts; they
retain fractures and retain their own direct-mod obligations. This branch does
not reinterpret those action guarantees.

With complete pools, two/three held modifiers both finish at total 4/5/6 with
8/12, 3/12, 1/12 mass. Draws mean **total**, not additional modifiers:

| Held | Guarantee | Other random additions at targets 4/5/6 |
|---|---|---|
| 3 | none | 1 / 2 / 3 |
| 3 | one other-side mod | 0 / 1 / 2 after the guarantee |
| 2 | none | 2 / 3 / 4 |
| 2 | one other-side mod | 1 / 2 / 3 after the guarantee |

For three held prefixes, suffix saturation (no room for another suffix metamod)
drops from 1/3 to 1/12, whether the tested suffix guarantee is present or absent.
A two-held case can saturate suffixes at total five while one prefix slot remains
open. In the explicit unequal-weight fixture (remaining prefix weight 100 and
suffix weights 100/100/400), without a guarantee that event drops 139/315 to
23/140 (44.127% to 16.429%). With the weight-100 Fire suffix guaranteed first,
it drops 22/45 to 1/5 (48.889% to 20%). These are finite fixture values; the
two-held side split depends on the actual pool, so these percentages are not
base-independent game claims. Six-total mass in complete pools drops 1/3 to
1/12; the mean target decreases from 5 to 53/12.

Sparse-pool cases preserve all mass: three held plus only two available suffixes
finish at total four with 8/12 and total five with 4/12; six is unreachable.
Two held plus those two suffixes finish at total four with mass one. With only
one available suffix, both guaranteed and non-guaranteed cases finish at held+1
with mass one. The guarantee-success/exhaustion case does not redraw a count or
condition away the unattainable targets. All held prefixes survive; the unheld
lock is absent afterwards. These assertions use independent finite ordered
native pool enumeration and the same paths repriced under old/new count weights.

The correction reduces this saturation/recovery obstacle in the tested carriers.
It also shifts goal acquisition and cleanup probabilities. Only paired full
policy evaluation can establish the resulting real-item economic effect.

## Proposed bounded source-matched requalification — unrun

After staged integration and release of the Foulborn heavy slot, the integrator
can freeze one source/build (including this count-law interface and reconciled
cluster changes), produce matching native/DLL/WASM artifacts, and migrate the
four pre-existing ABI2 corpus runtime pins to the actual ABI3 through their owner.
Do not modify historical result receipts or disguise that migration as law proof.

Propose **two existing representative requests**, each once native and once via
the actual Calculator worker, serially: the Normal-root BodyInt17 ES Current
Alchemy/paid-Scour witness, and the existing clean natural-T1 Bow Finder
Essence/lock/Scour request from the overnight worker preflight. This is four
prospective search/worker invocations, not an inherited or renewed allowance.
Preserve each original root, goal, prices, complete scope, caps and normal worker
controls; preflight exact resolved comparison identity with existing corpus/Lab
and worker owners. Add a finite Calculator primitive 4/5/6 odds smoke in that
matching module. Keep no-policy periods, censoring, failed status labels and
responsiveness separate from checked cost/properness.

First independently evaluate each preserved old graph under both the preserved
old-law native build and the new-law native build, changing only the count law
and recording each exact source/artifact identity. This isolates mechanical
repricing and occupancy/recovery changes. Then compare freshly found policies
under the new law with those same old graphs freshly evaluated under the new
law. Preserve root, goal, prices, complete action scope and deterministic work
budgets across search comparisons; report wall caps and elapsed times separately. Reuse graph structure only
where its native action/control/goal carrier still passes fresh admission;
refuse unsupported policies explicitly rather than inventing a continuation.
Use the existing original-root exact evaluator, preserving full success,
off-policy mass, paid recovery and reconciled consumption/prices. Mechanical
cost shifts are not pure search improvements versus old-law receipts. Any search
improvement must compare old/new graphs freshly checked under the same new law.

No new timed batch is launched here. Integrator
01a0fd2f-7f88-7588-a1ea-bfe1e6991036 owns review, runtime-pin migration,
combined qualification and qualified push. Cluster session activation and
unresolved ordinary/Abyss jewel laws remain with their stated owners.
