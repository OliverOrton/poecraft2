# Generated Planner-Envelope Qualification And Ladder-Service Repair v1 — Execution Log

Parent: [Plan](plan.md)

## Selection — 2026-08-28

- Oliver selected this boundary after reviewing the controlled diagnosis and
  an independent Pro review.
- Branch: `main`.
- Starting source checkpoint:
  `28c4c30c75cef6ff8e1b38fa218f9b5a98d70203` (`Complete verified fragment
  core v1`).
- Upstream state: `main...origin/main`; the completed checkpoint is pushed.
- Initial worktree exception: untracked `0`, three bytes, content `0` plus a
  line ending. It appeared during the preceding diagnostic window after the
  initial clean check. It is preserved, excluded from every commit, and is not
  authorization to clean or alter it.
- Completed fragment archive is parked and outside implementation scope.
- The preceding unattended-hardening six-hour soak remains owner-waived, not
  passed, and is not overnight qualification.

## Starting Diagnostic Evidence

Configured Native Solver Lab:

- server version `0.2.0`;
- exactly 31 typed tools;
- profile `native_allflame_no_imprint_v1`;
- singleton active dispatcher
  `supervisor-c0ee9221-d818-461f-8f38-046e0f1dda7a`;
- host reservation policy `solver_lab_host_reservation_v2`; and
- no running or queued diagnostic job at plan activation.

Full 60-second requested-finish controls with a 180-second host watchdog:

| Control | Revision | Job | Attempt | Full request | Core solve | Result |
| --- | --- | --- | --- | --- | --- | --- |
| unrestricted | `case-rev-597c7d2e206a4728f1120a33d7e7455b` | `job-2e01c537-942c-4072-b97f-1e7876318297` | `attempt-0ef28511-c749-44fe-a8f6-607d531e8305` | `df5ef4d605c913531322e11ad5889ed14252b58441436522ac76054a3b966c0e` | `01da933fb77c5ffc53fbb50b2b7f3e91911117826e575f1fb1f84ac8e58d82d9` | host watchdog; no survivor; no partial |
| no Temporary Bench | `case-rev-ec7a1e7485ab2ba846d94dd19b76e345` | `job-2eed42f2-dc56-4715-a281-7869d1d76592` | `attempt-f70da9be-3eae-4a2d-b6c0-bd36f62c423a` | `a2b707580d0f0bc2fb521fdb037e5d174301f9d9468021ee86901aad53613a3a` | `8a5f5c63390eb6ff694cd765e49ab0d635b1f31ee884bb37c916c84e0ef73c52` | host watchdog; no survivor; no partial |
| no Metamod | `case-rev-a59748e1e64d80992845c8eae2635234` | `job-3c910cf2-f362-421b-bb39-7a2327981565` | `attempt-90d94c45-99a2-4564-a4aa-cc38ebd0d06c` | `bb31d366279b4a93b2257dafc24ee707dc021b617e29f2b8ba74b4f279c46116` | `9334a2df40c53516289b5e106317c4ade77bc61322e90764cebd9e4d38c4ce5b` | host watchdog; no survivor; no partial |

All three falsely reported the same specifically named component:

`disabled_families = 5a83bca202e341f443101647f76b6f65565aa0d1f7ace4e0bd50ba4f77205068`

Source inspection found that this component hashes
`request["action_scope"]["explicit"]`, the profile's explicit Imprint-scope
document, rather than the goal's effective disabled-family list. The complete
goal/action-vocabulary/core identities still differ, so the defect is a false
component disclosure rather than a demonstrated whole-request collision.

Currency-only normal-cap control:

- revision `case-rev-09ff640e39a2835842f1a78e3f2d7d19`;
- job `job-2c280025-0fb6-41ea-9a8f-6d3d611fd05f`;
- attempt `attempt-e7db2c94-2889-471e-8adf-0bce3e360e16`;
- full request
  `cb274f0835e57f59bf18cdc89dc62157487d15fd205aa0bf2390c2c10b659fce`;
- core solve
  `5000fae67644b026ffcd20ba86a7a455b5147ab9b1bff644882bb4b11f75b528`;
