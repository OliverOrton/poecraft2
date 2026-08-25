# Gate 3 — Carrier-Aware Admissible Bounds

**Result: passed on 2026-08-24.**

Parent: [Carrier-Aware Proof Bounds And Consumers](../plan.md)

## Retained proof

The existing clean-carrier goal MDP now has a carrier-aware relaxation over
rarity and exact satisfied-goal mask. It makes the concrete problem easier by
removing junk for free, preserving every satisfied goal/fractured carrier
feature, and granting favorable blocker and side shapes, while retaining exact
rarity legality. Existing optimistic pool probabilities remain the only draw
authority. Unknown productive shapes use the universal deterministic cover;
unpriced or free executable shapes, changed influence identity, and unsupported
local shapes fall back without strengthening the proof.

A separate terminal-debt component handles exact-goal-mask states that remain
nonterminal because cleanup or replacement is mandatory. It charges at most
one proved legal first primitive and then grants arbitrary cleanup/replacement
and success. This retains the direct protected-reforge case: junk does not
force a preliminary Scour charge.

The strict clean-pattern component remains disabled for Eldritch-eligible
sessions. Automatic Eldritch side options continue to use their existing
no-stronger primitive relaxation.

## Proof checks

The focused carrier/Eldritch suite passed 1,653 checks. Its complete synthetic
fixture materialized 175 states and 342 ordinary rows and verified

`h(s) <= price(a) + E[h(S')]`

for every row. Additional checks cover exact goal zero, rare fractured
progress, protected goal-plus-junk/metamod nonterminals, direct Chaos
replacement without Scour, changed-influence fallback, unknown deterministic
shape fallback, action-set monotonicity, and all three materialized automatic
Eldritch side rows. The suite's compiled Eldritch policy also completed its
existing 10,000-run control.

The native build and `git diff --check` passed. The full automatic aggregate
was not used as Gate 3 authority because its resource-stop and Imprint sections
already fail at checkpoint `bd80522`; Gate 3's focused runner was isolated for
this proof check and the aggregate call list was restored afterward.

## Qualification

The retained report is `gate3-qualified-fixed-work-2/`. Compared with
`pre-gate3-checkpoint-fixed-work.json`:

| Quantity | Control | Gate 3 |
| --- | ---: | ---: |
| Certified global lower | `0.01165` | `3.47245` |
| Exact-evaluated upper | `59810.9537769745` | `59810.9537769745` |
| Solve wall | `39310.6266` ms | `41083.6708` ms |
| Expanded states | 1,000 | 1,000 |
| Compiled graph | 51 / 149 | 51 / 149 |
| Transition hash | `fb8dc170b29920df` | `fb8dc170b29920df` |
| Final policy hash | `1b98ca41e69ad1b1` | `1b98ca41e69ad1b1` |

The upper independently exact-evaluates with success probability 1, complete
prices, zero off-policy mass, and unchanged expected cost. The carrier-progress
component owns the start lower (`3.47245`); terminal debt contributes
`0.01165` there and owns satisfied-mask/nonterminal states where appropriate.
This is a proof-bearing public lower/gap consumer, not a restricted-search
value or an executable-policy claim.

Early evidence is retained deliberately. The mask-only candidate did not move
the lower. The first rarity candidate raised the lower but took about 172
seconds because both eligibility and terminal debt rescanned the full action
registry on every lower query. Price-scoped action lists plus per-state caches
remove that accidental cost; the qualified run is within 4.6% of control.
One harness report also records a Windows partial-file atomic-replace failure
and carries no solver authority.

## Decision

Retain Gate 3 and enter Gate 4. The continuation lower is useful and fully
proof-scoped; Gate 4 may use it only in unresolved lower descriptors where
successor coverage is proved. Legacy carrier/action ordering remains unchanged.
