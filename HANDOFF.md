# Handoff

**Status: no active implementation boundary.** The
[Solver Development Checkpoint/Replay](docs/archive/2026-08-27-solver-development-checkpoint-replay/README.md)
milestone is complete. Oliver must choose the next chunk before implementation
resumes.

## Checkpoint

- Branch: `main`, local-only; nothing was pushed.
- Native checkpoint/replay implementation: `f15f590` (`Add native solver
  graph checkpoint replay`).
- Final cache ownership: `952524b` (`Keep replay evidence outside coroutine
  state`).
- Release-WASM compiler workaround/rebuild: `5f49b4e` (`Stabilize and rebuild
  release WASM`).
- The documentation/archive commit containing this handoff follows that source
  checkpoint.
- Calculator product scope remains `calculator_product_v1`: generated
  automatic Imprint programs off, voluntary economic Restart off,
  goal-progress-gated reforges on, junk-free exact terminal success, and no
  disabled action families unless a diagnostic control is selected.

## Completed Result

All nine selected stabilization/debt items are now complete. The final item is
an honest native-development checkpoint of a completed coarse transition
graph, not request/result caching. It persists the ordered abstract states,
dynamic operators, state-local automatic admission, action-envelope evidence,
and sparse graph arenas required to give numeric IDs their exact meaning.

Replay enters the ordinary transition-cache compatibility path and reruns
Bellman optimization, strict refinement/repair, compilation, and exact
evaluation. Identity, graph-option, incomplete-boundary, corruption,
truncation, and binary-layout mismatches refuse. A loaded checkpoint may not
silently rebuild. The native C ABI and benchmark harness expose this only for
development; release WASM has no checkpoint API and solver mechanics,
publication authority, and product defaults are unchanged.

Implementation also repaired two cache-fidelity defects found by the replay
control: nested certification can no longer replace the requested solve's
retained graph, and exact outer quotients carry the complete product action
scope used by cache compatibility.

## Measured Replay

Separate native processes saved and loaded the dynamic Eldritch Annul/Exalt
forced-winner control. Both produced exact `0.0103` value, lower, upper, and
evaluated cost; transition hash `0f4bbf35042d2517`; policy hash
`cd2bdb9db48843ae`; a 6-node / 7-edge compiled graph; byte-identical strategy
SHA-256 `E494EA2F...E874A33`; exact success; zero off-policy mass; and complete
automatic-action lifecycle evidence.

Replay reports graph reuse and reduced measured expansion from 44.69 ms to
0.07 ms. Total case wall was 719.12 ms ordinarily and 1315.29 ms on replay,
so this small control does not establish a total speedup: file loading/setup
and downstream certification dominate it. The intended win is avoiding much
larger coarse graph construction while iterating on downstream refinement,
repair, compiler, and evaluation code.

## PDR Exactness Boundary

The prior four-mod PDR boundary is unchanged. The last matched run reached one
strict frontier, reduced about 14,000 stale alternative rows to two, and then
hit the 1 GiB solver-owned memory boundary with 846,846,750 bytes retained by
the proof store plus quotient. Its independently exact-evaluated bounded upper
is `7866.432124027084` and certified lower is `21.772459401271156`.

The coarse checkpoint makes that case cheaper to iterate after graph closure;
it does not reduce strict proof memory or make the case exact.

## Recommended Next Boundary

Select P1.4 from the
[ranked worklist](docs/future/priority-worklist.md): attribute and reduce the
retained strict proof/quotient memory owner after the first frontier insertion.
Use coarse replay for downstream experiments where it actually removes setup
cost. Do not begin with another generic cap increase or a broad benchmark
matrix.

A first-closed-strict-partition checkpoint is a conditional later extension,
not missing data in the shipped coarse format. Build it only if measurement
shows strict replay itself is now the iteration bottleneck; it must jointly
serialize the persistent oracle, partition/dependency generations,
obligations, kernels, cursors, and incumbent.

## Acceptance

Passed:

- fresh native release build passed;
- solver API checkpoint/refusal tests passed 2,686 checks;
- solve, quotient-proof, and policy-refinement suites passed 86,220 / 616 /
  2,083 checks;
- separate-process dynamic-action parity with byte-identical compiled JSON;
- release WASM rebuilt; its 28/28 smoke suite passed and the checkpoint symbols
  remain absent from the export list;
- the complete repository pipeline passed ingest, economy, canonical data and
  artifact, bindings, 3,417,290 native checks, benchmark specifications,
  release WASM, and nonvisual web tests;
- `npx tsc --noEmit` passed;
- 1,759 local Markdown targets had zero missing, and all 62 current documents
  were reachable from the main map; and
- `git diff --check` passed.

No rendered browser review or broad hard-case benchmark matrix was run or
claimed.