- completed naturally in 36.897 seconds;
- exact closed at `4552297.606053835`;
- 603 expanded states, nine supported/priced actions, zero automatic
  operators, zero carrier-ladder epochs;
- ordinary result identity
  `12c47d1abf9c31b8e0c744bb5dc272f68aa2022205950a5b82abd8eacf89e780`;
- report SHA-256
  `280ac9ba77c91bc3cf7fe290802ac58695a846040fd618b8d94c05d05a38bcd3`;
  and
- strategy SHA-256
  `0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529`.

Low-cap crash witness:

- revision `case-rev-a48530752a745e57cb5055019e8d330e`;
- job `job-eb673849-7476-4480-bc25-7b2a93fb9306`;
- attempt `attempt-c9da5c17-cd0d-4698-94fc-d706fa8741e7`;
- Windows access violation exit `3221225477` (`0xC0000005`);
- 2.432-second process wall;
- last verified partial at 256 discovered, zero expanded, 256 frontier,
  zero rows, and zero transitions;
- partial artifact `artifact-a4f4fed797a5e89e4e9d9862f8b9c2d8`;
  and
- partial SHA-256
  `89e422666742845163f1fc0a1a8c70a6fa143a1e35d3611951ff30e3af3e7516`.

Historical reference evidence:

- historical carrier ladder: 35 epochs, 11,750 candidates, 615 goal subsets,
  242 automatic operators, and proper upper `87361.16904205005`;
- later recorded line: three epochs, 12,844 candidates, 63 goal subsets, 370
  automatic operators, and proper upper `14454067.42607058`; and
- both are reference evidence, not current incumbent injection or literal pass
  thresholds.

## Gate 0 — Local Source Complete; Hosted Observation Pending

- Read the repository documentation lifecycle and cross-layer impact map.
- Activated the boundary and committed its plan, execution log, active indexes,
  and handoff as `77627b2` (`Activate generated planner-envelope repair v1`).
- Confirmed the hardcoded hosted/local portability problem spans
  `engine/CMakePresets.json`, `scripts/engine-build-common.ps1`, and the Windows
  workflow.
- Confirmed `HANDOFF.md` incorrectly says nothing was pushed even though
  `main` is synchronized with `origin/main`.
- Removed compiler and Ninja installation paths from the CMake preset. The two
  PowerShell build entry points now resolve CMake, Ninja, GCC, and G++ from
  task-specific overrides, `PATH`, bounded portable locations, or dynamically
  discovered Visual Studio bundled tools, and pass the result to CMake.
- The Windows workflow now provisions MSYS2 UCRT64 GCC, CMake, and Ninja and
  exports their resolved absolute paths to the build script.
- Local narrow verification passed:
  - each of CMake, Ninja, GCC, and G++ resolved without an edition/version
    hardcode;
  - `cmake --list-presets` accepted the path-agnostic preset;
  - `scripts/dev-engine.ps1 -Task Configure` configured and generated the
    UCRT64 Release build; and
  - `scripts/dev-engine.ps1 -Task Tests` completed successfully with no work
    remaining.
- The workflow YAML parsed successfully and exposes the expected
  `msys2/setup-msys2@v2` provisioning step. The public Actions history confirms
  the synchronized starting checkpoint, but these unpushed workflow changes
  have no hosted run. No hosted success is claimed.
- The preserved untracked `0` was neither read after initial characterization
  nor modified, staged, renamed, or committed.

Gate 0's local source prerequisite is complete. Its hosted observation remains
pending an owner-authorized push and does not block local Gate 1 correctness
work.

## Gate 1 — Source Complete

### Effective Family Identity — Source Complete

- Bumped the immutable execution-request contract from v3 to v4 so legacy
  queued identities cannot be mistaken for the repaired disclosure.
- Removed the false `disabled_families` and grouped `action_vocabulary`
  components. Request comparison now exposes distinct hashes for
  `explicit_imprint_scope`, `effective_disabled_action_families`,
  `allowed_mechanic_families`, `product_action_envelope`, and
  `goal_action_list`.
