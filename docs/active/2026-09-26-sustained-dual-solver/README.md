# Sustained dual-solver programme

Oliver selected execution of the attached P0–P9 plan. The ZIP is retained
verbatim in `research-inputs/sustained-184b934/` (SHA-256
`55289fdb04928e3edd3af981c044f9c6b30ff1316554464ae1d4189149e316c7`);
its 22 declared payloads, sizes and hashes were verified before import. The
packet is research input. Native source, applicable repository rules and
Oliver's request retain authority.

## P0 baseline and expectation identity

Started at clean `main` `184b934396e9aba7d009f8dcbad3f488ee71dbfd`,
matching the packet's reviewed ref. Protected root `0` was not inspected.
The previous [dual-lane record](../2026-09-26-dual-lane-target-support/README.md)
owns original U evidence. The saved U4 ledgers under
`out/dual-lane-target-support/U4/` use compiled manifest SHA-256
`852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`,
role corpus SHA-256
`404c0ee632b199b85a5fce76a62f9d75ca59839849b2b9efd1bcf4c174125b20`,
and benchmark executable SHA-256
`99e03218c6274413374bdd9c21f79237cf88e27f3a2c17bbb26147218956a121`.
The A4/A5 roots are empty Rare Conquest Lamellar level 86, with pinned Allflame
prices, goal-relevant product scope, 240-second Finish, 300-second native
watchdog, 315-second host cleanup and 1-GiB solver cap. The runs were serial.

| Saved lane and target | A5 checked upper | Actual status | Expectation and process |
|---|---:|---|---|
| Current ordinary clean L | C85,558.7061856044 | `refused_unsupported_action` reliability class with bounded policy and matched exact graph evaluation | met, exit 0 |
| Finder conditional-retention L | C524,079.6986482172 | `bounded_feasible`, matched exact graph evaluation | unmet, exit 2 |
| Finder conditional-retention R | C133,226.02816593435 | `bounded_feasible`, matched exact graph evaluation | unmet, exit 2 |

Finder's saved failure is an expectation-schema mismatch: the case's Current
contract requires Current telemetry sections, while Finder returns version-1
Finder telemetry. No report errors or cap-check failures were present. The
new corpus copies the old cases, leaves their Current expectations intact, and
adds two A5 Finder cases with explicit solver-mode and goal-terminal
expectations. Its manifest SHA-256 before further edits is
`eb450af001c2cd732293eafa5aa68e2ff92056a21fcdd8dcb500647fd81fa0674`.
The benchmark's new Finder branch still requires a finished solve, bounded
policy status, a compiled graph and matched exact evaluation when a policy is
returned; an absent policy is separately classified. Native `--validate-only`
accepted all eight new corpus specifications. A source-matched Finder A5 L run
returned exit 0 with expectation met, bounded policy C524,079.6986482172,
matched exact evaluation and no report errors. It used benchmark executable
SHA-256
`3df3ce72e001c563c834c2bd909b701648705d737fe5d90f7f159d83150fabf4`
and the new corpus manifest hash above; its
[ledger](../../../out/sustained-dual-solver/P0/finder-L-expectation/ledger.json)
retains the exact command and graph. Host wall was 40.53 seconds; this single
case is a contract check, not a timing comparison.

The separate A5 Current reliability class comes from telemetry
`actions.unsupported_observed=1` with sample `chaos`; policy compatibility
reports `primitive_renewal_expected_actions_exceed_simulator_cap` at state 0.
The independent checked graph still exists, while the full requested action
envelope has that unsupported observation. The source classifier checks this
before policy status and emits `refused_unsupported_action`; it must not be
silenced merely because an upper is available.

The older A3 Current F4 report at
`out/strategy-finder/F4/current/cases/rp-a3-product8-long240.json` has a
bounded checked policy and `termination=refused_resource_cap` but an empty
diagnostic `cap_hits` array and `cap_hit_mask=0`. The C API derives the mask
from `diagnostics.cap_hits`, then maps any remaining `RefusedResourceCap`
termination to `other_resource_cap`; this explains the apparent mask/cause
disagreement. Final publication retains a coarse stop through
`successful_refined_publication_termination`. The saved report does not expose
which earlier coarse branch chose that stop, so a root-cause repair is not
claimed. Its separate bounded-best-policy contract failed in that saved run.

At the first read, the hosted Windows run for `184b934` was still
`in_progress`; the saved review's older build success is not a final test
result. The GitHub run is
[36282378378](https://github.com/OliverOrton/poecraft2/actions/runs/36282378378).

## Active checkpoint

P0 expectation contract and evidence resolution are complete. P1: implement and test
Current's immutable target-neutral proof capability in its actual lower,
retirement and publication consumers. P2/P3, shared completion and both
consumers remain unimplemented. Timed native programme invocations spent: 1
of the declared maximum 24. No processes are active.

The proposed mathematics is conditional. In particular, zero is a global
lower only under nonnegative costs, while a positive complete selected-row
value can be a policy upper without being a global MDP lower. Current's old
positive clean-target proof must never retire R alternatives or promote R
exactness. The plan's abstract examples do not establish native mechanics.
