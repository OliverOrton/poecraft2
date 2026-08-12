# Economy Deployment

**Status: operational production-activation runbook.** The workflow is
implemented; external Cloudflare resources, domain routing, and repository
secrets still control activation.

Parent: [Economy](README.md)

Verified against code and local publication: 2026-08-09 through the Gate 2
Allflame/Bestiary refresh. Scope:
`.github/workflows/economy-refresh.yml`, the checkpoint module, CLI commands,
publication ordering, live provider responses, and local staged publication.
External buckets, credentials, DNS/CDN state, and a live workflow run were not
activated or verified.

## Workflow

`.github/workflows/economy-refresh.yml` runs at `0 */6 * * *` UTC and through
`workflow_dispatch`. One non-cancelling concurrency group prevents two runs
from replacing the same index concurrently.

The job:

1. checks out the repository and builds the current canonical game-data DB;
2. restores the latest private economy checkpoint and verifies byte size,
   SHA-256, and, for newly written manifests, the actual economy DB schema
   version, or initializes a new DB when no pointer exists;
3. refreshes every provider-discovered league for the categories currently
   marked required;
4. validates, publishes static output, applies retention, and writes a new
   checkpoint manifest;
5. uploads raw evidence, reports, and the content-addressed SQLite checkpoint
   to the private bucket; and
6. uploads immutable public snapshots before replacing `league-index.json`.

One required league/category fetch, parse, or mapped-key failure leaves that
league's previous snapshot pointer in place and marks it stale while other
successful leagues can advance. A malformed, hash-mismatched, or attested
schema-mismatched private checkpoint fails closed. Older checkpoint manifests
without `database_schema_version` remain accepted after their original size
and hash checks; the restored v1 database is then migrated in order to v2
before refresh.

## Required GitHub Actions Secrets

| Secret | Purpose |
| --- | --- |
| `R2_ACCOUNT_ID` | Cloudflare account containing both buckets |
| `R2_ACCESS_KEY_ID` | S3-compatible read/write access key |
| `R2_SECRET_ACCESS_KEY` | Matching secret |
| `R2_PRIVATE_BUCKET` | Raw responses, run evidence, DB checkpoints |
| `R2_PUBLIC_BUCKET` | Immutable snapshots and league index |
| `ECONOMY_PUBLIC_BASE_URL` | Production custom-domain origin, no trailing slash |
| `POECRAFT_ECONOMY_USER_AGENT` | Optional identifying provider User-Agent |

The public bucket must be served from the production custom domain. Do not put
R2 credentials or the development `r2.dev` endpoint in the web bundle. Serve
the bucket at `/economy` so the default `/economy/league-index.json` works, or
set `globalThis.POECRAFT_ECONOMY_INDEX_URL` before application modules load.

## Publication Ordering And Recovery

Private upload order is raw evidence and the content-addressed DB, then the
small `database/latest.json` pointer. Public upload order is every immutable
`snapshots/<content-hash>.json`, then the short-cache
`league-index.json`. The browser verifies the index-selected snapshot hash
before activation and retains a last verified cache on failure.

The restore manifest is generated and verified by:

```text
python -m poecraft_economy.checkpoint write
python -m poecraft_economy.checkpoint verify
```

Local fixture/validation/publication commands are:

```powershell
$env:PYTHONPATH = "tools/ingest;tools/economy"
py -3 -m poecraft_economy ingest-fixture --force
py -3 -m poecraft_economy validate
py -3 -m poecraft_economy publish --output data/economy/published
```

These commands are documented operations; they were not run during the
documentation restructuring.

The 2026-08-09 Allflame refresh deliberately used a new database, raw-response
root, and staging output under the operating-system temporary directory. It
did not read publication state from `data/economy/poecraft-economy.db`, upload
to R2, alter Cloudflare resources, or replace any external index. The validated
local output was merged into `apps/web/public/economy` only after every
index-selected content hash resolved to a matching immutable snapshot.
The Gate 2 rerun used the same isolated process and advanced Allflame,
Hardcore Allflame, and Standard to new Croaker/owner-default identities. All
previous hash-named snapshots and archived historical artifacts were retained.

## Current Coverage Contract

The scheduled refresh selects only `CategoryCapability.required` entries.
Those are Currency, Fossil, Resonator, Essence, and Beast. The Beast catalog
maps Craicic Croaker for Imprint without turning every fetched Beast into a
solver action. Each of the three generic rare beasts uses the explicit,
overridable one-chaos owner default and keeps `owner_default` rather than
provider-quote provenance. `BaseType` stays optional and `base` stays
manual-only. See [Economy Data](data.md) and [Economy Notes](NOTES.md).