- Added the finite 14-name native family vocabulary to the Lab contract solely
  for identity validation. A synchronization regression reads the native
  contract owner and fails if the mirror drifts. Family membership remains
  native authority.
- The effective disabled-family input is validated, deduplicated, and sorted,
  matching the native bit-mask's order-independent semantics. Unknown or
  malformed family documents are rejected before request identity is issued.
- Service, JSON CLI, and freshly launched stdio MCP regressions cover
  unrestricted, `temporary_bench` disabled, `metamod` disabled, and the frozen
  currency-only case. They prove three truthful effective-disabled identities,
  one unchanged explicit-Imprint identity, four distinct core/full identities,
  stable dry-run replay, and appropriate stability/change in the other three
  action-envelope inputs.
- Narrow result: 15 tests passed in 9.35 seconds across the contracts, service,
  JSON CLI identity regression, and real stdio MCP identity regression.

### Native Low-Cap Finalization — Source Complete

- Reproduced the immutable original witness
  `diagnostic-clean5-census-currency-only-20260829` revision
  `case-rev-a48530752a745e57cb5055019e8d330e`, job
  `job-eb673849-7476-4480-bc25-7b2a93fb9306`, attempt
  `attempt-c9da5c17-cd0d-4698-94fc-d706fa8741e7`. Its Windows exit was
  `0xC0000005` after a verified partial with 256 discovered, zero expanded,
  256 frontier, zero rows, and zero transitions.
- GDB reproduced the same immutable input and stopped in
  `copy_solve_summary -> pc_solver_solve_finish`. The facade unconditionally
  indexed `result.values[result.start_state]`, although a discovery cap may
  truthfully finish before the Bellman table has any row. This is a C ABI
  finalization defect, not an action-envelope or catalogue defect.
- The C ABI now publishes positive infinity as the unavailable start value
  when no value row exists. Existing JSON serialization therefore retains its
  safe `null` representation. The result remains a named resource-cap refusal
  with no policy; it does not fabricate closure or a strategy.
- A direct exact rerun of the original immutable case exited zero in 473 ms as
  `refused_state_cap`, `refused_resource_cap`, with 256 discovered, zero
  expanded, zero rows, zero transitions, no policy, and no process survivor.

All seven immutable MCP runs below use the same case
`diagnostic-clean5-census-currency-only-20260829`. Each revision was cloned
from the original witness, natively validated, submitted with a complete
idempotency key, and allowed to finish naturally. Every attempt exited zero,
had `survivor=false`, and reported `refused_state_cap` /
`refused_resource_cap`, no policy, zero expanded states, zero rows, and zero
transitions. Discovered/frontier states exactly equalled the selected cap.

