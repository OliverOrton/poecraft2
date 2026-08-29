# Native Solver CLI Workflow v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Gate 0 activation

- HEAD: `d690a954ccb47d2d041b2faaf9b2963b37eba231`
- Parent: `48da3eeaa7fa03d890c3353eb0758d58694d537c`
- Branch: `main`
- Upstream divergence: `origin/main...HEAD = 0/0`
- Status: only protected `?? 0`; no other dirty path
- Installed `poecraft-solver-lab.exe`: present
- Repository MCP executable/module/registration/processes: absent
- Shared catalog: zero active jobs/attempts, queue resumed, dispatcher released

The existing CLI already owns structured profiles, cases, drafts, revisions,
submissions, matrices, jobs, attempts, summaries, comparison, evaluation,
bundles, cancellation, queue control, and supervision through
`SolverLabService`. The selected work composes those owners; it does not
replace them.

## 2026-08-29 — Gates 1–4 implementation checkpoint

- Added `solver_lab_matrix_v1` and `solver_lab_resolved_matrix_v1` to the
  existing schema registry.
- Added a mechanics-neutral RFC 6901 helper with a registered case-path
  vocabulary, typed and finite values, a 24-hour watchdog ceiling, patch and
  Cartesian limits, canonical patch ordering, and duplicate/overlap rejection.
- Added `SolverLabService.derive_case` as a replay-safe composition of the
  existing draft, structural/profile, native validation, and immutable
  revision owners. Derived case IDs bind the complete base and patch identity,
  rather than catalog allocation order.
- Added `SolverLabService.run_matrix_definition`. It derives content-addressed
  revisions, resolves complete execution requests, writes the immutable
  resolved manifest under `build/solver-lab/matrices` before job submission,
  and then creates deterministic experiments and planned jobs.
- Added filtered dispatch to the existing supervisor. A filtered owner can
  claim only its declared job IDs; a competing legitimate owner remains the
  singleton dispatcher.
- Added `derive-case`, `run-matrix-file`, and `run` CLI adapters. Stdout remains
  one `solver_lab_operation_result_v1` JSON document; changed status is emitted
  only to stderr, and existing large artifacts remain on disk.
- Added the checked-in matrix definition
  `experiments/solver-lab/native-cli-workflow-v1-smoke.json`.
- Focused new workflow tests: `6 passed in 3.46s`.

No solver, ladder, fragment, proof, mechanic, C ABI, WASM, GUI, MCP, or catalog
schema behavior changed. Terminal-only native acceptance and the final focused
and repository suites remain pending.

## 2026-08-29 — Gate 5 terminal acceptance and defect repair

The first isolated acceptance catalog at implementation checkpoint `f88063b`
proved the CLI workflow and exposed an authoring defect. Derived exact
prefix/suffix controls remained at native phase 0 with zero states until their
120-second watchdogs, while frozen suffix job
`job-469f04d2-f85b-42c9-bc2f-42ffd5efc71c` / attempt
`attempt-b756ccdf-1d64-449d-9cae-c6c7524fcb1a` completed exact in 47 seconds
with strategy SHA-256
`dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a`.

CLI reads isolated the cause: `_localize_editable_case` rebuilt
`product_action_envelope.envelope_goal` from the public goal and dropped the
frozen envelope's `fossil_mode=goal_relevant` and
`requested_fossil_actions=[]`. That broadened native action-catalogue work.
The in-scope repair preserves existing envelope-only controls while overlaying
registered editable goal fields. The derivation request now binds
`solver_lab_case_derivation_v1`, preventing old broken revision results from
silently replaying under the repaired workflow. Focused workflow regression:
`6 passed in 3.41s`. Repair checkpoint: `0e9d907`.

Fresh final acceptance root:
`build/solver-lab/acceptance/native-cli-workflow-v1-0e9d907`.

- Frozen four-mod base:
  `conquest-lamellar-allflame-clean-4-pdr-product8`, SHA-256
  `bbd55011c734132ef9f211eff16e67a44cfc72988c65d23f2152acade5ec130f`.
- Applied patches: `/goal/min_satisfied_slots=3`,
  `/goal/disabled_action_families=["fossil"]`,
  `/caps/max_diagnostic_samples=32`, and `/watchdog_seconds=60`.
- Derivation identity:
  `75356c37a0b70ad16ddc4a1212980595bec5efecb39c9592e711365bdb6b5cf3`.
- Draft: `draft-dcb968c8-f901-4323-8ba9-96b152dc9425`.
- Immutable revision:
  `case-rev-7886ca98a25c6ef8b0c17c35fb3d611c`, SHA-256
  `7886ca98a25c6ef8b0c17c35fb3d611c346169054d55a87e677128d8be02230e`.
  Native `--validate-only` passed; equal derive replay returned the same IDs.
