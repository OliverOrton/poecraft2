# Accounts, Publishing, And Discovery

**Status: deferred design.** Phase 12 accounts/sync has not been resumed;
publishing and discovery remain blocked on it. This file records future product
contracts, not active implementation sequencing. See [HANDOFF](../../HANDOFF.md).

## Scope

Accounts are a later phase. The local Emulator, Simulator, Strategy Builder, and Stash should work first without login.

Design saved resources with stable IDs and schema versions from the beginning so account sync can be added without redesigning every item and strategy.

## Guest And Account Modes

Guest mode:

```text
no account required
manual saves stored in IndexedDB
local Stash
JSON export/import
no public publishing
```

Account mode:

```text
server-backed Stash
cross-device access
profiles
sharing and publishing
forks
favorites
follows
ratings
public discovery
```

On first sign-in with local guest resources, ask whether to merge them. Do not merge silently.

## Stable Resource Identity

Use stable IDs from the first local implementation:

```ts
interface ResourceIdentity {
  resourceId: string;      // UUID generated locally or by server
  schemaVersion: number;
  createdAt: string;
  updatedAt: string;
  ownerId?: string;        // absent for guest-local resources
  revision: number;
}
```

Items and strategies are mutable saved resources. Published strategy versions are immutable snapshots.

```text
strategy resource:
  editable current strategy

strategy version:
  immutable snapshot created for publishing

publication:
  visibility, statistics, description, tags, and social metadata
```

## Backend Timing And Stack

Add the backend after the local app, engine, Stash, and native strategy simulator are usable.

Recommended backend:

```text
Node.js
TypeScript
Fastify or a similarly small HTTP framework
PostgreSQL
```

Reasons:

- shared TypeScript contracts with the web app
- straightforward Path of Exile OAuth integration
- PostgreSQL fits ownership, versions, follows, ratings, and search metadata
- the simulation engine remains client-side initially

Items, strategies, and summary statistics are small enough for PostgreSQL JSON/relational storage at first. Versioned object storage becomes required before publishing so referenced engine/data/economy artifact bundles can be retained without bloating PostgreSQL.

Suggested later folders:

```text
apps/api/
packages/contracts/
schemas/postgres/
```

## Authentication

Path of Exile OAuth 2.1 should be the normal user login.

Use a confidential OAuth client owned by the backend and request only `account:profile` for initial login. The backend performs Authorization Code with PKCE, validates the callback, obtains the account profile UUID, and creates or finds the local account.

Do not handle Path of Exile usernames or passwords. OAuth client registration is controlled by Grinding Gear Games, so approval is an external dependency for the account phase. Guest mode remains fully usable if approval or the OAuth service is unavailable.

Steam may be added as an optional linked identity later, but it should not be the primary account because Path of Exile users may play through standalone, Xbox, or PlayStation accounts.

Admin fallback:

```text
separate admin-only credential login
not offered as normal public account signup
strong password/passkey and rate limiting
```

The admin fallback exists for site operation if Path of Exile authentication is unavailable.

## Manual Save And Sync

Account mode still uses manual Save.

```text
Save:
  write current resource revision to the account

Save As:
  create a new account resource

Import:
  create a new unsaved copy
```

The browser may cache saved account resources locally for responsiveness and offline reading, but it must not upload unsaved changes automatically.

Initial offline policy:

```text
opening cached account resources:
  allowed

manual Save while offline:
  do not queue a hidden future upload
  offer Save Local Copy or JSON export

automatic background synchronization:
  not required
```

If a saved resource changed on another device, manual Save should detect the revision conflict and offer:

```text
Save As New Copy
Replace Server Version
Cancel
```

Sophisticated collaborative editing is not required.

## Validation And Migration

Every local/account resource and JSON import is validated against its `schemaVersion`.

```text
supported old version:
  migrate forward explicitly

unknown future version:
  preserve raw payload and allow export
  do not partially load or overwrite it

invalid or oversized import:
  reject with a useful error
```