| Cap/evidence | Revision / content SHA-256 | Job / attempt | Full request / core solve | Ordinary result / finalization | Report artifact / SHA-256 |
| --- | --- | --- | --- | --- | --- |
| 1 / full | `case-rev-c4b29c26af43c6720552b61a8289d78a` / `c4b29c26af43c6720552b61a8289d78a63b4b486d1b806b903651126cf83db1c` | `job-763c9641-43a2-44ce-abea-eda51377adbf` / `attempt-e1d619ae-d393-4bc5-bc03-cf3ce9b1814d` | `d5921b7261bad14a2a2c14102afffc166b66be7d03727b4bf5c9513402581cf2` / `c5dc3a24bb800a5d23b682b8d89f82e8865f86e7c6bf7fac321683c720094760` | `8e6e87d15c0539254056960d4c2a82a7aa6cf581480f3e55c15f851d5c124bef` / `5fec8f977ad267777c8b151f0363a0d5b2381eac03af5399ed3babd195147d4a` | `artifact-ed92bfac8c467d1d9bc257972992581e` / `c641ccce6186fa0755b3ed5ad0bedfff99d0552e44243054b82ff62989bd6b7b` |
| 2 / full | `case-rev-13bd4c3cf4bab3a69017d628cff9f6d7` / `13bd4c3cf4bab3a69017d628cff9f6d7788b81ccd4edd516356dc57ebee35e1a` | `job-3b9d3190-4161-4761-9ad3-8930edb5bfcb` / `attempt-acd8f3c7-eb88-461f-a1e7-7d569df4428a` | `14aa88de442ba36720f54fe078ffb4f6cdc8a7344dba8969223a6684a437fc8e` / `6c11e46a52f0ab6e8c8dd3d1debfa6cb18535b3d881e689c51050ff82ecc64e6` | `9871e2846659abe792a57d407c139ad468e2e0f96096c8bce8d17b261720e158` / `accfac012b3d5cf5e6bfb412e29ffbef3c3f9a655522f7ce8da094396a365caf` | `artifact-00217ee01db1d0e0833730dd2c2a30a6` / `d209b4dcbfc26ec1c732790195b888c073f56f61dbce98ea0e5a4960ff84c9ea` |
| 255 / full | `case-rev-c2a5b8ac0d2c6ecbf61b8d997ada10bf` / `c2a5b8ac0d2c6ecbf61b8d997ada10bf5252e79b3319dd9038dd28af8634d219` | `job-bbf76d70-5831-481f-be97-18a0fbf5a573` / `attempt-e3c75403-3cd2-4f37-8a02-cf1ec0a00fa3` | `e0dafc7d0da7860d280ba6397637bca03f61e1c3fbb48b3f77596f7727ef5125` / `f54d6aba2bc7095615291bfca49e520b5c076e16ca45db7b2afe5b5cf78f0126` | `953d1ceba7cfc0ab015a60642e6e3ab958b4293f4b4fd83ea70c8b41e46e27b4` / `59efbd538247cf70a505039f3717eb8a41e8753d6e62faa012e355cf6b6c84e2` | `artifact-c1cb2889d1855eb7ac6c66c5967df0d7` / `6adaa0df7771642f66cf2c73b5fc09308f5f2e6945f01cd190e7572d0edbb909` |
| 256 / full | `case-rev-3383ce15e7584b7ba39fd2aefb17b996` / `3383ce15e7584b7ba39fd2aefb17b99696e768cb3a73340e03238b3102cf1744` | `job-b06b5a60-64cb-4907-8cda-c31a471439db` / `attempt-3b7d3f7d-0751-42c5-ac98-aa8dc345eaf1` | `0b96a4e59e66cbb20efc526fc7f0dcea68969f5b7d69ad11d2ce2dec92fadbbd` / `14f9d8022586bc11d7d63dd8555b21afafdfef647626e48ee1275f5560e0670c` | `046e6f7bbb0ce95dfeec9ba3670201a1217ac8c320d2d94bddd14abb04dcbfd3` / `ba30d8864d151dddaff23f72f9832a81a44a35cca39e429a1dba43b0eb1fe6f9` | `artifact-93ee25dd87da5dde0367c935fbdc3db5` / `12fce36310858724b6776d6188f88b28099b69bb06482b61ff1e6f25b4447645` |
| 257 / full | `case-rev-a4f58821e5979becfe7f00ae52c309a4` / `a4f58821e5979becfe7f00ae52c309a44598ef955795cf4bfdf618a6aab8aa64` | `job-2c985c1a-5fae-4804-87b6-517424bd4485` / `attempt-09c53492-5ffd-4f18-a132-084f04a01977` | `f82dfa2af9802cff32e58c3f91bada08e742ebc2fa72a8e8eb80c4ded68a3aa8` / `e8696b87ee2fd2b918d64add4c368c0ae06c60efd731bc114c403e81bb4f9670` | `2dad51c1eb0571c5423d8e4e026f3bb4c6d51f10fcff6c8f2a22595f95aeb9d8` / `21fba7dda065df56cba104d9dbc49a0d41c7347f94c4d7e205d60412e21e5da1` | `artifact-7e957787515b51b5b0a1d4e0fb3cb06e` / `1098c728b5b5d9b9d843674cf1d4fe11fa059658a8bc08358776daab76084c16` |
| 512 / full | `case-rev-97c03e2ecffcee979979ee75e1338b3e` / `97c03e2ecffcee979979ee75e1338b3e067601dd209fc5ec9657b2c324df6e1b` | `job-b1ffeade-8054-4240-af66-60cb277eef0f` / `attempt-eb861404-059f-42b2-a284-e1ffa2bc957a` | `f8ac046cd7765a04e529e1988d61ff362e55a70c3d995ccd102c60520d6cf33b` / `3b28afb32b548bf92be050a9f24abbc5796f6d21b2ffba9bb55bf31215893858` | `ecc59fc1ed16a25af430d81bcf2278d5b2d3bd072ccffff29a0f9d895f6af927` / `02babbd78fda5aaf57d34dd66b018de5cbc2b14eaef514a895abade1d4892493` | `artifact-b1e0d34a9b4317ba3aa0e18d86e4319f` / `34fd680044c5fac2a85a6c37fd8d248987ba025a75c023430be7b6ba514aef05` |
| 256 / compact | `case-rev-e248f974899ff55527f84e60e895de93` / `e248f974899ff55527f84e60e895de9375adf73de268e562af18113f16c4d32b` | `job-55262c5d-c46f-43a2-909d-b5b1bf53552e` / `attempt-1aa13532-7da9-43c3-a44b-e369b2d1052a` | `c67a5b4fef7a456074798c38b2d6d5b7356689ee10ae21b48c11ca9f8146ff05` / `b2d6b2bf450a27e1e12b42aca18df24eb477420388fc3939b9ba4273e7d5296e` | `ddb076ad07510209bd4f944bedd2974d37271f8babb92a338f37f4d42d2dbf64` / `c09585ab2710ccc38953d3b4c849fb2a62d4ed8a281d4b5bf1f5fe68116436fa` | `artifact-ef79f7aba780ae423509b063811629bd` / `449fa2258cf57239a830c7bc9491f48ca0bdd791ee44f5e0d30a7c065b9d50fd` |

