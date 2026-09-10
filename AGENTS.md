# poecraft2 — shared working rules

The native C++ engine owns crafting mechanics. Python owns ingest; SQLite is
canonical and the runtime artifact is derived. Bindings and the web UI adapt the
engine. Solver development aims to extend certified exact closure.

## Start with the task

Follow applicable instructions already supplied. Do not reread an identical
file just to log compliance; read it when its contents or freshness are uncertain.
Before editing, inspect relevant local changes and applicable directory rules.
Use HANDOFF when active work or continuation needs clarification. An explicit
request from Oliver selects that task; do not invent the next implementation.

Read the relevant source/contract directly. Use docs/README.md only to locate an
owner; docs/direction.md is orientation, not universal preflight. Mathematical
changes require the relevant claim, preconditions and enough of its argument to
check them. Links do not require recursive reading of every linked document.
Historical archives answer specific unresolved questions, not routine startup.

## Preserve authority and user work

Do not hand-edit canonical SQLite or derived compiled data. Do not duplicate
crafting rules in the frontend. Oliver decides ambiguous PoE mechanics; do not
research or invent them. Keep lower, upper, ordering and exactness authorities
separate; retain full scope, identity, probability and pricing obligations.

Preserve unrelated work and owner-protected path `0`, including its contents.
Do not inspect, stage, change or delete that file. Avoid destructive resets and
cleaning. Commits stay local unless Oliver asks to push; use the agent co-author
trailer. Work sequentially without subagents for the selected programme.

## Validate proportionately

Use a focused test when it resolves uncertainty or validates a retained change.
No routine suite at each phase. Select final checks by actual changed layer and
downstream impact. Rebuild WASM when ABI or strategy-vocabulary changes require
it; scripts/build-wasm.ps1 self-activates the SDK from C:\emsdk. No Simulator for unchanged strategies; when fresh qualification is genuinely
required, use the current owner-approved 1,000 trials. Report unrun/failed checks
honestly. Rendered UI review belongs to Oliver unless explicitly requested.

## Navigation and commands

- Before adding experiment infrastructure, consult [the tooling map](docs/foundation/tooling.md).
  Reuse an owner that preserves the request; start investigation with compact
  projections and expand raw evidence only for a specific unanswered question.
- Architecture/cross-layer edits: docs/foundation/change-impact.md.
- Solver contracts and mathematics: docs/solver/README.md and its relevant links.
- Substantive research import: docs/solver/research.md; preserve the argument once,
  update affected canonical knowledge, and include disposition in the usual reply.
- Native build: powershell -File scripts/build.ps1.
- Full acceptance only when justified: powershell -File scripts/test.ps1.
- Python: PYTHONPATH=tools/ingest;bindings/python with py -3.
- Web: npm test and npx tsc --noEmit in apps/web when that layer is affected.

A small fix needs no new plan, claim, research packet or archive. For substantive
work, use one living record and a short HANDOFF; do not repeat status across maps.
