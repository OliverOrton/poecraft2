# Product Reliability Coverage

**Status: stable non-visual coverage reference.**

Parent: [Product](README.md)

The cross-base reliability pass exercises the returned-policy chain as one
contract:

```text
Solve -> compile -> unsaved Strategy Board -> exact Calculator -> Simulator
      -> JSON save/reload -> exact Calculator
```

Automated web coverage owns base and item-level changes, input/goal modifier
editing and tier selection, every implemented crafting tab, price-ready and
missing-price states, solve progress/cancellation/retry/repeated solves,
exact/bounded/capped presentation, compiled-strategy opening, Calculator and
Simulator runners, workspace persistence and draft recovery, and the
large-board degradation model. The release-WASM corpus runner repeats the
native solve/compile/exact/simulation classifications for the versioned
[cross-base reliability corpus](../../fixtures/solver-reliability/v1/README.md).
These are non-visual assertions; they do not claim rendered-browser review.

The native structural harness separately covers all 979 engine-certified
ordinary-session bases. The generated runtime portfolio has 49 deterministic
cases across 27 item classes, two- through four-goal side shapes, dense
low-probability work, and crafted/fractured/influenced/Eldritch/Veiled starts.
Every returned policy must compile, exact-reconcile, and simulate in both
native and release WASM. The selected Gloves and Ring policies each complete
10,000/10,000 successful runs with zero failures and zero off-policy actions.

The app always presents configured caps and reports every cap hit independently
from policy availability. Exact, bounded, capped-with-policy,
capped-without-policy, unsupported, and downstream compile/evaluation/
simulation failure states are not collapsed into a generic unavailable state.

## Manual Visual Checklist For Oliver

1. Switch bases and item levels; edit input and goal modifiers and select
   tiers. Confirm names, sides, and item previews update without stale rows.
2. Visit every crafting tab. Confirm ready prices, missing prices, and disabled
   operations are legible and do not imply that missing is free.
3. Exercise Solve progress, cancellation, retry, and a repeated solve. Inspect
   exact, bounded, capped-with-policy, and capped-without-policy cards; verify
   policy quality, stopping cause, cap name, and skipped-price/vocabulary
   counts remain distinct.
4. Open a returned policy in Strategy Board. Switch between exact Calculator
   and Simulator, then save/reload the strategy and recover a draft.
5. Open a large compiled graph. Confirm degradation notices, board navigation,
   selection, and runner controls remain usable at the intended desktop size.

Code and test authority:
`apps/web/src/app/engine-worker.ts`,
`apps/web/src/app/solver-result-presentation.ts`,
`apps/web/test/engine-smoke.test.ts`,
`apps/web/test/solver-result-presentation.test.ts`,
`apps/web/test/solver-benchmark.ts`, and the remaining files selected by
`apps/web/package.json`'s `test` script.

Dated qualification:
[Cross-Base And Compiled-Strategy Reliability Pass](../archive/2026-07-30-cross-base-strategy-reliability/README.md).