The full-evidence runs used 22,200, 42,920, 83,060, 83,080, 83,420, and
149,280 reforge-work units respectively. Their native walls were 769, 767,
768, 775, 771, and 782 ms; compact cap 256 used the same 83,080 deterministic
work units and 767 ms. None published an invalid strategy.

The persistent configured MCP process used for this crash-only matrix was
started before the v4 identity source change and therefore bound these jobs to
legacy execution-request v3. Its process provenance is recorded rather than
misrepresented as v4 evidence. The fresh stdio MCP regression above is the
v4 identity proof; final operator qualification will require a fresh
current-source server before v4 identities are accepted.

Native regressions complete the remaining structural cases without changing
the requested envelope:

- the stepped public C ABI runs both full-evidence modes at a one-state cap,
  completes one row, and returns a named state-cap refusal with no policy;
- the direct finalization helper exercises a cap before any value row; and
- the existing solver fallback fixture interrupts refinement after retaining
  a verified executable policy, requires bounded-feasible publication and a
  compilable strategy, or accepts direct exact certification if the cheaper
  core policy now closes first.

Narrow verification passed: public API suite, 2,727 checks / zero failures;
solver suite, 86,220 checks / zero failures. Gate 1 changes no action-envelope
behavior. Gate 2 operator-lineage and phase-owner instrumentation is next.

## Gate 2 — Source Complete

Implemented `solver_operator_lineage_v1` as a bounded read-only projection over
the owners that already existed. It does not add a catalogue, scheduler,
admission path, or ledger. The complete projection now records:

- all 14 `SolverActionFamily` values and all ten non-None native
  `AutomaticCandidateKind` values;
- permanent registry candidate/dependency roles, generated fixed-option count,
  primitive dependency uses, priced/supported operators, and a complete
  fixed-option semantic FNV-1a identity independent of samples;
- pre-canonical variants, canonical effect/template classes, collapsed
  variants, carrier-local checks/admissions, synthesis work/time, retained
  rows/bytes/transitions, and bounded deterministic planner-index samples;
- carrier/operator pairs scheduled, begun, completed, interrupted, and still
  waiting; carrier epochs; joint-policy attempt/success participation by
  automatic kind; and selected reachable-policy consumption; and
- missing-frontier discoveries, priority offers, service completions, maximum
  open obligations, and terminal/current open obligations.

