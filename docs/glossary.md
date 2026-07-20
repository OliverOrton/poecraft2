# Glossary

**Status: current project vocabulary index.** Definitions summarize terms and
link to the stable document that owns their full contract.

Parent: [Documentation map](README.md)

## Product Surfaces

**Emulator** — Interactive one-action-at-a-time item crafting backed by the
native engine. See [Product](product/README.md) and [Mechanics](mechanics/README.md).

**Calculator** — Product surface for exact single-action outcomes, exact graph
evaluation, and one-item solving. See [Calculator](product/calculator.md).

**Strategy Builder** — Graph authoring surface for start, operation, routing,
and terminal nodes. See [Strategies](product/strategies.md).

**Simulator** — Native compiled strategy runner used from Python and WASM. It
produces sampled outcomes and is distinct from exact evaluation. See
[Strategies](product/strategies.md).

**Stash** — Local manually saved item and strategy resources, separate from
workspace layout and crash recovery. See [Workspace](product/workspace.md).

## Data And Engine

**Canonical SQLite** — The source-of-truth normalized game-data database.
Runtime artifacts are derived from it. See [Engine Data](engine/data.md).

**Compiled artifact** — The validated runtime `manifest.json`, `strings.json`,
and `game-data.json` bundle loaded by the engine. See [Engine Data](engine/data.md).

**Session** — Immutable runtime view for one base and item level, with dense
modifier IDs, masks, and lookup tables. See [Engine](engine/README.md).

**Carrier** — The current item state from which a solver action or exact option
is considered. Carrier flags, mods, capacities, and preserved structure affect
legality and outcomes. See [Solver](solver/README.md).

**Action descriptor** — Stable action identity plus parameters, legality,
transition facts, and price-key requirements. See [Mechanics](mechanics/README.md).

**Pool** — The request- and item-shaped eligible modifier set before weighted
selection. See [Pools](engine/pools.md).

**Weight** — The integer spawn weight after ordered selector rows and mechanic
multipliers. See [Weights](engine/weights.md).

**Bitset** — Dense modifier-set representation used for masks, groups, tags,
and pool operations. See [Bitsets](engine/bitsets.md).

## Solver

**Goal slot** — One requested modifier-family/tier condition represented in the
abstract solver goal. See [Solver](solver/README.md).

**Abstract state** — Compact, hashable projection of mechanic-relevant item
facts used by the exact transition graph. See [Solver](solver/README.md).

**Junk class** — Equivalence class for non-goal modifiers that retains the
facts required by admitted actions. Strict versus compact partitioning is an
explicit exactness/state-width choice. See [Solver](solver/README.md).

**Action envelope** — The primitive action families and modes made available
to a solve before state-local legality and exact candidate admission. The web
product normally requests the `goal_relevant` mode. See [Solver](solver/README.md).

**Admission** — State-local process that keeps an exact automatic option only
after legality, relevance, resource availability, complete-kernel evaluation,
and deduplication. See [Solver](solver/README.md).

**Fixed option** — Exact multi-step operator represented as one Bellman choice
but compiled back to ordinary primitive strategy operations. See
[Solver](solver/README.md).

**Automatic candidate** — Carrier-local solver-discovered permanent bench,
temporary blocker, protected repeat, Imprint, renewal, or other supported
technique admitted through exact option machinery. See [Solver](solver/README.md).

**Exact kernel** — Complete probability and resource distribution for one
state/action or option. Equal kernels can share representation without merging
mechanically distinct carriers. See [Solver](solver/README.md).

**Entry-relative retry** — Kernel encoding whose retry/self mass points to the
owning carrier rather than an absolute state ID, allowing exact template sharing
across equivalent carriers. See [Solver](solver/README.md).

**Focused expansion** — Resource-bounded graph discovery guided by optimistic
values toward currently relevant policy states. It does not authorize heuristic
mechanic deletion. See [Solver](solver/README.md).

**Bellman value / `V(start)`** — Minimum expected downstream cost assigned by
the solver to an abstract state, especially the solve's concrete start state.
See [Solver](solver/README.md).

**SCC policy iteration** — Current optimization over compact sparse transition
rows using strongly connected components, Howard-style policy improvement, and
sparse solves/fallbacks. See [Solver](solver/README.md).

**Review projection** — Deterministic display-only grouping of raw strategy
nodes and edges; it has no execution or solver authority. See
[Strategies](product/strategies.md).

**Exact evaluator** — Occupancy-based whole-graph calculation of terminal mass,
visits, expected actions/materials, and costs. It is separate from sampled
simulation. See [Solver](solver/README.md).

**Success-normalized work** — Optional `total / P(success)` estimate for
independent whole-strategy retries. It is not conditional-path expectation.
See [Calculator](product/calculator.md).

## Evidence And Limits

**Pinned case** — Versioned fixture with fixed artifact, goal, action/economy,
caps, and expected evidence role. See [Evidence](evidence.md).

**Pinned economy** — Immutable economy identity used consistently by solve,
compile/evaluate, and sample comparison. See [Economy](economy/README.md).

**Resource cap** — Checked solver/evaluator boundary such as states, rows,
transitions, selected owned bytes, or serialized output. A cap refusal is not a
mechanic invalidity. See [Solver](solver/README.md).

**Selected owned bytes** — Engine-reported estimate for selected solver,
evaluator, and facade-owned allocations. It is not total process, JavaScript,
or WASM heap usage. See [WASM](engine/wasm.md).

**WASM heap high-water** — Emscripten linear memory grows within configured
limits and is not shown to shrink after owners are released. See
[WASM](engine/wasm.md).