- Natural bounded job `job-8e49128e-7c7d-4053-8761-e21153e61759`, attempt
  `attempt-801e9662-2552-44f8-9106-f8516c53a543`: target-filtered dispatcher,
  watchdog/partial at phase 1, lower `21.726316318107614`, 7,166 discovered,
  6,028 expanded, 1,138 frontier states, and 67,283 rows. Job/full/core
  identities were respectively
  `94f05fe0278b732431203bb44ea05b523a4c4492ca6e988bd1eba8e7038bee07`,
  `57f075ce947bc01930b32c2c6bca73cfb5f8b39493be88aee4bed5f388d7d9ef`,
  and
  `0ca21a91fa19d086b3aadcddf1140a13c9ceadaaf5a5dc2b079daca0ac3e2fbe`.
- Live-canceled job `job-776debec-47fc-46f3-8881-0c9918aacb79`, attempt
  `attempt-4b7605ed-d58b-41ad-abaf-ee086db2db0f`: observed running native PID
  23420 with creation token `23420:134325022184933682`, canceled through CLI
  in `416.0579000017606ms` by
  `graceful_then_process_tree_termination`, `survivor=false`, PID absent,
  zero held leases, zero reserved bytes, and released dispatcher ownership.
- Attempt comparison: request/full/core identities equal; all enumerated core
  components equal; outcomes honestly differ as watchdog versus canceled.
- Frozen exact control: `conquest-lamellar-allflame-clean-3-suffix-product8`,
  job `job-abb78cdf-e6e2-4489-b016-04ca20208a9f`, attempt
  `attempt-7c6a7545-e4b1-4086-be1f-a018036fb0b1`, exact-closed cost/bounds
  `1101.15648683309`, 3,882 states, 114,444 rows, strategy SHA-256
  `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a`.
  Recorded native independent exact evaluation is matched/converged, success
  probability 1, zero off-policy mass, and cost reconciled.
- Matrix definition SHA-256:
  `fd13afc8864b7e73c347b2d00ead61488c21d32d6a2e875b8782603564d6cb29`.
  Resolved identity:
  `199366024423df4cea39fe2bd8cd5fba93ba52c17c37ded24d06baf2d039af00`;
  manifest SHA-256:
  `aa742d1ff1858889a955c0c7de00d46eaeec5d50f580fd86b4146b85f797fabb`;
  experiment `exp-matrix-file-199366024423df4cea39fe2b`.
- Matrix revisions:
  `case-rev-6d5d67713f3fb0b7ae7907083d769a56` and
  `case-rev-34fd2b215919bea89979d63536f14cc0`.
- Matrix jobs/attempts:
  `job-matrix-199366024423df4cea39-000-000` /
  `attempt-be1e21e7-d972-45c0-a484-208269c60580` and
  `job-matrix-199366024423df4cea39-001-000` /
  `attempt-bd33cfd6-56a8-4d59-bc54-8ed4586c2e44`. Both published naturally;
  coordinate 0 exact-closed with strategy SHA-256
  `08b77e33554066cf240c714a86e8217d5a84e29381357f1ef9104033e24f5488`;
  coordinate 1 retained its truthful completed bounded evidence without a
  strategy. Equal matrix replay returned identical definition, resolved,
  manifest, experiment, revision, and job identities.

No investigation bundle was required or exported by this boundary. No GUI,
MCP, direct SQLite, complete-document replacement, or alternate runner was
used for acceptance.

## 2026-08-29 — Final acceptance

- Complete focused Lab suite: `73 passed in 43.01s`. One preceding run had an
  expected stale schema-vocabulary assertion and a non-repeating Windows GUI
  lifecycle timeout; the vocabulary assertion was updated, the lifecycle test
  passed alone, and the complete suite then passed.
- Full `powershell -File scripts/test.ps1`: passed once after focused repair,
  including 3,417,655 engine checks with zero failures, 13 solver benchmark
  specifications, 28/28 release-WASM checks, all ingest/database/artifact/
  binding/web suites, and required 10,000-run Simulator controls.
- `git diff --check`: passed.
- Protected untracked `0`: untouched. No push and no visual review.
- The prior six-hour soak remains owner-waived, not passed, and is not
  overnight qualification.

Gate 5 and the boundary completion contract pass. The only behavior repair was
the in-scope local-authoring envelope-preservation defect revealed by native
acceptance; solver, ladder, fragment, proof, ABI, WASM, GUI, and product logic
were not changed.
