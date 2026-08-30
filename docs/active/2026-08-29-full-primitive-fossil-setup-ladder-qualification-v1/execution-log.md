# Full-Primitive Fossil Setup And Carrier-Ladder Qualification v1 Execution Log

**Status: active.** This log records chronological execution evidence for the
[plan](plan.md).

Parent: [Active boundary](README.md)

## 2026-08-29 — Boundary Activation

Oliver selected the full-primitive Fossil setup and carrier-ladder
qualification boundary for an unattended long-running goal.

Starting repository state was verified before mutation:

- HEAD: `f975c0cb4eb2b4069473e6f9281c208d40c8f29d`;
- branch: `main`;
- upstream `origin/main`:
  `f975c0cb4eb2b4069473e6f9281c208d40c8f29d`;
- tracked worktree: clean; and
- sole status entry: protected untracked `0`.

The protected file was not read or modified. No implementation, build, test,
CLI submission, service restart, or run mutation occurred during activation.

The retained starting Fossil witness from the prior generated-envelope
boundary is:

- revision `case-rev-c1b3c87d43e16122687521d348cf38b5`;
- content SHA-256
  `c1b3c87d43e16122687521d348cf38b5db2a1c8ba748a130ef15f364ec4aae88`;
- job `job-709e39e0-5ead-4fce-9a9c-dca35cd726aa`;
- attempt `attempt-2db95f4b-ebc7-4fab-a17c-e4e55a246cb8`;
- watchdog terminal after 60.161 seconds;
- no native partial report or phase owner;
- zero progress samples;
- empty worker log identity
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`;
  and
- supervisor cleanup reported no surviving process.

This evidence selects setup attribution first. It does not yet authorize a
Fossil, action-envelope, ladder, mechanic, cap, ABI, strategy, or product
behavior change.

## Evidence Recording Contract

For every persistent run, append a bounded record containing:

- case and immutable revision IDs;
- request/content/core/component identities;
- job and attempt IDs;
- artifact paths, IDs, sizes, and SHA-256 hashes;
- selected setup, action, ladder, work, memory, bound, policy, and terminal
  fields;
- cancellation, process-tree, and reservation disposition;
- before/after comparison and conclusion; and
- the source commit and executable/artifact identities used by the run.

Raw JSON reports and bundles remain in their structured artifact locations and
must not be pasted wholesale into this log or agent context.

## 2026-08-29 — Gate 0 Reproduction And Attribution

Execution began at clean tracked activation commit
`e7b81a0143ed1b348ed9a80b28be44916cfd7a9d`; branch `main` was ahead of
`origin/main` only by that activation commit, and protected untracked `0` was
the sole status entry. The file was not read or modified. The CLI service was
healthy: profile `native_allflame_no_imprint_v1` retained content SHA-256
`876824a29d51ef8e87013639a86120315ca13235833261980b3eb28917b6bb56`,
all seven frozen cases were present, dispatcher ownership was released, and
there were no live leases or reserved host bytes.

The historical revisions used by the prior add-back matrix were inspected
through the CLI. All five relevant revisions had
`action_mode=goal_relevant`, but their nested product envelope omitted both
`fossil_mode=goal_relevant` and `requested_fossil_actions=[]`:

- full-minus-Fossil `case-rev-096465054dabdb6899368ade9d9ff928`;
- Fossil add-back `case-rev-c1b3c87d43e16122687521d348cf38b5`;
- full primitive `case-rev-ac23c2eaefb561d22f4f92f7bdd09f1b`;
- cumulative full primitive `case-rev-9456180796c2d052e4b0b9b8d359be01`;
  and
- cumulative full-minus-Fossil
  `case-rev-cdfb71c9e2403db6b20d067ea8b42e91`.

This is the already-repaired Native Solver Lab authoring defect recorded in
the archived CLI-workflow boundary: the pre-fix localization path dropped
envelope-only Fossil controls. Immutable revisions truthfully preserve the
bad request. Current checked-in product fixtures and the current localization
path both preserve `fossil_mode=goal_relevant` and the empty requested list.

The matched old full-minus-Fossil request completed normally:

- job `job-4686b8b0-7533-4dd9-b10e-d2dbb1e6c4bc`;
- attempt `attempt-67669716-d846-40dd-bc76-907723aa2c5b`;
- job/full/core identities
  `67aaaffe1f833cc1df0533d7544a05c11c93928f58a0963710a7a5ff96b9bb0f`,
  `3e2f712116ef75fe8f0d4bd74a9cc351d404562bb791c3ae018036009c2400fa`,
  and
  `952afddfe4c4d394aec29520d5112026ccae05cdb64a45f4c0d79e3b28aca3ba`;
- natural `completed/refused_resource_cap`, 32 expanded states and 212 rows;
- independently evaluated bounded-feasible strategy with three nodes, three
  edges, SHA-256
  `6ea9b6dbd6296f6c28312478525edca27cfcdd207c318a0b8c9130c4bed6555e`;
  and
- report 404,182 bytes, SHA-256
  `9de81bd69812dde225752c5cea411a7f3b417c92316531bef6de9d4998f245cc`.

The old Fossil-addback revision reproduced its synchronous setup trap under
the current source identity:

- job `job-7c06fdcb-2f64-4659-9daa-ebdef0f39a19`;
- attempt `attempt-c6f4322c-aac0-45d4-8e08-876047d6e5cb`;
- job/full/core identities
  `b7d82549c025c3bea195c6b7b01e58a2af5545fe295f44dcf0b0db5706afb7a9`,
  `7018b27c338e19491a8523edf6c779c0e37d526e231a234ebd0674db6438266d`,
  and
  `4d79ecd5ab08dd8ce00629213770b700e67d85081cfdaea47e7c3239a188d468`;
- after about 40 seconds it remained at phase 0 with zero states, rows,
  native telemetry, or compiled graph;
- its partial checkpoint proved that the product-envelope probe had already
  returned 12,959 actions: 12,950 exhaustive one-through-four-Fossil
  loadouts plus nine currency actions;
- the owner was therefore the second synchronous `pc_solver_create`, which
  rebuilt the explicit 12,959-action request before cooperative solve work;
- CLI cancellation terminalized after 48.434 seconds with 466.277 ms
  acknowledgment using
  `graceful_then_process_tree_termination`;
- worker PID 37720 was absent, dispatcher ownership released, and both live
  lease count and reserved host bytes returned to zero;
- partial report 2,767,240 bytes, SHA-256
  `c7f9ef76460947ebc6d8dc6713ab879e2179d810628b5b1a3e12c7069b438257`;
  and
- supervisor terminal record 1,805 bytes, SHA-256
  `cc4187b88c2adfc556e184c5856ad8db5e410f446d2b664920e5a39abda5d96b`.

No direct envelope edit was made. The CLI correctly refused such a patch as
outside its bounded registry. Instead, the current checked-in clean-five
fixture was imported, natively validated, and frozen as revision
`case-rev-5916e3d1908ff18c3f2b432b2a0f6cc3`, content SHA-256
`5916e3d1908ff18c3f2b432b2a0f6cc3276bf21914c4f4e22aacdfa961ffec26`.
It owns the intended bounded Fossil fields. Three registered derivations were
then validated and frozen:

- Fossil add-back `case-rev-b7eb5e639f6c7f85372bf49f5916edd5`, SHA-256
  `b7eb5e639f6c7f85372bf49f5916edd5d416d1b6c17ec622f2f034db62be96c6`;
- full-minus-Fossil `case-rev-8f1c8d5d8686b6dd250f3ce33acfceee`,
  SHA-256
  `8f1c8d5d8686b6dd250f3ce33acfceee7300e8ce2e30ed82ea07eb36c651e1cb`;
  and
- full primitive `case-rev-49bd02aa57ce7d739d21410212da2c5b`, SHA-256
  `49bd02aa57ce7d739d21410212da2c5b434ad2ea0ac23ad9682114ca40a8a78a`.

The corrected Fossil-addback request completed naturally:

- job `job-379d2766-2cf2-41c4-acc8-bc5b907a4329`;
- attempt `attempt-e7db1f73-1b9a-46cd-9c7c-b13d7ff545bc`;
- job/full/core identities
  `df0ae2f573c4b25c644370a28c92fd2b05114f46cc448d442d05aca6749fed5e`,
  `fb6a94c313b30738f0d5149667af29699b95c43567a785a48d58096c3315119c`,
  and
  `970acb8750ce0d71b71f3316edaa5f0292a0a20b8bca5633e8d81773ed3d71b0`;
- source commit `e7b81a0143ed1b348ed9a80b28be44916cfd7a9d`, executable SHA-256
  `33b00a678191e53eac057b756cc121516635c7f6f875857bc84c1b01bfeb2450`,
  artifact manifest SHA-256
  `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`,
  and economy SHA-256
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`;
- bounded Fossil accounting: 12,950 possible, 13 generated, 12,937 deferred,
  lazy requested mode in the final explicit solver, and 22 layout primitives;
