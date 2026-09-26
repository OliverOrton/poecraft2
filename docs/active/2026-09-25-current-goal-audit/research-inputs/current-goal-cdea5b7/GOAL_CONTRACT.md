# Native goal contract: minimal diagnostic implementation and future product shape

## 1. Selected implementation boundary

Use one small engine-owned goal/assessment component consumed by the existing calculator, Current solver and compiler. Do not add a second solver or maintain a parallel success predicate in TypeScript. Type and file names below are proposed; adapt naming to current owners.

A minimal internal representation is:

```cpp
// PROPOSED NATIVE DESIGN, not existing API.
enum class ExtraExplicitPolicy { ForbidUnmatched, Allow };
struct InclusiveCountRange { uint8_t minimum, maximum; };
struct GoalTerminalConstraints {
    ExtraExplicitPolicy extras = ExtraExplicitPolicy::ForbidUnmatched;
    std::optional<InclusiveCountRange> prefixes;
    std::optional<InclusiveCountRange> suffixes;
    // Normalise minimum-open requirements to counts using actual native caps.
};
```

Keep existing requested slots, native tier/member semantics, threshold and rarity in `GoalSpec`. Default construction and existing v1 JSON retain `ForbidUnmatched`. The private benchmark diagnostic can resolve constraints before building any CalcContext. Do not invent a public v2 parser, frontend default or ABI field as an incidental part of this programme.

An engine-owned `GoalAssessment` exposes current rarity match, the full satisfying mask, covered count, per-side matched/occupied counts where justified, requested coverage, occupancy satisfaction, unmatched count where proved, and final success. This is a passive recomputation from the current native state, not a cached “ever acquired” flag. Keep the existing native member/slot resolution; do not assume arbitrary overlapping group goals are disjoint.

For the selected all-required A4/A5 family, native mask-disjointness, exclusivity and side checks establish that one satisfied slot is one occupied explicit affix. Other domains must retain their established contract or refuse unsupported count normalisation; do not redefine their matching semantics accidentally.

## 2. Three arms

### L: legacy clean

Use the original rule `C && n == g`. Preserve the current byte-stable emitted condition for legacy callers where possible. Native lower, candidate and identity behaviour is unchanged.

### E: explicit occupancy, same clean target

For A4, retain all three requested prefix families and the requested suffix, all required. State `prefix_count == 3 && suffix_count == 1`, with extras otherwise permitted. For A5, use `prefix_count == 3 && suffix_count == 2`. The values 3/1 and 3/2 are derived test inputs, not special rules in runtime code.

After validating the native disjoint/side domain, coverage forces at least that many explicit affixes on each side. The count equalities leave room for no extras. Hence E and L agree on every valid item in the declared domain. Prove/test this before calling it a same-model treatment.

The preferred normalisation recognises this equivalence and produces the same semantic terminal identity and legacy-clean capability trait. Preserve authored syntax separately for display/audit. If an equivalence cannot be proved, keep distinct identities; do not force hashes equal merely because a test sample agrees.

### R: coverage only

Set `extras = Allow`, with no extra occupancy restrictions. R is a different optimisation problem. Its zero boundary includes all covered dirty states. All contracts, compiled success guards, mode labels and proof identities must use R consistently.

## 3. General constraints: what is safe to promise

A requested minimum of two open suffix slots means `suffix_count <= native_suffix_cap - 2`, using the requested final rarity and actual native base/session capacity. For the all-required A4 target on a three-suffix-cap domain this combines with required suffix coverage to imply count one. Do not treat an open-slot bound as an exact count in general.

For threshold goals `m < number_of_slots`, legacy clean accepts `n=g` for any achieved `g>=m`. Fixing `n=m` is NOT equivalent. Three listed goals with a threshold of two allow all three clean requested affixes under the legacy contract. The same warning applies to side allocations that vary by which threshold subset is satisfied.

Count all explicit affixes under the actual native count contract, including temporary crafts and below-tier requested-family members. Implicits remain outside the legacy cleanup rule. New implicit, socket, influence or quality constraints are not selected.

Different expression syntax need not create extra state dimensions. Prefix/suffix counts and slot status already exist. A more explicit goal object is not inherently a smaller SSP; any speed or quality effect must be measured separately.

## 4. Identity and ownership

Include the complete resolved terminal contract in every goal-bound identity: calculation/solver context, goal-gated row cache, resumable work, proof-potential scope, quotient session, original-root check, retained policy, report comparison and compiled provenance. Reconstruct child/private contexts from the complete GoalSpec; audit hand-built copies that currently copy only slots and rarity.

Price-independent physical transition payloads may be shared only if their native mechanics and observation dependencies really do not depend on the target. Goal-dependent retry normalisation, terminal absorption, option exits and value/evidence caches cannot share across L and R on an action name or old goal hash.

No process-global/thread-local terminal switch. Two simultaneous or sequential handles with different terminal contracts must be isolated, including reprice, abandon, resume and handle reuse.

Use fresh contexts per experimental arm. Do not mutate a live terminal contract after rows, values or certificates exist. A diagnostic override must be resolved before construction and frozen in the request record.

## 5. The compiler and independent checker

`exact_goal_condition` currently constructs an OR over occupied counts combined with at-least-count requested predicates. Keep that code path for L and proven-equivalent E. R emits rarity and requested threshold without the global extra-affix restriction. General explicit occupancy is conjoined through the existing native prefix/suffix-range vocabulary.

Every actual success ingress still tests the declared original contract. Effective default-edge semantics stay protected. Do not use auxiliary predicates collected from intermediate graph conditions as if they were the original requested goal.

The generic evaluator checks a supplied programme; publication additionally binds the programme to the original problem. Its goal/observation model and native macro exits must not be inconsistent with the emitted graph. A loose success is not a successful clean check. Different modes must be visible in graph description and evidence identity without relying on the display name as authority.

## 6. Candidate initiation is not terminal truth

Do not mechanically relax all clean-entry checks. Some describe a native programme's real initiation domain or a proof's valid projection. Classify each occurrence. Update a guard only where it encodes final target semantics rather than necessary mechanical/observation conditions.

Preserved-side policies need actual useful holdings, capacity and complete outcomes. “Covered but dirty” does not grant free cleanup or imply that one fixed programme is useful. Nonterminal assessment can expose acquisition and occupancy separately while retaining all native blocker/junk distinctions.

## 7. Public compatibility

The existing parser accepts only v1 (or its existing omitted-version behaviour). Historical pre-boundary requests also say v1; version alone cannot recover their old target. Preserve today's meaning for current v1 defaults, and require historical source/semantic identity for old replay rather than pretending that v1 has always meant the same thing. The future product design is in [PRODUCT_DECISION.md](PRODUCT_DECISION.md). This programme does not silently make existing saved v1 goals loose. Unknown new goal fields/version must not be accepted and ignored by a supposed public feature. New public support requires an explicit version/capability and migration decision.