Persistent item and strategy payloads use stable global base/mod keys, never session-local dense IDs. The stable base key is its RePoE metadata path; runtime integer base IDs are not persisted. Server revisions and timestamps are assigned by the backend; client values are advisory.

## Visibility

Saved strategies support:

```text
private:
  owner only

unlisted:
  accessible by link, excluded from discovery

public:
  visible in search/discovery and on profile
```

Items may use the same visibility model later, but strategy publishing is the first priority.

## Publishing

Publishing creates an immutable strategy version.

Publishing is gated on engine release readiness. Do not ship public publishing until:

```text
all crafting mechanics intended for the public launch are implemented
native, Python, and WASM pass the shared rule/strategy checks
the public data/update package is versioned and reproducible
100,000-run browser jobs meet the progress, cancellation, memory, and completion-time targets
```

Flow:

1. User manually saves the strategy.
2. User chooses title, description, tags, and visibility.
3. The app freezes a strategy version.
4. The browser runs 100,000 simulations for that exact version.
5. The app submits the version and simulation summary.
6. The publication becomes available.

The immutable version stores the exact validated strategy payload plus its schema version, game-data version, engine version, and economy snapshot ID. Historical versions remain viewable even when the editable strategy changes.

Required publication statistics:

```text
sample count
success rate
average/median/percentile cost when cost status is complete
cost status: complete, incomplete, or disabled
missing price keys when cost status is incomplete
average craft actions per run
engine version
game-data version
economy snapshot
simulation configuration
```

Reaching a success terminal defines success. Restart transitions affect execution but are not treated as the goal.

Published costs remain tied to the economy snapshot used for publication. A later recalculation may attach new cost statistics without changing the immutable strategy version.

If any used operation/input has no price, the publication may still publish success and craft-action statistics, but its cost status is `incomplete` and the UI must not present average, median, or percentile cost as complete. If the strategy itself branches on cost and a required price is missing, the standardized publication run is invalid and cannot publish until the economy snapshot is complete for that strategy.

## Historical Runtime And Data Retention

Each published strategy version points to an immutable artifact bundle:

```text
project-produced WebAssembly engine artifact
compiled game-rule data artifact
strategy schema/version
economy snapshot
content hashes and artifact manifest
```

Keep these bundles in versioned object storage while publications reference them. Only project-produced, checksum-verified runtime artifacts may be executed.

Historical publications are always viewable as their stored strategy and submitted statistics. When the archived runtime is still compatible with supported browsers, reruns may execute it in an isolated worker. If browser security, compatibility, or a critical engine defect makes that unsafe, the publication becomes view-only. Never silently rerun an old publication with the current engine/data and label it as the original result.

## Browser-Generated Statistics

Initial publication statistics may be generated in the browser.

Store:

```text
strategy version hash
engine version
data version
run configuration
100,000-run summary
economy snapshot id
generated timestamp
```

Client-generated results are adequate for the initial community feature and are trusted as submitted. They are not tamper-proof. Discovery ranking and later verification policy are intentionally deferred.

## Strategy Cards And Goal Presentation

Different paths into success terminals may allow many valid final items. Do not invent one canonical goal item.

Published and Stash strategy cards should show:

```text
title
description
compact success-route summary
success/cost statistics
author
tags
fork/source attribution
updated/published version
```

A strategy may optionally store a representative successful item selected from a simulator result. This item is illustrative only; reaching a success terminal remains authoritative.

The representative item can be imported into Emulator as a new unsaved item.

## Forking

Forking creates an independently editable strategy owned by the new user.

Preserve:

```text
source strategy id
source published version id
source author
fork timestamp
```

The fork may later diverge completely, but attribution remains visible.

Forking a published strategy behaves like Import:

```text
create new unsaved/owned copy
do not modify source
```

## Unpublishing

Unpublish removes a strategy version from public/unlisted access and discovery.

It should not:

```text
rewrite existing forks
remove attribution records
destroy version history required for integrity
```