- registry/layout 28.334 ms, registry generation 1.208 ms, setup owner 3.121
  seconds, solve 15.757 seconds, and total native case 16.736 seconds;
- 1,347 discovered/expanded states, 5,144 rows, 1,415,110 reforge work,
  and 11,293 transitions;
- native peak owned bytes 80,452,689;
- `completed/refused_resource_cap`, bounded feasible, lower
  `38.962958772833694`, independently evaluated upper
  `4552297.606053835`;
- exact evaluation matched with success probability 1 and zero off-policy
  mass;
- strategy 150 nodes/517 edges, 445,256 bytes, SHA-256
  `559a854ade69dcf51b3a8e718edc58a55355e3c36f2ca2265723a6ddfdea2ecb`;
  and
- report 853,068 bytes, SHA-256
  `374bac28c1cd300c76e65e71eb22102e89d974f24d00b335815673bb0f847e9c`.

Gate 0 conclusion: there is no current bounded Fossil setup defect to repair.
The apparent blocker was a stale immutable request authored before the CLI
envelope-preservation fix. Exhaustive native construction remains distinct
and valid; current bounded product construction is observable, cancellable,
and naturally terminal without a cap or mechanic change.

## 2026-08-29 — Gate 1 Exact Fossil Controls

Existing native controls already owned:

