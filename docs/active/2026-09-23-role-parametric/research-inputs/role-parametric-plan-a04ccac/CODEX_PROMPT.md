# Role-parametric continuation search for non-identical goals

Repository: OliverOrton/poecraft2
Reviewed main: a04ccac571573e4aca2403113d2a47c903da8af8
Selected programme: R0–R5 below. This is the complete implementation prompt.
Earlier conversations, home-directory attachments and sandbox files are not
required to begin. The accompanying research expands the derivations and cases.

## 0. Owner request and baseline

Oliver's central idea is a strategy abstraction that treats goal modifiers as
roles: preserve acquired roles, acquire a missing role, choose useful progress,
and finish/recover. The PRIMARY case is non-identical modifiers with different
numbers of tiers, different weights, prices and native effects. Test three T1
bow prefixes, the same with one/two suffix goals, and asymmetric armour triples
with the corresponding suffix extensions. Exact symmetry is a control, not a
requirement. Do not conclude the programme failed merely because weights differ.

Current memory work is documentation/evidence only. Main at a04ccac follows two
commits after compact-step runtime `1596e23`. In Ring-two, the final temporary
terminal-frontier/comparison-lifetime trial went from 58 states/447 rows to 186/
2497, no memory cap, but returned the same C149977.25092497544 graph. All temporary
source was restored. Do not automatically reapply it or repeat that experiment.

The compact Calculator release's C4 recovery is preserved at C3746.1319409485764.
Oliver accepted its measured 2.643-second maximum ordinary call for the quality
gain. The 250 ms gate still fails; this acceptance is not unlimited latency waiver.
There is no need for another transport, batch-size or general structural campaign.

Follow AGENTS/HANDOFF and inspect relevant local changes before editing. Reconcile
later main/local work instead of overwriting it. Sequential work, no subagents,
automatic restart, inherited deadline, destructive cleanup or push. Do not inspect,
stage, edit or delete protected root `0`. Preserve unrelated work. Native C++ owns
mechanics; do not fetch/invent PoE rules or edit canonical/compiled data. Keep
original expected Chaos as objective, primitive count as separate reward, and all
probability/pricing/observation/authority contracts. Reuse existing runner/Lab/
worker/compiler/evaluator/portfolio, one living record and short HANDOFF.

## 1. Relevant owners and pre-existing functionality

Read the capacity README and P3-C receipt under
  docs/active/2026-09-23-capacity-capability/
then relevant sections of:
  docs/foundation/tooling.md
  docs/solver/mathematics/representations.md
  docs/solver/mathematics/policies.md
  docs/solver/resources-resume-replay.md
  docs/solver/upper-authority.md

Inspect existing code, not an invented duplicate architecture:
- solver_model.hpp: FixedOptionSpec already has side, real programme actions,
  exit_goal_slots, exit_min_satisfied and carrier_goal_slot.
- solver_options_helpers.hpp: synthesize_automatic_options already binds goals,
  Eldritch sides/setups, temporary blockers and protected repetitions. Blockers
  already group by exact eligible-pool effect. The inspected ProtectedRepeat
  loop emits singleton target slots; verify whether equivalent set-valued
  continuations exist elsewhere before declaring coverage missing.
- solver_options.cpp: complete kernels already memoize after matching planner,
  resources and same_complete_option_kernel. Shape likeness cannot weaken this.
- solver_options_automatic.cpp and current private/return-bridge owner: transactional
  native admission, whole-positive-support construction and actual graph-local
  decision boundaries. Existing backward observation machinery is not absent.
- calculator-delivery-probe.ts and test corpus loader for actual product checks.

The removed old carrier planner had descriptor-only testing and failed native
mass correspondence. Do not recreate it as a role layer with no real consumer.
A new internal type must serve a concrete native construction/checking path.

## 2. Mathematical and semantic contract

A role binding b supplies the ACTUAL native family/member/tier partitions, full
background pools/conflicts, original prices, item/control state and action scope.
Use variable-length tier/weight vectors. State-dependent pools must be queried
at the real carrier; a root fingerprint is not a constant probability for all
later states. Keep below-tier blockers, crafted/fractured roles and suffix effects.

