# Feeder integration acceptance

This isolated integration merges qualified Feeder `d76e3369` over the accepted
combined-feature receipt `ed79b16d`, retaining published main `1ffba88e` ancestry.
Source merge `1fff2b61` changes only the handoff conflict resolution relative to
Feeder. Test checkpoint `e429e69f` adds six bounded Python ABI regressions.
Native/web source remains byte-identical to the [Feeder owner](../2026-10-03-strategy-feeder/README.md):
engine `1ec6f4c1`, web source `024a8d62`, WASM `0b800f57`, MJS `23c405b9`.

The review found no production fix to propose. Native paid child invocation
retains actual failed outputs, child action/material accounting and absolute
parent limits. Move-to-current still requires matching native session identity.
Exact inventory evaluation and Builder recombination compilation still refuse.
No approximate inventory merging, adaptive exact authority or crafting law moved
to the frontend. ABI/export/layout is unchanged; matching WASM is retained.

## Acceptance and remaining work

Owner evidence is reused only for matching source: native300/300 including the
1,000-trial primary child, full npm37/37 plus all owners/real worker, matching WASM,
and final TypeScript. New serial shared DLL/benchmark and C-header smoke pass.
The first unfiltered Python binding run produced258 passes/40 failures; all six
new Feeder tests passed. Every failure was the same missing SQLite input
(`no such table: base_item`). Its empty test-created database and failure receipt
remain in `out/feeder-integration`; no mechanics failure has been inferred.

A byte-identical copy of the previously qualified frozen SQLite `f239ec69` is now
restored in this checkout (81,133,568 bytes,5,383 bases), with read-only table and
hash preflight. No SQL write, ingest, derived compilation or data/price refresh
was performed. The full binding retry is **UNRUN**.

The rendered fixture and native trace capture are prepared, **UNRUN**. They reuse
the installed Chrome/esbuild/Playwright owner, actual Builder DOM/React and
ephemeral IndexedDB. They exercise saving a new child revision while the parent
keeps its embedded revision; real resource selectors and port labels; Undo/Redo
and draft persistence; held recombination disclosure; and a separately executed
DLL failed-child trace including paid cost and actual item. This controlled
workspace transport review makes no full product-server end-to-end claim.

The [qualification receipt](qualification.json) freezes exact identities and
remaining commands. Bulk evidence and the runnable serial adapter stay in
`out/feeder-integration`. LOCAL released12:39:12UTC without survivors; diagnostic
owns the slot during preparation. The ready remaining batch runs full Python,
finite native trace capture, fresh web build metadata and rendered Builder
serially after a new parent grant. Publication remains held until acceptance
passes; overnight publication approval already covers completed tested work.

No main/owner branch modification, push, deployment, dependency install, dev
restart or protected-file inspection was performed.
