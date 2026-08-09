# Final Evidence

**Measured:** 2026-08-08/09 on
`codex/verified-best-policy-publication`, starting from
`c95359663088d515982ba33e83fe2d15f89438ee`.

Parent: [milestone entry](../README.md)

## Before / after action-family matrix

The frozen qualitative before matrix is in
[Gate 0](gate0-baseline.md). The after matrix follows the actual public path:
goal JSON -> bounded relevance -> dependency retention -> candidate query ->
price completeness -> automatic admission -> solve -> compile -> exact
evaluation.

| Family / route | After product classification and result |
| --- | --- |
| Eligible Eldritch armour | 10 Ember/Ichor/Chaos/Annul descriptors retained as dependencies, zero exposed as primitive candidates; useful prefix and suffix salvage materialize, win at cost `1.1`, compile, and exact-evaluate. |
| Ineligible equipment class | No Eldritch dependency reason or option; Ring public probe remains unchanged. |
| Influenced / illegal Eldritch carrier | Registry dependencies remain hidden; carrier-local admission rejects with `eldritch_side_influenced_carrier_illegal` / setup legality. |
| Missing Eldritch price | Option is absent, `automatic_candidate_missing_price` is retained, and missing-price count is positive. |
| Permanent goal bench | Direct goal craft remains a candidate and a public exact compiled winner costs below the expensive Restart control. |
| Ordinary temporary blocker | Retained as a dependency only when it can alter a relevant follow-up; existing and pre-cleanup programs compile and execute. |
| Prefix / suffix lock | Retained under `automatic_protected_side_dependency`, hidden from candidates, and covered by existing exact protected-side/repeat kernels. |
| Cannot Roll Attack / Caster | Retained under `automatic_cannot_roll_dependency`, hidden from candidates; each bounded synthetic exact-solver winner costs `13`, compiles, and executes 64 successful sampled runs. Missing-price and illegal-carrier controls reject. |
| Multimod finish | Multimod remains dependency-only; bounded exact-solver winner costs `6`, compiles, and executes 64 successful sampled runs. Missing setup price rejects. |
| Remove Crafted Modifiers | Retained under `automatic_crafted_cleanup_dependency`; four-step pre-cleanup/setup/follow-up/post-cleanup compiles and executes. Never a standalone candidate. |
| Ordinary Essence | Exact guaranteed-goal rows survive, can win through the public C ABI, compile, and exact-evaluate. Unrelated rows are filtered. |
| Corruption-only Essence | Four compiled rows are rejected natively under `filtered_corruption_only_essence`; raw keys cannot enter ordinary Solve. |
| Harvest | Target-tag match survives; unrelated tags are absent. `harvest_reforge:defences` wins, compiles, and exact-evaluates through the public C ABI. |
| Fossil | Existing positive-relevance beam emits 4 of 15,275 loadouts for the Energy Shield probe and defers 15,271. Dense wins, compiles, and exact-evaluates through the public C ABI. |
| Fracture / Imprint | Existing native public Fracture and dedicated automatic Imprint controls remain green; neither dependency contract was broadened. |

## Role, dependency, and layout telemetry

An eligible Body Armour bench-salvage product envelope reports:

```text
registry descriptors       152
selectable candidates       19
parent layout primitives    19
automatic dependencies     133
filtered descriptors       125

candidate families:
  currency 10, Harvest 5, bench goal crafts 3, Fracture 1
dependency families:
  bench 123, Eldritch/other 10
filtered families:
  Essence 65, Harvest 28, bench 22, Eldritch/influence/Veiled 10

fossil universe 15,275; generated 0; deferred 15,275
materialized automatic operators 6; materialized dependency primitives 2
```

The Energy Shield filter probe reports 17 candidates and a parent layout width
of 17: currency 10, Fossil 4, Harvest 2, and Fracture 1. It retains 144 bounded
dependencies, filters 120 descriptors, emits only 4 positive-relevance Fossil
loadouts, and defers 15,271. Unrelated Harvest tags, all 65 non-matching or
corruption-only Essences, and 14 irrelevant bench rows remain filtered.