Distinguish:
1. Exact fixed-target state/action symmetry: requires native goal, legality,
   observations, costs/resources and mapped successor laws to correspond over
   the full represented member domain.
2. Common parameterized structure: same construction/decision skeleton, different
   coefficients and potentially different optimal actions.
3. Proposal-only reuse: analogous plan, independently grounded and checked.

Under complete coherent relabeling, V_(sigma theta)(sigma s)=V_theta(s).
This does NOT imply V_theta(sigma s)=V_theta(s) for a fixed asymmetric theta.
Keep shape identity, native-law/binding identity and value/certificate identity
separate. No averaged probabilities, universal below-tier buckets, old values,
parent/private IDs or certificates copied because shapes match.

For a finite proper instantiated controller:
  V_b=c_b+Q_b V_b; N_b=n_b+Q_b N_b.
Its costs/decisions are recomputed per binding. Equal topology need not mean equal
coefficients, support, cost or optimum. Parameter values that remove a positive
exit can change properness. Complete original-root native evaluation remains
acceptance. A restricted family optimum is only an upper to the full target,
not an admissible lower or omitted-action retirement.

A useful exact subcase is multiple stopping queries on ONE unchanged physical
attempt. If carrier, actual programme, observation timing and cleanup are the
same, reuse its complete joint law and classify outcomes differently for each
role set. Unequal role probabilities are fine. If targets change the actual
operation, intermediate stop, layout or control, the premise must be reproved
or separate native construction is required. Existing outcomes caches may
already do this; establish the real additional saved work.

For invented disjoint outcomes p_i, fixed complete tails u_i with no reentry to
the changed stage, attempt cost c and an IDENTICAL zero-extra-cost retry endpoint,
accepting set T has
  J_T=(c+sum_T p_i u_i)/sum_T p_i.
With c1,pA.1,pB.4,uA5,uB8: A-only15, B-only10.5, either9.4.
With uB100 the extra outcome is not worth accepting. Added exit j helps iff
u_j<J_T under those hypotheses. This is motivation, not a native shortcut:
if rejecting i costs r_i and neutral failure p0 costs r0, include
sum_(i not in T) p_i*r_i+p0*r0 in its numerator; the addition criterion becomes
u_j-r_j<J_T. Different return items or tails revisiting the edited decision need
the complete coupled vector law. Simultaneous goal hits must
be disjoint exact attained-subset outcomes, not summed marginals. Missing tails
stay unknown. Never return a partial success-only law.

## R0 — Native data and one real existing-machinery delta

Use existing case derivation and fingerprint/specification owners. Derive/validate
these six intents before expensive solves (same item level 86 empty Rare, no implicits):

B3 Spine Bow (`Metadata/Items/Weapons/TwoHandWeapons/Bows/Bow20`):
 LocalAddedFireDamageTwoHand10
 LocalAddedColdDamageTwoHand10
 LocalAddedLightningDamageTwoHand10
B4 adds LocalIncreasedAttackSpeed5.
B5 also adds LocalCriticalStrikeChance6.

A3 Conquest Lamellar (`Metadata/Items/Armours/BodyArmours/BodyStrDex20`):
 LocalIncreasedArmourAndEvasion8
 LocalIncreasedArmourAndEvasionAndStunRecovery6
 LocalBaseArmourAndEvasionRating8
A4 adds ChanceToSuppressSpellsHigh5___.
A5 also adds AdditionalPhysicalDamageReduction5_.

All min_tier 1; require all listed slots. Validate these candidate suffix IDs,
actual side/member/level availability and joint satisfiability. A4/A5 correspond
to core CB02/CB01 targets when complete identity agrees. Reuse, do not rewrite,
those frozen cases. Derive the others through the same owner, updating goal and
envelope_goal together. An old Restart-enabled tri-elemental C79273 controller
and old non-elemental Bow C12770 result are not new B-case baselines.

Preserve current native exact terminal cleanup semantics, ordinary Restart-off
and Imprint-off, all action-owned paid replacement, frozen economy/base price,
original modifier universe and actual product action filtering. These are six
explicit test choices, not invented existing qualified fixtures.

