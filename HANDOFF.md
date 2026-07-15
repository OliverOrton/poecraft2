# Session Handoff - S7 planning active

Written 2026-07-15 after Oliver selected realistic end-to-end one-item solving
and a large solver-specific performance pass as the next milestone. Read
[AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

## Active boundary

Implement **S7.0 only: benchmark corpus and solver telemetry**. Add native and
worker/WASM optimal-solver benchmarks, propose the permanent real craft corpus,
capture the unoptimized baseline, and set comparison targets and safety caps.
Stop before S7.1; do not mix optimization or mechanic fixes into the baseline
commit.

S7 takes priority over new mechanic breadth. The trade/corruption/finishing,
Hinekora's Lock, beast imprint, and recombinator work is parked as mechanic
track M1-M5. Phase 18 recombinator engine work, accounts/publishing, and ML also
remain parked, deferred, blocked, or later. Economy E0-E7 is complete and is
not an S7 dependency; only external production activation remains.

Oliver skipped S6 Phase 3 ambient Emulator odds entirely. It is not deferred
work and must not reappear in a kickoff, backlog, or performance scope.

## Why S7 exists

S1-S6 is a working exact vertical slice, but not yet a realistic endgame
planner:

- solve expands the full reachable closure;
- expansion, every Bellman sweep, and policy extraction scan the candidate
  action set again;
- the current Vaal Regalia registry can contain 15,604 actions, including
  15,275 one-to-four-fossil combinations;
- the candidate action set also widens the junk-class abstraction;
- solve caps do not independently bound discovered states, state/action rows,
  transitions, reforge work, bytes, or compiled graph size;
- existing Phase 14 benchmarks measure action/simulator throughput, not
  optimal solving;
- the six-slot all-T1 gate is a small synthetic Chaos/Restart fixture.

The active plan therefore combines real craft gates, exact solver-only macro/
sub-policy operators, certified action control, compact transition storage,
cycle-aware optimization, policy compression, and native/WASM performance.

## Planned S7 sequence

```text
S7.0 benchmark corpus + solver telemetry
S7.1 one-item correctness/state substrate
S7.2 action control + storage + first performance pass
S7.3 fixed solver options/macros
S7.4 renewal and observation-aware options
S7.5 deep optimization, cache reuse, policy compression
S7.6 real end-to-end product gate
```

Each phase ends test-green with one local commit and a rewritten handoff. Do
not roll the next phase into the same change.

## Recorded decisions and remaining approval

Oliver ruled on 2026-07-15:

1. tied/no Eldritch dominance acts as the corresponding ordinary currency;
   prefix/suffix intent is a separate explicit setup-and-craft option;
2. remove-crafted-modifiers costs one Scour;
3. make the solver as optimal as practical and report its status honestly;
   Oliver will evaluate the resulting strategies;
4. there is no owner-set solve-time or memory completion ceiling—make both as
   good as practical, keep them measured, and retain operational safety caps
   and responsiveness gates.

During S7.0, propose the permanent ordinary, advanced, and endgame targets and
start states for Oliver's approval. Before S7.1 implements a corpus-relevant
metamod ambiguity, ask Oliver for the focused ruling and encode it as a fixture.

Mechanic rulings come from Oliver. Do not research or guess them.

## Current correctness gaps S7 will own

- Harvest resistance conversion and Fracture are registered engine actions but
  are not supported by `solver_calc.cpp`.
- The stable solver plan mentioned scour/Alchemy and bench removal, but neither
  is a primitive solver descriptor today.
- With tied/no Eldritch dominance, current Exalt/Chaos/Annul fallback to
  ordinary currency behavior is owner-approved and needs a pinned fixture, not
  a semantic change.
- Crafted/fractured Boolean abstraction is insufficient for Multimod,
  recrafting, or fracture-retry options that depend on the actual carrier.
- Side-targeted operations need side-specific open/removable legality facts.

## Economy track completed

Economy E0-E7 is implemented and locally accepted. `tools/economy` maintains a
separate canonical SQLite database, dynamically ingests every exposed PoE1
league, and publishes immutable content-addressed snapshots. The workspace has
a compact league selector, verified IndexedDB caching, per-league overrides,
and pinned economy identities for runs, evaluations, solves, compiled
strategies, and saved results. Harvest resistance prices use the target-specific
`harvest_resist:<target>` vocabulary throughout native and web registries.

The six-hour publisher, retention tooling, and deployment instructions are in
the repository. External activation still requires the two R2 buckets, custom
domain, and repository secrets listed in
[docs/economy-deployment.md](docs/economy-deployment.md). The last live smoke
covered all six then-available leagues, accounted for 496/496 rows with zero
unresolved mappings, and loaded every snapshot through native, Python, and
WASM. Large economy payloads are heap-marshalled to avoid Emscripten stack
exhaustion.

## Documentation state

Active authority is now:

1. [docs/direction.md](docs/direction.md)
2. this handoff
3. [docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md)
4. stable architecture in
   [docs/crafting-solver-plan.md](docs/crafting-solver-plan.md)

The completed economy track is recorded in
[docs/economy-ingest-plan.md](docs/economy-ingest-plan.md), with operational
activation in [docs/economy-deployment.md](docs/economy-deployment.md).
Completed pre-S6, Strategy Builder Calculator, and S6 execution plans were
moved intact to [docs/archive/README.md](docs/archive/README.md). S6 Phase 3's
skipped status is preserved. Future mechanics are parked in
[docs/solver-mechanic-extensions.md](docs/solver-mechanic-extensions.md) under
M1-M5 so they no longer compete with S7 numbering.

## S7.0 gate

- One PowerShell entry point produces comparable native and WASM JSON reports.
- Reports separate registry/layout, expansion, transition calculation,
  optimization, extraction, compilation, and verification time.
- Counts include actions at every filter, tags/junk classes, states, evaluated
  pairs, transitions, cache behavior, policy/graph size, and resource caps.
- Native process and WASM heap memory, maximum worker step, and cancellation
  latency are measured.
- Approved real fixtures plus oracle/refusal cases have frozen prices and
  versioned inputs.
- The baseline is captured before any optimization and the final performance
  comparison targets and safety caps are written into the active plan. Solve
  time and memory have no owner-set completion ceiling.

## Gotchas worth retaining

- Do not collapse Unveil's sampled offer and policy choice into one random mod.
- A repeat option must use a fixed expressible exit predicate and return every
  meaningful success, salvage, and brick exit.
- Scour then Alchemy is not universally legal when locks or fractures preserve
  explicit modifiers.
- Generate macro options lazily; adding them all to the existing flat registry
  would worsen the action explosion.
- Price-only solves should eventually reuse transition work, but S7.0 measures
  the current rebuild behavior before changing it.
- Cost-bearing work must pin its effective economy identity at start; changing
  league or overrides only affects new work unless the user explicitly re-costs.
- New C ABI or strategy vocabulary requires a WASM rebuild before web tests.
