# Handoff

**Status: no active implementation boundary.** The completed
[Solver Exactness, Iteration, And Debt Closure](docs/archive/2026-08-27-solver-exactness-iteration-debt-closure/README.md)
milestone is archived. Oliver must select the next chunk before implementation
resumes. The maintained [ranked project worklist](docs/future/priority-worklist.md)
owns the remaining P0–P3 candidates.

## Checkpoint

- Branch: `main`, local-only; nothing was pushed.
- Final engine correctness checkpoint: `ba6680f` (`Keep synthetic Restart out
  of reforge dispatch`).
- Rebuilt release-WASM/web checkpoint: `3e28150` (`Rebuild WASM after solver
  stabilization`).
- The documentation/archive checkpoint containing this handoff follows those
  commits.
- Calculator product scope remains `calculator_product_v1`: generated
  automatic Imprint programs off, voluntary economic Restart off,
  goal-progress-gated reforges on, junk-free exact terminal success, and no
  disabled action families unless a diagnostic control is selected.

## Completed Result

Eight of the nine selected stabilization items shipped:

- exact V3 broad destructive rows are cooperatively resumable and cannot
  publish partial work;
- Calculator Solve rejects Mirrored/Synthesised carriers and excludes the
  owner-ruled irrelevant mirror-producing Fossil, while simulator mechanics
  remain intact;
- strict refinement yields immediately when an alternative exposes a new
  carrier frontier, avoiding stale obligation work from the old generation;
- telemetry collection and JSON serialization are separate source owners and
  compact/full-evidence intent is explicit;
- the current solver documentation is a navigable index plus ten narrow
  authority pages;
- influenced-modifier presentation uses one native-facing order; and
- rare nonzero probabilities remain visible, with exact values separated from
  Simulator samples and Wilson 95% intervals.

Final acceptance also found a correctness defect outside the planned list:
synthetic Restart inherited the default `Transmute` type and entered renewal
dispatch before its synthetic branch. Coarse, factored, and exact evaluation
now keep Restart as the deterministic base-purchase transition.

The ninth item, cross-process development checkpoint/replay, did not ship. An
honest checkpoint must serialize joint calculator states/operators/admission
authority at the coarse boundary and the persistent oracle, partition
generations, proof obligations/dependencies, kernels, resumable cursors, and
incumbent at the strict boundary. Saving request/result JSON or sparse rows
would still rebuild the expensive owners and is explicitly rejected as fake
replay.

## PDR Proof Boundary

The final matched four-mod PDR run stopped after 168.418 seconds at the named
1 GiB solver-owned memory boundary. It inserted one strict frontier state,
completed 2 alternative rows instead of about 14,000, and used 3,507,568
logical / 1,380,787 V3 strict reforge work. It retained 846,846,750 bytes in
the proof store plus quotient and reached a 1,179,431,999-byte native owned
peak.

The independently exact-evaluated bounded policy remains
`7866.432124027084` Chaos with certified lower `21.772459401271156`. The
coarse exact value `8084.680082389483` does not reconcile with that compiled
value, so strict refinement remains required. The frontier-yield repair is a
real reduction in wasted proof work, but it does not close the PDR case.

## Recommended Next Boundary

Select P1.3 as its own milestone: design the versioned development-only
checkpoint/replay format at the completed coarse graph and first closed strict
partition boundaries. It should be an iteration tool only and refuse every
identity or completeness mismatch.

After replay is real, continue P1.4 against the now-measured retained
proof/quotient memory owner. Do not start with another generic cap increase or
re-run a broad benchmark matrix. Success means less retained memory, fewer
obligations/kernels, another frontier advance, or exact closure—not merely a
larger displayed lower.

## Acceptance

- fresh native release build passed;
- focused solve/API/policy-refinement suites passed 7,902 / 2,644 / 2,083
  checks with zero failures;
- release WASM rebuilt;
- `npm test` and `npx tsc --noEmit` passed;
- the complete repository pipeline passed ingest, canonical-data/artifact,
  3,417,248 native checks, benchmark specifications, bindings, release-WASM,
  nonvisual web tests, and the repository's 10,000-run compiled-strategy
  controls;
- documentation link audit and `git diff --check` passed.

The fresh PDR diagnostic intentionally skipped sampled verification because
its compiled policy was independently exact-evaluated and published only as
bounded. No rendered browser review or broad benchmark matrix was run or
claimed.