Hard deletion is an administrative/privacy operation, not the normal creator workflow.

## Discovery

Discovery is a later product-design decision.

The backend should preserve enough structured publication metadata to support future search and filtering, but this document does not choose ranking formulas, recommendation behavior, popularity signals, default filters, or page composition. Initial delivery may be a simple public publication listing.

## Social Features

First account/social release:

```text
profiles
favorites
forks
follows
ratings
```

Later:

```text
reports
admin hide/unpublish and moderation audit records
comments
activity feeds
```

Reports and moderation are a later community phase. Comments should wait until reporting and moderation are operating.

Publishing still creates user-generated text before reports or comments exist. Keep the initial protection deliberately small:

```text
server-side text length limits
safe text rendering/escaping
rate limits for publishing, ratings, and follows
creator unpublish
```

## Minimal Privacy, Terms, And Attribution

Before account or publishing launch, provide short, plain-language Terms and Privacy pages.

Minimum requirements:

```text
show the required notice:
  "This product isn't affiliated with or endorsed by Grinding Gear Games in any way."

identify collected account data:
  Path of Exile account UUID/name
  saved resources and publications
  OAuth/session tokens
  limited security/operational logs

state purposes:
  authentication, sync, publishing, abuse prevention, and service operation

provide account export:
  JSON export of owned items, strategies, and publication metadata

provide account deletion:
  revoke/delete stored OAuth credentials
  delete private account resources
  unpublish authored public resources
  anonymize retained immutable version/fork-integrity records

retention:
  ordinary security/operational logs: 30 days
  deletion remnants in rotating backups: expire within 30 days
  longer retention only when required for active abuse/security investigation or law
```

Store OAuth credentials encrypted at rest and use secure, HTTP-only, same-site session cookies. Collect no email address or unrelated profile data unless a later feature clearly requires it.

Include third-party notices for software dependencies. RePoE software is MIT-licensed, while its generated game data belongs to Grinding Gear Games and must be used and published consistently with GGG's terms. Do not imply ownership of game data, names, art, or icons.

## Minimal Data Model

Initial backend entities:

```text
users
poe_identities
admin_identities
items
strategies
strategy_versions
publications
simulation_summaries
economy_snapshots
forks
favorites
follows
ratings
```

Folders/tags for a user's Stash may be stored as resource metadata. Public tags should be normalized separately from private organization tags.

Later community entities:

```text
reports
moderation_actions
comments
```

## Implementation Order

1. Build the local app and manual-save Stash.
2. Use stable resource IDs/schema versions locally.
3. Add API and PostgreSQL.
4. Add Path of Exile OAuth login and admin fallback.
5. Add account Stash and prompted guest merge.
6. Add account export/deletion, privacy/terms pages, attribution notices, and token/session security.
7. Finish the intended public engine mechanics, cross-target validation, data packaging, and performance/readiness gates.
8. Add immutable strategy versions and visibility.
9. Add browser-generated 100,000-run publishing.
10. Add simple public listing, profiles, favorites, forks, follows, and ratings.
11. Add reports and admin moderation.
12. Add comments and activity features later.

## Invariants

- Login remains optional for local use.
- Guest data is never merged without confirmation.
- Item and strategy changes save manually only.
- Publishing always freezes an immutable strategy version.
- Publication success is determined by reaching a success terminal.
- Published statistics identify their engine, data, and economy versions.
- Incomplete market prices are labeled incomplete and never displayed as complete cost statistics.
- Historical publications never silently substitute current engine/data for their original artifact bundle.
- Forks preserve source attribution.
- Unpublishing does not break fork history.
- Path of Exile OAuth authentication is verified by the backend.
- Account users can export and delete their data.

## References

- [Path of Exile OAuth 2.1](https://www.pathofexile.com/developer/docs/authorization)
- [Path of Exile developer policies](https://www.pathofexile.com/developer/docs)
- [RePoE repository and license](https://github.com/repoe-fork/repoe/blob/master/LICENSE.md)