Retain actual ragged tier/weight vectors, satisfying/member masks, full native
pool/conflict context and the action/programme realizations. Names ending in6/8/10
are not tier-count measurements. Require a demonstrated asymmetric armour case;
if these data do not supply it, report that before selecting an additional native
triple by an outcome-independent rule. Do not modify weights to create one.

Inspect a few legitimate diagnostic carriers (missing one/two goals, below-tier,
with suffixes). Compare actual generated specs/complete semantic candidates.
Find one missing multi-role stopping/continuation family, repeated structural
construction, or useful binding denied real service. Do not call an existing
loop over slots new capability. If all are covered, provide the precise negative.
Failure of exact numerical symmetry is NOT a stop for structure reuse.

## R1 — Small role/binding layer with an actual native consumer

Represent only the selected family: roles, native binding, preserved/unresolved
sets, opposite-side requirements, capacity/control facts, native programme shape,
stop alternatives and exact continuation endpoints. Keep numerical law and
certificate ownership with existing native owners. No universal DSL, symbolic
parameter-space solver, persistent recipe library, dynamic quotient or extra
coordinator/supervisor.

Materialize two unequal native bindings through that one structure. Permit their
best target/acceptance/order to differ after actual pricing and probabilities.
A missing native action mapping is unsupported, not a fabricated mechanic.

Where multiple queries use one identical physical attempt, reuse it under its
complete dependency key and union of required observations; specialize stopping
and continuation separately. Measure the union-layout cost and check whether
current memoization already eliminates the work. Never use shape as kernel key.
Fresh private layout means fresh IDs and exact physical entry, not transplanted
rows/vectors. Role state alone need not be a sufficient native state.

## R2 — Bounded progress-frontier/complementary-continuation pilot

Leading candidate: preserve singleton repeat targets and consider pairs of up to
three unresolved same-side roles using the existing multi-slot exit vocabulary.
At most three extra pairs per relevant follow-up initially. Accept a useful
attained subset only at a native observable decision boundary and route its
actual members/control to a separately complete tail.

Compare continuation of acquisition versus retaining a different useful role and
finishing the remainder. Include at most two logical completion stages initially,
using existing graph composition, not an unauthorized longer fixed programme.
Do not assume target-specific Harvest or protection exists for each role; bind
only native admitted/priced capabilities. Do not infer a free same-side lock.

If R0 shows this stopping family already exists, implement only the concrete
structural-reuse/service delta it identifies. Do not add a second speculative
candidate family. If no such delta exists, stop before unused scaffolding.

Every positive outcome retains true success, complete compatible old continuation
or new paid recovery. No skipping inconvenient junk, loss, cleanup, required
suffixes or fracture replacement. Old entry upper is usable only at its certified
entry, not as a uniform tail for all output members. Preserve native choice timing
and mandatory interiors. Complete independent original-root checking decides
acceptance; cheaper source estimates do not.

Use the existing bounded candidate owner. Start with one added family wave per
relevant semantic entry generation and at most three selected whole-candidate
checks. State the exact binding/entry count and omitted options. No perpetual
rearming on unrelated rows. A second wave requires new verified relevant evidence,
a demonstrated need and a maximum of two within unchanged total limits.

Keep first-policy service and cheapest compatible verified artifact. Ordinary
full-scope lower/proof obligations are unchanged. No execution-count activation,
price weight, disabled-family claim or capacity change is silently introduced.
Qualify actual ordinary product activation separately from native diagnostic use.

## R3 — Attribution and proportional qualification

Separate existing ordinary baseline; the same new family explicitly grounded
without structural sharing; and role-shared construction of that identical
candidate set. Use small native binding controls for the latter comparison,
not three full campaigns. Compare operations, full probabilities, choices,
resources, counts and actual decisions. Suppress only truly identical complete
semantic candidates, never differently parameterized options by shape.

A3 is the exposed heterogeneous design case; switch to A4 only if its eligible
native boundary is required, based on R0 not outcome cherry-picking. B4 is the
predeclared second application. B3 is a relative control and A5/B5 are mixed-side
extensions, not claimed blind holdouts.

