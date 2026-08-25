# Gate 2 Ordering Qualification Stop

Date: 2026-08-24

## Candidate result

The structural carrier/action ordering candidate improved the hard five-T1
witness but cannot be retained under this plan's control gate.

- The three-run four-of-five workhorse median moved from `21748.0132` ms to
  `21653.8258` ms (`-0.433%`) while retaining the independently evaluated
  `2698.87479601436` upper.
- On the clean five-T1 witness, the independently verified upper improved
  from `14454067.4260706` to `1562083.15196689` (about `9.25x` cheaper).
  Total wall moved from `73129.284` ms to `72578.5754` ms, first finite upper
  remained about 40 ms, expanded work remained about 12,848 states, and the
  run still discovered zero goal states.
- The candidate did not filter actions. Its selected five-T1 policy added
  Annul, Bench, Chaos, Exalt, Harvest augment/reforge, and Scour families.

These measurements establish that the proposed ordering can improve fixed-
time upper quality. They do not satisfy Gate 2 by themselves because the
retained controls must also remain qualified.

## Retained-control authority failure

Two artifacts named as retained controls are not valid reproducible authorities
for the selected clean `HEAD` (`526ff6f`) before carrier-aware ordering:

- The accepted Warlord report closes exactly at `224.123858897249` Chaos in
  `1873.6874` solve ms with policy hash `2000cd384f741812`. A clean detached-
  `HEAD` rebuild instead stops at `max_solver_owned_bytes` after `28128.0721`
  solve ms, with lower `212.3`, upper `307.556312036793`, no compiled policy,
  and no policy hash. A historical rebuild reproduces the accepted result at
  `c192311`, then first reproduces the refusal at `2b8d5ac`, where exact
  explicit-affix terminal semantics intentionally made goal-plus-junk items
  nonterminal. The old value and hash therefore describe a different terminal
  contract. The restored Gate 1 tree reproduces the current refusal.
- The accepted non-armour partial-five Bow publishes
  `2042605033.16647` Chaos in `22714.2374` total ms. The restored Gate 1 tree
  instead publishes `6026985788.49406` in `55791.0425` ms. The ordering
  candidate produces that same current-tree upper in `55291.8589` ms. The
  accepted value predates exact terminal semantics; a later
  `577532.360249086` current-semantics result used an uncommitted 40-second
  override while the tracked fixture still requests 10 seconds.

Warlord was also measured with carrier ordering alone, action ordering alone,
both orderings, and the pre-carrier-ladder one-goal ordering at the first exact-
terminal checkpoint. Every variant retained the current-tree memory-cap
refusal. A temporary 2 GiB cap avoided that cap but expired the 60-second
watchdog without closure. Therefore the missing accepted behavior is owned by
the terminal-contract boundary, not the Gate 2 candidate. It still makes the
plan's required "retain Warlord exact closure" and retained-control A/B gate
impossible to certify as written.

The tri-elemental Bow is also not a pinned reproducible control: only its
accepted result summary remains in `HANDOFF.md`; no input case or report is
tracked. See the
[control reproduction audit](../control-reproduction/result.md) for the
commit matrix, cap experiment, and required successor choices.

## Stop decision

Gate 2 stops and its behavior changes are restored. The repository retains
Gate 0 attribution and the Gate 1 authority/type/source refactor only. A
post-restore 1,000-expansion fixed-work run retains the Gate 1 lower, upper,
state cap, transition hash `fb8dc170b29920df`, and policy hash
`1b98ca41e69ad1b1` exactly.

Gates 3-5 were not entered. No release WASM rebuild, web acceptance, final
native suite, 10,000-run strategy acceptance, visual review, or full
repository pipeline was run.

The next implementation boundary must repair and pin current-semantics control
authority. Oliver must explicitly decide whether Warlord exact closure remains
a separate requirement or is replaced here by a bounded proper-policy control;
the old value and policy hash cannot be carried across the terminal-contract
change.
