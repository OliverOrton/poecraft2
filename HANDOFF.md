# Session Handoff

**Status: active implementation boundary.**

Active plan:
[Upper-Cap Sensitivity And Zero-Progress Live Renewal](docs/active/upper-cap-zero-progress-renewal.md).

Starting source is clean commit
`19cd3e11e3a2397c2ad76e2fd8c1ae75273882d2` on branch
`codex/upper-cap-zero-progress-renewal`.

The immediate boundary is Phase 1: preserve and recover the rejected
high-impact scheduler patch, reconstruct only its run-local opt-in plumbing,
and reproduce the 200,000-state candidate before changing scheduling or
renewal identity. Then run the fixed 200,000/400,000/600,000 cap matrix and a
complete zero-goal-progress audit.

Do not treat a live zero-progress carrier as Restart. Retain canonical
live-renewal sharing only when exact renewal signatures, legality, resources,
fallback/compilation behavior, and the remaining filtered non-renewal actions
prove discarded affixes unobservable. Unrestricted all-actions behavior and
product default caps remain unchanged.

The recovered patch SHA-256 is
`3bc21c3fc5cf5cf1b91607cb6c89102eaf7ffd651ed94c602b585bd7143f3501`.
