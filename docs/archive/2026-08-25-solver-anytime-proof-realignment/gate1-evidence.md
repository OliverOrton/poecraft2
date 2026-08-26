# Gate 1 — Action-envelope and mechanic-family authority

Status: passed on 2026-08-25. This is an ownership and current-mechanics
coverage gate, not final acceptance.

## Retained boundary

`ActionEnvelopeLedger` is now the typed lifecycle owner for every observed
carrier/operator obligation. An entry has one current state: queued, exact row
complete, exact inapplicability proved, incumbent-dominated by the independent
global-lower/verified-upper comparison, transactionally rolled back after a
named cap, omitted by explicit caller scope, or unresolved under a named stop
owner. Entries retain their scheduler lane, proof authority, exact row when
one exists, and evidence drawn from the existing carrier, refinement,
registry-legality, and option-kernel contracts.

Gate 1 deliberately leaves `scheduler_view_enabled` false. The legacy
completed-pair set remains scheduling and solver-owned-memory authority, while
the ledger reports its own observational allocation separately. A diagnostic
audit initially exposed a work-order change and a Bow watchdog expiry when
the ledger was accidentally read and charged on the hot scheduling path. The
legacy read/accounting path was restored before qualification; no changed
behavior was retained.

The native and WASM benchmark contracts now name the seven required control
stages and fail at the first lost stage:

1. registered;
2. admitted on a legal carrier;
3. scheduled;
4. exact row materialized;
5. selected under a disclosed synthetic price;
6. compiled; and
7. independently exact-evaluated.

Synthetic control prices are explicitly reachability/selection evidence and
not market evidence. No crafting rule, terminal rule, action default, proof
boundary, or real-case market price changed.

## Mechanic-family controls

The active corpus contains 19 cases: the nine Gate 0 product/control cases
and ten new focused forced-winner cases. Together with the existing explicit
Imprint case, all eleven focused control identities passed all seven stages:

- Harvest reforge/target tag and Harvest augment/target tag;
- Eldritch Chaos side intent and Eldritch Annul/Exalt side intents;
- protection, metamod-bearing followups, and temporary bench cleanup;
- Essence;
- pure-reweight, added-mod, and forced-mod Fossils;
- Fracture miss recovery plus Annul, Exalt, Scour, and rarity primitives; and
- explicit opt-in Imprint retry.

Every retained compiled strategy was proper, completely priced, finite,
success-probability one, and zero-off-policy under independent exact
evaluation. A focused control is rejected if any requested action is absent
from the typed lifecycle or any later selection/compilation/evaluation stage
is lost.

## Real-case ledger audit

All requested product action IDs occur in the corresponding typed lifecycle;
the missing count is zero in all nine cases. At stop, no entry remains merely
queued. Open cases retain named unresolved obligations and exact cases close
or explicitly omit their caller-excluded scope.

| Case | Requested / lifecycle actions | Exact rows | Exact inapplicable | Dominated | Caller omitted | Named unresolved |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| clean zero→five | 28 / 397 | 76,147 | 109,724 | 0 | 0 | 194,603 |
| four-of-five fixed work | 28 / 250 | 4,721 | 8,564 | 0 | 0 | 22,404 |
| owner fractured four→five | 28 / 283 | 34,704 | 55,928 | 139 | 0 | 102,775 |
| dirty preserved three→five | automatic / 62 | 64 | 0 | 0 | 35 | 0 |
| partial-five Bow | 41 / 146 | 2,516 | 1,878 | 0 | 0 | 7,150 |
| tri-elemental Bow | 33 / 252 | 4,660 | 2,741 | 0 | 0 | 7,506 |
| Warlord | 19 / 26 | 4,451 | 6,815 | 1,256 | 0 | 1,265 |
| ordinary one-mod oracle | automatic / 271 | 14 | 7 | 0 | 268 | 0 |
| explicit Imprint-on | 2 / 76 | 336 | 210 | 0 | 21 | 0 |

## Behavior-neutrality result

The final native report is
`build/performance/native-solver-solver-anytime-gate1-neutrality-final-v1.json`.
All 19 expectations passed. The nine Gate 0 cases retained their status,
bounds, graph census, exact-evaluation result, and requested action scope.
Key identities include:

| Role | Certified lower | Evaluated upper | Graph |
| --- | ---: | ---: | ---: |
| clean zero→five | 36.4286171890906 | 14454067.42607058 | 514/1788 |
| four-of-five fixed work | 3.47245 | 59810.953776974464 | 51/149 |
| owner fractured four→five | 3.47245 | 2698.8747960143623 | 215/563 |
| partial-five Bow | 101.067 | 6026985788.494062 | 154/387 |
| tri-elemental Bow | 0 | 79273.32503083374 | 83/239 |

The owner four-to-five strategy remains byte-identical with Gate 0, SHA-256
`2062ec8b3400fd5cb54538ce359b22821e681dfa11049c4cab5a8f696eac5db5`.
The non-armour case completed in 58.261 seconds rather than expiring its
watchdog. Wall times remain informational at this ownership gate.

## Checks

- release native build completed;
- the native harness validated all 19 specifications;
- the full 19-case native Gate 1 audit passed with independent exact
  evaluation and strategy output;
- TypeScript `npx tsc --noEmit` passed; and
- the focused runtime-report parity module passed all eight tests, including
  simultaneous disclosed market overrides and a separately disclosed base
  override.

Sampled verification remains deferred to the final gate under the plan's
testing cadence. No complete repository or web test suite was run here.

## Gate decision

Gate 1 passes. The ledger owns and discloses the complete observed obligation
lifecycle without consuming scheduler decisions, every required mechanic
family has executable current-semantics coverage, and the product cases retain
their Gate 0 behavior and proof authority. Gate 2 may consolidate verified
incumbent ownership and public progress truth.
