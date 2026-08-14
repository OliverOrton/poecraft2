# Gate 2 action and price availability

Captured 2026-08-09 from the isolated working database under
`build/solver-goal-realignment/gate2/economy-live`. Historical Mirage raw
payloads, immutable snapshots, fixtures, and evidence were not changed.

## Contract repair

- The current Imprint recipe consumes one `beast:craicic-croaker` and three
  `beast:rare` inputs. The historical Chimeral regression fixture remains
  frozen.
- Beast is a required live provider category. A completed league snapshot
  additionally requires a normalized Croaker row; unrelated Beast rows remain
  outside the runtime-input catalog.
- `beast:rare` is a first-class, overridable owner default at one chaos. Its
  runtime provenance is `owner_default`, not a fabricated market quote.
- Restored schema-v1 checkpoints migrate to schema v2 before refresh. Removed
  catalog keys are retired, and a checkpoint manifest records the database
  schema version while legacy manifests remain readable.
- Fetch, parse, and normalization failures are isolated per league/category.
  The deliberately malformed Hardcore Currency payload therefore left the
  other three league publications intact.

## Fresh isolated publication

The refresh completed Allflame, Hardcore Allflame, and Standard, and rejected
Hardcore Currency with `ValueError: primaryValue must be numeric`. Database
validation passed with 1,272 source/accounted rows, three completed snapshots,
zero unresolved rows, and 87 explicitly unsupported runtime keys.

The current Allflame identity is
`economy:allflame:de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`.
Its source cutoff is `2026-08-09T18:34:48Z`, immutable file SHA-256 is
`6a12a11ef676f25ce0a3ae82bdaae092f45e457aabbf3be2662ce20cfd34f12f`,
and game-data hash is
`76375e02fc21b0bc0d5709ab589aede8b1967b9a2d53b25aaf517a206f592000`.
It contains 863 prices, 583 explicit missing keys, and 29 low-confidence
prices. Croaker is a 66-chaos `quote`; the generic rare beast is a one-chaos
`owner_default`.

The additive publication also produced:

- Hardcore Allflame `9dece258da8894eb75cbb28115c89cca3149a03b885510e10ab6c22836dbc2fc`;
- Standard `7da0b6fe6884582874c8e75bbcc19e2c9cf76388992a68d546f14c967a469702`.

The public league index retains all archived entries and the prior Hardcore
last-good pointer with a fresh stale/error disclosure.

## Price-family audit

The current snapshot reports the following runtime catalog coverage. Missing
keys remain explicit; they are not assigned fallback prices.

| Family | Priced | Explicit missing | Note |
| --- | ---: | ---: | --- |
| Bestiary | 2 | 0 | Croaker quote plus rare-beast owner default |
| Bench | 673 | 70 | per-recipe derived coverage |
| Essence | 101 | 5 | 29 priced rows are low confidence |
| Fossil | 25 | 420 | only recognized market rows are admitted |
| Resonator | 4 | 0 | complete |
| Harvest owner allowlist | 30 | 0 | 16 reforge, 11 augment, 3 resistance swaps |
| Eldritch implicits | 8 | 0 | four Ember and four Ichor tiers |
| Influence Exalt | 4 | 0 | complete |
| Base | 0 | 1 | fixture-local explicit base override remains required |

All directly named basic, Veiled, Eldritch-action, Fracture, and Unveil keys
required by the supported runtime vocabulary are priced or zero-cost as
declared. The primary fixture retains only its explicit five-chaos base
override on top of this snapshot.

## Generated identities and focused checks

The regenerated local canonical database and derived engine artifact use:

- compiled manifest SHA-256
  `eae9043a1631fabf706729e355aa828afe7eadc4e8e7846a9eab67e7ad7598c1`;
- compiled strings SHA-256
  `ba2110894e94b533d42e0440b83fab468d848e438ff5e6d6ed976108ac0d507f`;
- game-data SHA-256
  `af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5`.

Focused checks passed for provider scheduling, required-key enforcement,
schema migration, stale-key retirement, per-league failure isolation,
checkpoint create/verify compatibility, runtime-schema validation, browser
source labels/pinning, Allflame economy loading, native Bestiary behavior,
native Imprint compilation, Python bindings, and web Bestiary bindings. The
current product-path automatic-Imprint gate also passed on the normal bounded
`goal_relevant` product registry with explicit `augment` and `regal`
continuations. It produced a policy with termination 3, stop cause 1, no cap
hit, 16 expanded states, and Imprint telemetry of 21 candidates, three
eligible, and zero missing-price exclusions. Compilation selected `augment`,
`bestiary:imprint`, `bestiary:restore_imprint`, and `regal`; exact evaluation
converged to success probability 1 with complete pricing. Simulator completed
10,000/10,000 successes with zero failures and zero missing-price runs,
consuming 36,155 Croakers and 108,465 rare beasts. The forced-winner fixture
preserves all Bestiary and primitive-continuation snapshot prices and discloses
only two non-Bestiary overrides: `eldritch_chaos` from 39.21 to 1,000 chaos and
`eldritch_annul` from 40.53 to 1,000 chaos.

The regular cross-runtime benchmark fixture
`vaal-regalia-allflame-imprint-retry` was then qualified natively without the
final Monte Carlo run. It closed exactly in two solver steps with 16 expanded
states, a maximum solver-step time of 73.4584 ms, and no watchdog expiration.
The lower bound, upper bound, solver witness, and independent compiled-policy
evaluation all equal `252.65352021274481`. The compiled nine-node, eleven-edge
strategy contains exactly one `bestiary:imprint` operation and one
`bestiary:restore_imprint` operation. Independent evaluation reports success
mass `0.99999999999999978`, zero failure/off-policy/unresolved mass, complete
pricing, expected Croaker use `3.6498862391562379`, and expected rare-beast use
`10.949658717468713`; the enforced exact 3:1 material contract passed. The
sampled half of that contract is intentionally unchecked in this qualification
run and remains a final native/release-WASM 10,000-run acceptance item.
The native report SHA-256 is
`84412c89af75db68b8f300753bc2ef613c3bb575abf14fb7446c9729f14d7dd3`;
the prepared strategy SHA-256 is
`9f633f123e00b667826e56bae1b3f7990dc3c2e3863444db88ae20c7a7102d37`.