- an explicitly requested one-Fossil public-API action;
- bounded goal-relevant generation over the real artifact, including the
  qualified one-slot count of four generated and 12,946 deferred loadouts;
- a two-named-Fossil synthetic exhaustive registry with canonical order
  `fossil_a`, `fossil_b`, `fossil_a+fossil_b`; and
- a real-artifact exhaustive combinatorial count for all named one-through-
  four-Fossil loadouts.

The missing explicitly requested multi-Fossil oracle was added beside the
requested-single API control. It retains no engine behavior change and checks
the canonical Cold+Defences identity, both Fossil component price keys,
`resonator:2`, exactly one candidate, and telemetry accounting of 12,950
possible, one generated, 12,949 deferred, lazy requested mode.

Focused milestone acceptance:

- `powershell -File scripts/build.ps1`: passed;
- `poecraft_engine_tests --solver-api-only data/compiled/current`: 2,831
  checks, zero failures;
- `poecraft_engine_tests --solver-abstract-only data/compiled/current`:
  40,075 checks, zero failures; and
- `py -3 -m pytest tools/ingest/tests/test_solver_lab_cli_workflow.py -q`:
  six passed in 4.10 seconds, including the regression that preserves the
  frozen envelope's `fossil_mode=goal_relevant` and empty requested list.

The Gate 0 cancellation provides the required pre-solve teardown control:
terminal process-tree removal and complete reservation release were verified.
Gate 1 passes. Gate 2 requires no engine repair; only the missing exact
requested-multi regression is retained.
