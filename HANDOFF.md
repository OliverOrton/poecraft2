# Session Handoff

**Status: an implementation boundary is active.**

Plan:
[Root Broad-Row Falsification](docs/active/plan.md)

Branch: `codex/root-broad-row-falsification`

Starting source: `0b72110` (`main`)

## Current Boundary

Both frozen gated runs still have `expanded_states = 1`. The first Chaos row
uses 2,807,580 of 3,000,000 reforge-work units; the next competing root Fossil
request consumes the remainder. Root broad-action competition is the
immediate measured exact-search wall.

Proceed through Gates 0 through 5 in order. First correct interrupted-row
attribution. Then test an exact streaming fixed-policy upper that accumulates
proved terminal mass without interning failures. Retain a production path only
if it improves a frozen executable upper within the unchanged remaining work.

Do not implement partial-state admission, raise caps, or silently defer an
action without an explicit admissible unresolved lower placeholder. The
unrestricted exact solver remains default.

Commits remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