After a useful native witness, qualify applicable six intents once serially with
compatible baseline reuse and explicit incomplete cases. Do not rerun every long
solve after each patch. New policies require independent original-root evaluation.
No fresh Simulator for unchanged graphs; owner-approved 1,000 trials only when
needed, with censoring retained. Build native/WASM and test affected vocabulary,
options, compiler, evaluator, Finish/Cancel and ownership paths as appropriate.

For new cases the actual Calculator probe may need one explicit optional corpus
path using the existing validated loader; retain its old behavior by default.
Do not clone its execution stack or change inputs outside the frozen request.

Keep product profile: 240-second Finish, 300 native, 315 host, 1 GiB aggregate,
50M declared parent work and original other caps/checker inheritance, adaptive
max 8, compact transport. Total parent/child/compiler/evaluator overlap is inside
the same cap. Wide Ring/Bow profiles are separate evidence, not performance claims.

Material policy goal: >=20% cheaper complete original-cost controller on one
verified asymmetric full-root case and a qualified second structural application,
with cheaper incumbent preservation. Alternative reuse goal: >=25% less declared
native construction work at same coverage/preselected verified result, without
worse all-in time or memory outside measured noise. Label coverage, sharing,
economics and exact closure separately. A symbolic descriptor or symmetric-only
win does not satisfy the broad request. Newly finite output is a separate result.

Preserve ordinary C4 C3746.1319409485764; C5 first-policy C85558.70618560436;
Regalia exact C65.60036144971359; Ring2 C149977.25092497544 and bounded status.
Affected wide Bow4 C12770.827062219498 is a different target. Do not silently
substitute it for a new elemental case. Require no regression of setup, complete
cancellation/Finish, original certificate authority or retained stronger policies.
Keep accepted C4 latency tradeoff explicit; new material regressions need a decision.

Test unequal tier lists/weights, price changes, absent action realization,
overlapping goals, group conflicts, below-tier blockers, fractured/crafted roles,
suffix loss, simultaneous hits, hidden observation, missing tail, trap, stale key,
partial row, cap/cancel and more-expensive candidate rejection. Label-rename tests
compare legitimate mapped semantics, not blindly identical byte graphs or bounded
search timelines. Timed improvement claims get one necessary counterbalanced
confirmation. Preserve native simulation/reference independence.

## R4 — Canonical knowledge and structural discipline

Import research once and give every material finding a disposition. Extend current
mathematics, not a second manual:
- representations.md: role-parametric structure versus exact fixed-target symmetry;
  shape/native-law/value identities; variable-tier binding; coherent relabeling.
- policies.md: continuation-sensitive progress-frontier formula and exact-return
  assumptions, joint outcome partition, shared physical attempt queries and
  complete-composition authority. Link existing stopped-law/properness results.
- resources-resume-replay.md / upper-authority.md / publication.md: actual owned
  structure, invalidation, shared attempt/accounting and checking boundaries.
- scheduling-bellman.md: actual service and deferred-versus-retired distinction.
- research.md and claims.md: scoped applications/negative evidence; preserve broad
  open statuses and prior memory outcomes. No new theorem ID for a generic loop.
- source/tooling map: actual consumer and any probe-manifest extension only.

Do not create a giant method or many fields on Impl for a self-contained family.
A local structure must own real state and replace/serve an actual path. Record
all-in construction/checker cost, not only shape-cache hits. No uncharged cache,
full telemetry per step, extra supervisor or phase-local counters relabelled totals.

Run existing knowledge lint against the reviewed base and focused edited-link
checks where supported. Lint is not a proof; documentation-only work needs no
native build. The package's abstract tests are not native mechanic validation.

## R5 — Stop with a clear decision

Stop after two substantive failed variants of the same premise without a new
native witness. Do not rescue an ineffective role family by silently adding the
reverted memory patch, broader caps, a response cache, lower algorithm or trained
model. If native grounding/checking still dominates, name that next boundary.

Final handoff: actual source/build/request/economy/mode; native asymmetric vectors;
current baseline and generated family delta; structural versus numeric reuse;
policy graphs/evaluations; full positive support and primitive resources; time,
memory and omissions; failed/unrun tests; canonical anchors and real commit/push
state. No generalized exactness claim from one example. Return a usable text
handoff and stop. No push or automatic successor is authorized.