The eligible Eldritch solve closes at 9 expanded states. Its exact sparse work
is 24 state/action rows, 15 transition entries, 1,026 raw option outcomes, and
1,300 logical reforge work. Peak selected-allocation estimate is 65,876,965
bytes and final live estimate is 13,550,159 bytes. The selected policy hashes
are `8dc86ed513e8cf5b` for transitions and `21291fa98daae73f` for policy.

## Unchanged deterministic control

The baseline source was rebuilt in a detached diagnostic worktree at the frozen
`c953596` commit. The same unchanged public solver control was then measured in
the implementation tree. The diagnostic worktree was removed after capture.

| Measurement | Before | After | Result |
| --- | ---: | ---: | --- |
| Expanded states | 24 | 24 | identical |
| State/action rows | 92 | 92 | identical |
| Transition entries | 86 | 86 | identical |
| Logical reforge work | 2,720 | 2,720 | identical |
| Bellman backups / action evaluations | 230 / 920 | 230 / 920 | identical |
| Transition hash | `e9f2ba9132f51c8c` | `e9f2ba9132f51c8c` | identical |
| Policy hash | `bfcb25789b4f99ae` | `bfcb25789b4f99ae` | identical |
| Peak owned-byte estimate | 65,877,546 | 66,408,950 | +531,404 (+0.807%) |
| Live owned-byte estimate | 13,280,079 | 13,292,189 | +12,110 (+0.091%) |

The small memory increase is registry role/reason bookkeeping and is not a
material state-space expansion. The public control retains value `5.4351`, 24
states, and the same exact-router policy identity.

## Strategy, exact evaluation, and simulation

Native public Eldritch:

```text
expected cost               1.100000
expanded states             9
compiled vocabulary         real Ember/Ichor setup + Eldritch Chaos/Annul
exact eventual success      1
exact failure/off-policy     0
Simulator runs              10,000
successes                    10,000
failures                     0
missing-price runs           0
```

Release-WASM repeats the same product registry, hidden dependency, solve,
compile, exact-evaluation, and 10,000-run sequence:

```text
expected cost               1.9893533499818465
expanded states             45
state/action rows            73
transition entries          87
logical reforge work         1,300
exact eventual success      1
exact failure/off-policy     0
Simulator runs              10,000
successes                    10,000
failures                     0
action-not-applied           0
no-matching-edge             0
missing-price runs           0
```

The release artifacts are:

```text
poecraft_engine.mjs     41,798 bytes
SHA-256 2eb81eb0005fa50fa70291b421dacd83f7e1a28d1333fff93733eaba565650cd

poecraft_engine.wasm 4,981,408 bytes
SHA-256 1038ed0e16806c5b3a6d557b8a07de69cef6ed41f90af455c4a545318716c9c9
```

## Final acceptance

```text
powershell -File scripts/build-wasm.ps1
  release module rebuilt successfully

npx tsc --noEmit  (apps/web)
  pass

powershell -File scripts/test.ps1
  ingest tests                    18/18
  economy tests                    8/8
  compiled artifact validation     pass
  Python binding tests            17/17
  native engine checks     3,001,130 / 0 failures
  benchmark specifications        12 validated
  release-WASM engine smoke       28/28
  remaining web suites             pass
  overall exit                     0
```

The first complete-pipeline attempt reached the new release-WASM case and
stopped on an over-specific test assertion that expected the native fixture's
`1.1` cost from a different concrete carrier. The returned WASM policy was
already exact and cost `1.9893533499818465`. The assertion was corrected to the
actual forced-winner contract—finite positive cost strictly below the priced
non-Eldritch continuation—while compilation, exact-evaluation, Eldritch-node,
and 10,000-run assertions remain unchanged. The focused release-WASM file then
passed 28/28 and the complete pipeline above passed cleanly.

## Deferred work

- Veiled automatic crafting.
- Real-carrier public Multimod forced-winner qualification beyond the existing
  resource/strict-materialization boundary; exact synthetic forced-winner and
  public role-retention coverage are complete.
- Ring/Amulet evaluator-attribution and strict-partition repairs.
