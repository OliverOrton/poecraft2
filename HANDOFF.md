# Handoff

**Status: no active implementation boundary.** Oliver must select the next
chunk before implementation resumes. The maintained
[ranked project worklist](docs/future/priority-worklist.md) owns the remaining
P0–P3 candidates.

## Checkpoint

- Branch: `main`, local-only; nothing was pushed.
- Engine/WASM/web behavior checkpoint: `102ea11` (`Add solver action-family
  controls`).
- The documentation/archive checkpoint containing this handoff follows that
  commit.
- Calculator product scope remains `calculator_product_v1`: generated
  automatic Imprint programs off, voluntary economic Restart off,
  goal-progress-gated reforges on, junk-free exact terminal success, and no
  disabled action families unless a diagnostic control is selected.

## Completed Result

The engine now accepts a stable `disabled_action_families` request vocabulary
covering ordinary primitive mechanics plus temporary Bench, metamod, Imprint,
and Restart program scope. Filtering is dependency-aware: direct actions and
fixed/carrier-local automatic programs cannot reintroduce a disabled family.
Unknown names fail precisely. C ABI/WASM action descriptions expose native
family metadata, and result telemetry discloses the restricted envelope.

Calculator exposes advanced diagnostic family controls, all enabled by
default. It sends restrictions only to product-envelope/scoped Solve goals,
uses native family metadata for action readiness, and states that bounds and
exactness apply only within the selected envelope. The existing ordinary
Imprint and economic-Restart controls remain separate.

The historical 13 native-solve failures did not reproduce: current starting
source was already green at 100,169 solve checks. A warm-machine race in the
pre-existing exact-evaluation cancellation smoke was made deterministic with
a genuinely multi-step proper graph.

## PDR Attribution

One matched four-mod PDR ablation disabled `temporary_bench` under the 50M
logical-work / 1 GiB product diagnostic boundary. It retained a fully
exact-evaluated executable policy at `7862.857725228987`, with success
probability one, zero off-policy mass, complete prices, lower
`21.772459401271156`, and truthful `bounded_feasible / refused_resource_cap`
publication.

The ablation is not a product improvement. It reached 43,535 exact states and
the 1 GiB peak-memory boundary after 6,240,606 logical / 2,415,465 V3 reforge
work, before creating any alternative obligations. The retained control had
13,991 exact states, created 299,394 obligations, completed 14,054 alternative
rows using 49,999,992 logical / 43,404,344 V3 work, and stopped on the 300 s
watchdog with zero policy improvements. Temporary Bench therefore does not own
the obligation count; disabling it moves the bottleneck earlier and may remove
a compact inherited policy/partition.

Detailed evidence is in the
[archived result](docs/archive/2026-08-27-solver-stabilization-action-family-controls/result.md).

## Recommended Next Boundary

Select P1.2: make broad destructive-row recurrence cooperative and genuinely
resumable with deterministic bounded slices and retained cursors. The control
still has a native work item above 100 seconds; a partial row must never gain
proof authority, while completed resumed output must be bit-identical to an
uninterrupted row.

After that representation is stable, select P1.3: a development-only
deterministic checkpoint/replay harness at the coarse-to-strict boundary and
closed strict partition. Then use both tools for P1.4 family/row attribution
and the narrow measured proof repair. Do not start five-goal tuning, broad cap
increases, state-dimension deletion, or mechanic changes by implication.

Two possible P0 reports still require Oliver's concrete inputs: one influenced
modifier ordering/source example and one low-probability Calculator request
with expected versus observed quantity.

## Acceptance

- fresh native release build passed;
- focused API/S8.3/solve suites passed 2,643 / 541 / 100,169 checks;
- release WASM rebuilt;
- final full repository pipeline passed ingest, canonical-data/artifact,
  3,467,980 native checks, benchmark-spec validation, bindings, release-WASM,
  and nonvisual web tests;
- `npm test`, `npx tsc --noEmit`, and `git diff --check` passed.

No rendered browser review or broad benchmark matrix was run or claimed.