The lineage allocation is intentionally excluded from solver-owned cap
authority; it is emitted only as observational telemetry and remains subject
to `max_telemetry_json_bytes`. A separate finalization call preserves lineage
on named refusals even if a pre-existing optional per-row diagnostic encounters
the already-fired resource cap first.

Added the append-only `pc_solve_phase_owner` vocabulary and
`pc_solve_progress.phase_owner`. Native progress, the WASM JSON facade, web
protocol, worker initial state, and benchmark bound trace now use the same
twelve names: setup, planner construction, temporary-effect precompile,
dependency preparation, primitive rows, state-local automatic synthesis,
ladder scheduling, Bellman optimization, policy assembly, compilation, exact
evaluation, and done. Final lineage retains aggregate setup/precompile evidence
for synchronous owners that finish before the first stepped snapshot.

Focused verification:

- native engine and benchmark targets built successfully under the portable
  UCRT64 toolchain;
- automatic Veiled S8.3 fixture: 552 checks / zero failures, including a
  non-empty generated-operator lineage, parsed JSON object, positive complete
  generated count, positive Veiled planner count, bounded samples, and the
  nested public telemetry projection; its existing 10,000-run Simulator check
  remained 10,000/10,000;
- public C ABI suite: 2,816 checks / zero failures, including append-only struct
  size, valid live owner range, broad-to-narrow Bellman/policy/compile/evaluate
  mappings, and terminal `done` ownership;
- `apps/web` `npx tsc --noEmit`: passed; and
- `git diff --check`: passed before documentation finalization.

A non-qualification reduced-cap instrumentation probe used the checked
`endgame-fractured-es` benchmark with `max_discovered_states=32`, progress
enabled, and Simulator verification skipped. It terminated normally as
`refused_state_cap` in 400.03 ms. The final telemetry retained
`solver_operator_lineage_v1`, complete hash `14650fb0739d0383`, all 14 family
and ten automatic-kind aggregates, and the bound trace observed
`dependency_preparation -> done`. This explicit-envelope case generated zero
fixed options, as expected. Its temporary report SHA-256 was
`905b0eb932f04fe0f4393a64b4cfe7900e1ba3b6797c0ccfafcd994939132b17`;
it is diagnostic evidence only, not an immutable Lab result or a Gate 3
performance substitute.

Gate 2 changes no solve behavior. Gate 3 two-layer primitive/generated add-back
attribution is next; a Gate 4 ladder-service repair remains unauthorized until
that matrix names a specific measured owner.

## Gate 3 — MCP Preflight Stopped On Stale Identity Authority

The mandated read-only MCP inventory completed before any Gate 3 catalogue or
job mutation:

- configured MCP source advertises version `0.2.0`; the live configured server
  exposes exactly 31 typed `poecraft2-native-solver-lab` tools;
- profile `native_allflame_no_imprint_v1`, SHA-256
  `876824a29d51ef8e87013639a86120315ca13235833261980b3eb28917b6bb56`;
- exactly seven frozen cases, including both
  `fragment-clean-one-goal-renewal-control-v1` and
  `fragment-clean-one-goal-renewal-shadow-v1`;
- 19 immutable local revisions, 20 pre-existing drafts, 40 bounded terminal
  jobs, and 41 bounded terminal attempts;
- job statuses: 23 completed, 11 failed, two canceled, four partial; attempt
  statuses: 23 completed, 14 watchdog, one crash, three canceled;
- no queued/running/cancel-requested job and no starting/running/
  cancel-requested attempt;
- queue active under dispatcher/supervisor
  `supervisor-c0ee9221-d818-461f-8f38-046e0f1dda7a`, process identity
  `57428:134324392766030429`, with one-worker
  `solver_lab_host_reservation_v2` ownership and no current reservation; and
- host reservation formula remains solver-owned cap plus 512 MiB worker
  headroom, with a separate 512 MiB global safety reserve.

