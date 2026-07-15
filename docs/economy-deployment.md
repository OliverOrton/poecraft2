# Economy Refresh Deployment

The economy publisher runs in `.github/workflows/economy-refresh.yml` every six
hours and on `workflow_dispatch`. It rebuilds the current canonical game-data
database, restores and verifies the latest private economy checkpoint, refreshes
every provider-discovered PoE1 PC league, publishes immutable snapshots, and
replaces the league index last.

## Required GitHub Actions secrets

| Secret | Purpose |
| --- | --- |
| `R2_ACCOUNT_ID` | Cloudflare account containing the two R2 buckets. |
| `R2_ACCESS_KEY_ID` | S3-compatible access key with read/write access to both buckets. |
| `R2_SECRET_ACCESS_KEY` | Matching S3-compatible secret. |
| `R2_PRIVATE_BUCKET` | Private raw-response and database-checkpoint bucket. |
| `R2_PUBLIC_BUCKET` | Public immutable snapshot and league-index bucket. |
| `ECONOMY_PUBLIC_BASE_URL` | Production custom-domain origin, without a trailing slash. |
| `POECRAFT_ECONOMY_USER_AGENT` | Optional identifying contact User-Agent for provider requests. |

The public bucket must be served through the production custom domain. Do not
put R2 credentials or its development `r2.dev` hostname in the web bundle.
Serve the published bucket at `/economy` on the web origin (so the default
`/economy/league-index.json` URL works), or set
`globalThis.POECRAFT_ECONOMY_INDEX_URL` before the application module loads.
Only the public index URL reaches the browser; credentials remain workflow-only.

## Publication ordering and recovery

The workflow has one non-cancelling concurrency group. It uploads raw evidence
and a content-addressed SQLite backup to the private bucket, then replaces the
small private checkpoint pointer. Public snapshots are uploaded under their
content hash before `league-index.json` is replaced. A failed league/category
keeps that league's previous index pointer and marks it stale while successful
leagues advance.

The restore step verifies both checkpoint byte size and SHA-256 before using a
database. If the private pointer does not exist, the workflow initializes a new
database. A malformed or hash-mismatched checkpoint fails closed and does not
publish an index.

Run the same gates locally with:

```text
PYTHONPATH=tools/ingest:tools/economy python -m poecraft_economy ingest-fixture --force
PYTHONPATH=tools/ingest:tools/economy python -m poecraft_economy validate
PYTHONPATH=tools/ingest:tools/economy python -m poecraft_economy publish --output data/economy/published
```
