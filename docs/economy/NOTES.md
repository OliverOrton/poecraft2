# Economy Notes

**Status: non-authoritative working notes.** Implemented contracts live in
[Economy](README.md).

Parent: [Economy](README.md)

## General

### 2026-07-19 — #debt — Scheduled Beast coverage

Status: open.

`fixtures/economy/price-key-catalog-v1.json` maps Craicic Chimeral to
`beast:craicic-chimeral`, but `PoeNinjaAdapter` marks Beast optional and
`refresh_all_leagues` fetches required capabilities only. Scheduled snapshots
therefore do not currently receive the Imprint beast quote. `beast:rare`
remains intentionally manual-only. Documentation must not describe production
Imprint pricing as complete until code and acceptance evidence close this gap.

### 2026-07-19 — #debt — External production activation

Status: open; operational, not repository implementation work.

The scheduled publisher requires private/public R2 buckets, a production
custom domain, and the repository secrets listed in
[Deployment](deployment.md). Their live state was not verified during the
documentation restructuring.