A read-only `submit_job(dry_run=true)` for frozen case
`conquest-lamellar-allflame-clean-3-prefix-extended-product8`, idempotency key
`gate3-inventory-dryrun-current-contract-20260829-001`, resolved current source
commit `9ad971e42d24fd861b720bb41a80248118211d70` and current benchmark SHA-256
`7a4597fa3a04a33c8691fdea2572cd29fd9c59c9ff7c9534ba20fbad41f0cfd0`.
It nevertheless emitted `solver_lab_execution_request_v3`, grouped
`action_scope` / `action_vocabulary`, and the known false
`disabled_families` component. Full dry-run identity was
`5dbdb8bfec58a2067fe4082c7d97687bea4c353eaffe722c30c60dfda4c564a1`;
core identity was
`212b93e2c8c312eb3ad033ba1e3e0ffd16976a51a0c970d2124b8b14cf2657d2`.

This proves that the long-lived configured MCP process still owns the stale v3
Python identity contract even though it resolves the current native executable.
The boundary's explicit stale-server stop applies. No Gate 3 draft, revision,
job, attempt, queue, priority, ownership, or catalogue mutation was made. No
process was killed or restarted, no ownership was force-cleared, no GUI or SQL
was used, and no repository CLI run substitutes for MCP qualification.

Resume Gate 3 only after an owner-controlled normal restart of the configured
MCP server and a fresh dry run proves `solver_lab_execution_request_v4` with
`explicit_imprint_scope`, `effective_disabled_action_families`,
`allowed_mechanic_families`, `product_action_envelope`, and
`goal_action_list`. Re-run the full read-only inventory at that time before
creating the two-layer add-back matrix.

## Gate 3 — Fresh v4 Preflight And Matrix Control

Oliver authorized a normal MCP restart. The verified idle stale launcher tree
(`53364` / Python supervisor `57428`) was terminated without force-clearing
catalogue ownership. A fresh task and configured server recovered ownership
through the normal dispatcher protocol as
`supervisor-9bfd7140-1c63-4470-93f4-366284619fe1`, process identity
`5464:134324587492816518`. The prior owner is durably `stopped`; the queue is
active and there was no active job, attempt, worker reservation, or process
survivor.

The repeated read-only inventory before any new catalogue mutation found the
same version `0.2.0`, exact 31 typed tools, one profile, seven frozen cases, 19
immutable local revisions, 20 pre-existing drafts, 40 terminal jobs, and 41
terminal attempts. The seven cases include both fragment control/shadow cases.
A fresh dry-run submission for the three-prefix frozen case used idempotency
key `gate3-fresh-v4-preflight-20260829-001` and proved
`solver_lab_execution_request_v4`. Its request identity was
`96a28de42c91b22815ea171fc3f2a66b9fbb068baa7a6a1b814def13dc8d0435`,
core solve identity
`bec03ea55120f37ddacd506f5dc0ed6b30e8c18d93d2e88c0502041b548e1bad`,
and operation identity
`dccf7348a227801c349c8d1128db2df5df0f7c52bc36540e307cd6e983c47a62`.
The core components include all five truthful inputs and no legacy
`disabled_families` component.

The required generated-candidate suppression control did not previously
exist. Added `solver_planner_envelope_diagnostic_v1` as a benchmark-private,
finite allowlist over the ten non-None native `AutomaticCandidateKind` values.
Omission retains the complete product envelope; an empty list retains the
same product primitive registry/goal abstraction while suppressing every
generated automatic kind. The private construction hook is absent from the
public C header, bindings, WASM, and product goal vocabulary. Family controls,
mechanics, transitions, prices, caps, and ordinary defaults are unchanged.

The first MCP draft, `draft-f25e6ecd-a49b-4694-b4ba-782678781b0b`, clones the
normal-cap currency-only baseline and selects the empty automatic-kind list.
MCP native validation passed with content SHA-256
`806889b65fa89a63b1cff0f014dce03e7e86c7c5c668b150f19133525adbe20e`.
It remains a draft until the source checkpoint is committed.

Focused source verification passed: the benchmark target builds, the S8.3
suite reports 552 checks / zero failures, and the complete solve suite reports
86,224 checks / zero failures. The latter exposed a Gate 2 observational
defect: lineage classification could throw on deliberately synthetic
white-box action types outside the product family vocabulary. Lineage now
counts those as unclassified and skips their family join; it cannot fail or
alter a solve. Product registries continue to require zero unclassified
actions.
