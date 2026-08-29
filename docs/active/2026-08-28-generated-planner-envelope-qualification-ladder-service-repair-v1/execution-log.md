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
+

## Gate 3 — First Two-Layer Matrix Exposed Diagnostic-Scope Defects

All 34 immutable rows below were submitted through MCP against source `895fe6ac3044ef80361d5e5ad1c1b2acf4b900b5`. The source identity was dirty only because of the preserved untracked three-byte file `0`. The supervisor retained one-worker ownership; every attempt released its reservation and reported `survivor=false` under `solver_lab_host_reservation_v2`.

The primitive layer completed 13 rows and watchdog-expired only the Fossil add-back. Every generated singleton, every cumulative generated row, and the zero-generated/full-primitive baseline watchdog-expired at 60 seconds with no native partial report, zero bound samples, and therefore no MCP-visible phase owner, action/operator count, lineage, ladder, bound, or memory evidence. Pairwise rows were not applicable because no generated singleton finished separately.

| Row | Revision / content SHA-256 | Job / attempt | Full request / core solve | Terminal evidence | Artifact identities |
| --- | --- | --- | --- | --- | --- |
| `primitive-currency-only` | `case-rev-806889b65fa89a63b1cff0f014dce03e`<br>`806889b65fa89a63b1cff0f014dce03e7e86c7c5c668b150f19133525adbe20e` | `job-4eeddb82-82fe-4fdd-a226-4cf2e4943f4d`<br>`attempt-903c27eb-2f7e-4af6-a0b0-522a4e70b1ab` | `df1611b698b2ce824fa98aaa45c47619582263b89b2ee9df9061900916a14e69`<br>`a1c9df7e533009d4d06979a6a03e5dc70a8c7e2fcb2379b3abb6d39314e1db48` | completed 31.575s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208676697; ordinary 3620eb052105cc3ebd6eb6b6628c6eeffeaa60e6c147b116d80493a53a1e4547; finalization 2377ec10113a5aee48db4195dd5014a722f1ea4629a198077db21c34cd21f6c0 | ordinary_finalization artifact-675a738932aa97ae1ae77006a2dba08e/2377ec10113a5aee48db4195dd5014a722f1ea4629a198077db21c34cd21f6c0<br>partial_report artifact-87a0bbde30d59f5369a2c821bc0a0448/6fc8463e6fb4284a79d2f76092b057dba7031d484da8e9ace9abb33df054be06<br>report artifact-68f4b3c0da163b004ebbd0b4dd14c1cf/31fbdf32ba617efe5f9b6a9014fc2bef01259bf78859c75ad361f23b8a62890b<br>strategy artifact-1833774bc03fc4718023a8a5dd8150ad/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-59f0d63c129d4d68b639a95c8a73a5fd/2ef2b06c693ca7cc7d442e1c2017cab10e6229f1d88e40271aff813cbbd59fd7 |
| `primitive-addback-essence` | `case-rev-ab9fced90ba724afebc08a75dab8c3fa`<br>`ab9fced90ba724afebc08a75dab8c3fadeb35edbe2f703d08012f15eb736894f` | `job-879587f9-6d33-4767-a0a1-83bfa0689f58`<br>`attempt-9d718048-515c-4445-ae79-db3248c6aebc` | `e9bf2e85b3593e1ba8f482786f2f1265167cde552f212c35a9e41a8def6a1adb`<br>`bf90c99708d1229ee1ae854d4311603e6b851564e4a72b777091b6c943699e0d` | completed 30.902s; exact_closed; phase done; samples 30; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208676697; ordinary 31c893eed152a3c34428b4504cf43af6b0ddce5b7604ef309adaa4d7c674c5ee; finalization 6560f12103b0a38165a3a2bc34d9ac475fd8d86403aedfa4ae1d50494479cfb3 | ordinary_finalization artifact-f9545d171344844ad9be640c8c9362c9/6560f12103b0a38165a3a2bc34d9ac475fd8d86403aedfa4ae1d50494479cfb3<br>partial_report artifact-dfc2094ae7472139a28314cd9d633c1c/fec162e3b57e07b7831f15e02cda6e09bcbbe6547bf2d462a03d1816fca529e4<br>report artifact-1d9e26f0cb515363a16f792c27ed91e6/ff3fab26cd45b45b75be3149ec0c57a7fabffc32d7fc9a32576b9f1cfb53e5bc<br>strategy artifact-35a017cc50a8e7576da5d4e37d5e99cc/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-64d987ed7993ce4598f79894eebf8912/c563aacfd37f2bbc8b06422b91c25a78bcf6ccaf2a1934d655f84c15c84f95e9 |
| `primitive-addback-fossil` | `case-rev-c1b3c87d43e16122687521d348cf38b5`<br>`c1b3c87d43e16122687521d348cf38b5db2a1c8ba748a130ef15f364ec4aae88` | `job-709e39e0-5ead-4fce-9a9c-dca35cd726aa`<br>`attempt-2db95f4b-ebc7-4fab-a17c-e4e55a246cb8` | `d7b738b220c0c79fb28a49a2f4ca149c6eda71c8880e4933dda40c8651474a2e`<br>`3480295e38b57c3717f83860b5dc47db40d6cf9498d783c41ae4d7afee4b1477` | watchdog 60.161s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-a8763b2d4febf4ff0b6e103695c6733e/8958f6797d0b53eb43db1f070db2490c3403c9b112b7c24bdaeaeeb8feee682b<br>worker_log artifact-b05c74f142bb0ddde4e73b6505c41fab/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `primitive-addback-harvest` | `case-rev-e3fe8f0730107a2c5e187c0b17ec78f9`<br>`e3fe8f0730107a2c5e187c0b17ec78f9a864dcdf146590865d1cbeecf5c7826c` | `job-1ea82694-b009-4768-bf01-b46b6676fe2e`<br>`attempt-565188d7-43ad-4339-92f0-e0e38c4941c5` | `00595edd64133dfe22e9efd88c45e09b89d42a49b02fb86ab38654707a6778f2`<br>`c303dcacb361d46db328871d1defd5364c69d14803d2efd817d81f436e33ce91` | completed 14.094s; refused_resource_cap; phase done; samples 12; states 1347/1347/0; rows 7059; transitions 25561; reforge 1415110; peak 77520441; ordinary b02303fd7335a85a16668eb9e303c44ee14b3b745ecc9fb522dffec77a6c6a41; finalization 6fb46b58e8bacccda75913f99ee1adaf219d1a0eaa46f7833d356d6e85d2ad46 | ordinary_finalization artifact-087ecc3e2d6c8a06ef318b515267d411/6fb46b58e8bacccda75913f99ee1adaf219d1a0eaa46f7833d356d6e85d2ad46<br>partial_report artifact-c0dbaff2a9c8d8817ac89b671303582a/8fb576d2b1bb897818491ac72431ed2ae66ba4e73569379e1060b2076d0bcf83<br>report artifact-3ae02c2b304b98b3ece671f0ef21fed0/d7a5e767e1668acc8599aaf400f240ea43c2afe75502f8fa1a24d25410e884fa<br>strategy artifact-19037c5812807e052ac318c84029713f/2d2296b5ba04c989fbe0c5d6d3eee16a33f61f46069b19f25c2052c9af1df377<br>worker_log artifact-3306de066bfbf4aa236fb0b694274cba/65a0fedb545edd3539adcd2f1ace7a49453b0b052386150c7c659afef8140a31 |
| `primitive-addback-bench` | `case-rev-9607abd117887da682fa90876a29892d`<br>`9607abd117887da682fa90876a29892da8293102458474bdd8b4c09957d8aff3` | `job-397791e2-01a7-42ef-a2f3-c4aa7749e1ca`<br>`attempt-3a77d548-31d3-4c02-9a9b-b0f9894f04f2` | `afe1189e41954f6c2cf4c4305f0d9e55fd6ad9a52795ef2d27131cbaa0cd6ef0`<br>`58531fdbc04ff646960fb0c622e570bbd812d26a32a4e54d6b3e4543a951e009` | completed 31.427s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208781561; ordinary b7ce5075db84b300b3647751e09378b0693a7ee7fa562e3aee1c88ecdbbdb027; finalization 92a8a3eb7c5983ff97be543aa606781461319a676cc38ebcb777a26b5a8067a3 | ordinary_finalization artifact-ff44d2f863ae0eba4aed3dbff9a01ab9/92a8a3eb7c5983ff97be543aa606781461319a676cc38ebcb777a26b5a8067a3<br>partial_report artifact-07f8645b7275ff063bf3d53c4ec1abd6/2fd1973456de60a71951f1260a6ad29a0924016e900fd0d0a9c90c4f81ce2830<br>report artifact-c37778cc089f48167157065e0a60d574/d183154b51672b8b177eb2119fafc8f1e4c886681a9eff889f6f916755f94f09<br>strategy artifact-1261f39e43d03fd8511f9ca99af8d036/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-7e7c5cc1c1ef9dd80b1d649a83577b84/dc73876d0443fc78956c12dc9326faf158c2fb690023a1b7f871c891838335ce |
| `primitive-addback-eldritch` | `case-rev-238e3156a2737b7597f0dee284b4e830`<br>`238e3156a2737b7597f0dee284b4e8300f63c1220141538d15cdcdcb6870192c` | `job-14231bfc-f7ce-422c-8276-0dbafc313cb1`<br>`attempt-ddb416f8-cf2c-4df1-85cc-da09c16d6fc7` | `8ff1382b3df2d5b81f3fa6453d47886db12c0b77ff28c63ce9edad2fada364af`<br>`88b7b60ca85a8a45aec3a14bd3923897b24610cd0453bbb695b4c006d1f62cf1` | completed 30.827s; exact_closed; phase done; samples 30; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208781561; ordinary 7d1b63ec10348eb9c524e3243bbd23e0498411025b1b5fdc65635ebf3e04a087; finalization 0f6f4e43a87f25359b0db6c802a3b6325d40682071fbcc07ac91eb9ef7bf8ed7 | ordinary_finalization artifact-e742fb03d7d9db9abb7709339c92a361/0f6f4e43a87f25359b0db6c802a3b6325d40682071fbcc07ac91eb9ef7bf8ed7<br>partial_report artifact-b2dadf7b1a037bf9b6a2bf7cd7aa9a82/43bb26b7206a8ecd60ca753e43175f17b3bd27126950ba5e9cd4f1955c012508<br>report artifact-6c59335f043dda29d68bc1828a476859/ce84fd74bc2cf91b28d3158ede14ae64d05c9094a957bbf839f9667ee3df945a<br>strategy artifact-3a12030a1be818083659f676eea97d85/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-b87d1840214c62ee725d5f8646ea3cef/3c34d35e5a8470db5dbe406b6ed4a0d55102a908e69ea5c45d0b82dc471f3dfb |
| `primitive-addback-influence` | `case-rev-26805a066896a08a62680f20f5e38423`<br>`26805a066896a08a62680f20f5e38423ebe2fb8d649cd1190748b5f3d0f2c0db` | `job-d565178e-f000-4300-a1f9-22490db9c32b`<br>`attempt-cc7b9f15-cfe1-40b4-9d64-3223347659d9` | `1d9b944de24f020ac24fef47f5130e10e911fcb22c1bd64792621cc1688e97bd`<br>`aebf6311fc0cbebdd534cb699aaad6ed0695f72dfb9fe675f54b8863fa2fbb67` | completed 31.498s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208781561; ordinary 9c6fd09aaafd9584e4f0ca05fd85d2b5dfb69133a47732c4efe41183749fdb32; finalization 520d80c3dcd7141f078f5de87617d5a3e5cbfca6edf0c70c30772aaf1b68abe6 | ordinary_finalization artifact-035f3a7f2f8ee0f0f65525754bd685c7/520d80c3dcd7141f078f5de87617d5a3e5cbfca6edf0c70c30772aaf1b68abe6<br>partial_report artifact-75bcb850bc4833d37a8315483593d6c8/3620dab8c7360e186324fa4a10dec4ef8f2a0f6d06bf562d24e7fab084f08e44<br>report artifact-40623ee3599087ec99340ffca1fc0651/372f84d2dde9a1a965ee9511f7fe204cee5458594c08757e519888d005474363<br>strategy artifact-a26334a519b3875d3e6ee2a3fd81114b/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-5cbeab4e36f6fd166fca95d6742f30a0/ac20ecdd24cf8d3482fe3e7373606319bf7e8c5be4a43b85b391f13f01a56fd5 |
| `primitive-addback-fracture` | `case-rev-fde04fff27b8d684f2b5ebc8e5a9970c`<br>`fde04fff27b8d684f2b5ebc8e5a9970c3294dc57c8f18c33939dfb5e5cb6d98a` | `job-0ebf6b2f-58f8-44bf-8b44-b30a4dcf6567`<br>`attempt-233989a2-6d31-4ce3-9d35-1dc43a8d316f` | `be6b298d6bc7fc82816240590c7ab1ecf661e441dfcf4f890b2f62f38be49268`<br>`efcd034d06ae42d14b1d54863fa61ee75b76b9fad732f14150b481a439cb1142` | completed 14.216s; requested_bounded_finish; phase done; samples 34; states 6573/6401/172; rows 25101; transitions 53013; reforge 480516; peak 69490860; ordinary 1ae5795270c080f23550777072c626d164e2cb2435815573b7e0c7a883c3b4d8; finalization 89adee3e822bb7f9fadb3d38df0c9cb14c907586bd44ac4956b3063f73899fab | ordinary_finalization artifact-ebfa0438d266c315e3214b76c6edab54/89adee3e822bb7f9fadb3d38df0c9cb14c907586bd44ac4956b3063f73899fab<br>partial_report artifact-2c4383d1ab25bdfbbace0be19c435449/50fd9d97aea28c60678819a5b5e4837d094ec5e7879332e2f345791b5c6b6c97<br>report artifact-cf4b313bd8ebeb37118709f109bdeadd/fc928c016706592e1ee030611e03baf8b46b07e29e671632aed90a248b5aa5f7<br>strategy artifact-ee632950c0d88a2134ff55a39ed2ada2/bdcd2185a0e6de6d637a648e426a5da5e7db5e85cb04f73bc14191a5abdc5a29<br>worker_log artifact-76c3d225cadcd6720e1aad4588125bc5/058facb8ca1dc58d84ad970d092d7a8ca884552ebc53d954c10adbf8bbe3d1bf |
| `primitive-addback-veiled` | `case-rev-b8d0449b36de4a8afdc633c6aaa0b6cf`<br>`b8d0449b36de4a8afdc633c6aaa0b6cfca628ac128ec72e0d6c0e70439c72209` | `job-46eb136c-caa1-4ed0-a7aa-6a0b8ca5e188`<br>`attempt-e8301426-0762-492c-bf06-1a28aff1f245` | `cf6de1a3fd125f5edd855a4b4b3e42d3c53b5673ca804e9a714f9bc66097120a`<br>`93d644e71a0d0f7abd635438f45f858f86acc30a67732a99a186fb4581d0664d` | completed 31.648s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208676697; ordinary 0cc426a9f98f54310c659386886002b4ecf254c924d4be3ca16ef43d38962839; finalization 64c188a1185ad33a6c9b23b0038a0fce4f9bff74f41f0f49c674b68da2090536 | ordinary_finalization artifact-32d88e2d21cfbd2d5dea7a356e54474a/64c188a1185ad33a6c9b23b0038a0fce4f9bff74f41f0f49c674b68da2090536<br>partial_report artifact-d01a041dd28ab165f66e1a0b2bfa34fa/9c5a16ddc1075aab042a3ecbbca4687edd5e1f4e02c5f942b65af3bcf1c34890<br>report artifact-56348b33c29e420df94f8bd44993751f/dea70b5d56a83a94dc84d97437971ee01f466a9f8cadef6394581510fa9dd9c5<br>strategy artifact-9b1cf78f63063510e49d6e6d7ca90681/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-41e61f18b9cc58a2dac2ab6c01756614/fd0015b71afa043e7925693156e075261d20b94467241099b76ec3ebe650b3c3 |
| `primitive-addback-cleanup` | `case-rev-91212f8d824785603fb5972a9b7a2055`<br>`91212f8d824785603fb5972a9b7a2055c6bb85dc1de1c8a081e1440d41ce800b` | `job-e51b736b-c159-4fc4-bb26-3449cdd481c8`<br>`attempt-1ff9d9da-4659-42c2-b478-c1bf0021565b` | `bedbfe2df22abe78a7b47db88a3b21e6698a76d3ed0a1c2868c92abbd0767d30`<br>`06ebc38233d33d9494e40de12e4be7bfcdbfd19f302cbd1bbaa752f230991d05` | completed 31.306s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208781561; ordinary ac2a13c3d1871ebbc3ab11d23e62f882a572c66be8ea0cd1a962c08d4d72e253; finalization 9b6ed727e9ebba7e12e22f043662896fa41bd67895e5fccc63bb63a00193c396 | ordinary_finalization artifact-da4e32a960e0b357bac1388ae626a12a/9b6ed727e9ebba7e12e22f043662896fa41bd67895e5fccc63bb63a00193c396<br>partial_report artifact-57cde01fe7d68029598b6adc07bad0b6/721951e2213acbd217e5231154b8831c48012aa6e3fa04220508443d60293da7<br>report artifact-45965abe24716e71eeec2ef354c23fb8/d520ede3f814c0099931ef76e35dfcc57480e2887ab74a5ac5b7de312bfb5999<br>strategy artifact-5f3f4f06c82936b8a1b7a531329ffcb2/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-b3acbbb2f42d098d57311f867e160039/5d7949720ec543c14f93ec8e3955a3f50c6291b10a50eea332661d222bf37159 |
| `primitive-addback-temporary_bench` | `case-rev-d0879bf0f7dfb2ad69742636312932ef`<br>`d0879bf0f7dfb2ad69742636312932effff5fe218d18d4266e4f25ce9592da67` | `job-be162fee-0eda-44d7-9a21-87dc8d24c7e9`<br>`attempt-ca938fc5-9cd0-4ad2-82f2-5b07def7936d` | `5f0024812a6b78bd7a0c8df62050b1bc1100ca012ddf45e066ad9a6083122a8c`<br>`5a2a7f25e528d0a87969e261d95637cdb75b710d2539b8b39de87cce7c16f249` | completed 31.085s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208676697; ordinary 1a62bd18bcf91c24f423ade9be089b1387e4f09dc55e01bfdd1a6caef0606f94; finalization c50dae7749dc8a693f157bbe63620ac7b5dc06a6ae17bfec9ac0409f3466a03b | ordinary_finalization artifact-b0a46200bf35a6aab60a8051d1f6dca7/c50dae7749dc8a693f157bbe63620ac7b5dc06a6ae17bfec9ac0409f3466a03b<br>partial_report artifact-48f4cb7a9aad96191811fb214c0acaed/5b33c709523f282c3895f7d791f3746b5b8fe37664e28e196fcd3368b88b2cc8<br>report artifact-360c16a2fa5ba403f609a29b08c9235f/8c728c277d0f7cf02f5a71091ad101bc0883d04988f9a53fdedfb07ec46f1519<br>strategy artifact-d76fbc3466c5ed7a39d56a2d75d303c6/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-e870d8b35315030abf116900110eba43/d29a5e9d7eb46a8544f50c614a621356a1e2e1ce82e5d2210d320b16f042a84f |
| `primitive-addback-metamod` | `case-rev-dc79c9c984ffbeac9357fb6566b2a89a`<br>`dc79c9c984ffbeac9357fb6566b2a89a0e8afe1e60cf028a21c57fd98763af25` | `job-52e44e4b-b9b6-4c0e-95dc-e14ee9b79f09`<br>`attempt-831e912c-f08e-47d4-8f02-09c494203b63` | `157e289872ff7c67d1529bfb40e216e211ee1c2f6a058b528096a02c938792bc`<br>`a92ad2d9c33859cffaa63c0dee2a1a1d9645879c26d45808b1a9ee6bd78c9207` | completed 31.194s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208676697; ordinary d918befaa6249aef970f39db351b64e5e3343f9beeaf8d5ab2f099c39b12e5a9; finalization 7361f92c8f29a20c33072fc82360feaae8727c8563776fe18e30030c74d5b50c | ordinary_finalization artifact-9ec4676726de20f8fa82016e7a6467ab/7361f92c8f29a20c33072fc82360feaae8727c8563776fe18e30030c74d5b50c<br>partial_report artifact-68d18f154fe8056a54c0c124defae16d/8d446c357fff38babad15380bd4fad73937ea425b08f77b4fb72415cee499e7f<br>report artifact-00458f42d114745971d3a8b0d4b5572c/7a917fb68146d2c6aa53594ad58e6f80721984a7685308616404fceb20fabce4<br>strategy artifact-3139090957e40a1093087c71e17df07d/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-2b8d2ad8f5279ed2a64fe692ada15873/9b883538ade6b74e9c01765c23f2badaa00571a39beaf974cbbd5db9a357764d |
| `primitive-addback-imprint` | `case-rev-266742b25207522b0ccdff6f03f3855b`<br>`266742b25207522b0ccdff6f03f3855b90ec18e82c4352793c11aca1ea3f1584` | `job-d334b5ae-d93c-4d64-87a4-9b5a9082a9bb`<br>`attempt-2ced5bcc-08b5-4fe8-be96-c0decdd1bd1d` | `2f2067c356168189c1af65ca914102df3f457483eae9395acea12b30cf82c158`<br>`e5392f494732966e71fb637e9455e382086bad0312ff676f849f33c4c747ee27` | completed 31.106s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208781561; ordinary 315012da8430f1e3091ea3f7a83a2be300835627119a94f04b306fb9d329f926; finalization eebdf8565e908b16e18ed4c7bfd3efe280f61a15bd12c0f8d442286448ce5ecd | ordinary_finalization artifact-a0aacc1e09f28000995981e97a78dede/eebdf8565e908b16e18ed4c7bfd3efe280f61a15bd12c0f8d442286448ce5ecd<br>partial_report artifact-cc0a42ea3ae287d4fa8f9e3f0b998b68/08547d4d9b4fd4f65b43893fd71ef4a6c26459afe37526f6913b491b2e96779e<br>report artifact-54a87d82d90985cfd822dc876a82f4ee/872fedede33aefa841914582fde9c7e6d1db61bcd21dd4120d38561bdd6d5377<br>strategy artifact-6c7519995c60897046febe79a2c36184/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-dd7cb4ad0c2839509d692b4030dd41ca/ee06a0504185b67a7f969a87d1f2fec3e1cdee2ede6caaadf67af3eda98faf57 |
| `primitive-addback-restart` | `case-rev-44b49ff7bff5b623820d1aa0e10da7e4`<br>`44b49ff7bff5b623820d1aa0e10da7e46ef9ccbee7328bb5610edeee6ac87650` | `job-fbbbcc94-7334-491e-b4d2-7c1d0c6b2130`<br>`attempt-ec55a50b-6002-49ac-855a-766abd3e22dc` | `24b282568dd6ef01c59d11ff005398a113ae7b7fe308acd32984efa1d595614c`<br>`b581e759ed3bd03882e9e9ba2cf807c2448d318a97029627018b0df6283cbbe5` | completed 31.308s; exact_closed; phase done; samples 31; states 603/603/0; rows 2299; transitions 4424; reforge 459414; peak 208783314; ordinary 2f0cef2a582fd0a5155aeda58636adb72fb3f071592d07d4e56158f851db7c70; finalization 878ca2bbad4763d311d5448e5a5fed04e9a4fdf6ed2ff1d08647efc1eecf03ba | ordinary_finalization artifact-3f14e607638baeb277ed68cda6605e6c/878ca2bbad4763d311d5448e5a5fed04e9a4fdf6ed2ff1d08647efc1eecf03ba<br>partial_report artifact-223a58417b52611c45ed2c9e2e628d33/2c4ca1ab274c4aa1029de2c9085f3c099c93bc019f89966ee5a69cdf73386f2c<br>report artifact-81a8e3f7474ee1537536c49ec9f93da1/d264671e852c02fc5e016a98f2270e354019fae1770bf959ec21460a801c0fb0<br>strategy artifact-801d9f42c54a588e76654524d610fc8e/0487e17511c0b81d8495258069faf1188f5cc359e8804eb0dd47ade8812df529<br>worker_log artifact-05197b87aee69a1cd00222c92e87a036/f6c267aed1de89ae0702985ca78b03e7de65c271d6008c3ca929a585ce9b0953 |
| `generated-singleton-fracture` | `case-rev-5e750fcdc6cf3dd968056ff5beec1860`<br>`5e750fcdc6cf3dd968056ff5beec1860b4152479bfb95129fd6e962b8e450d7d` | `job-3629f971-a987-4e83-b18f-c32125b28dc5`<br>`attempt-130906a6-94bd-4815-9a41-a1393cbfb09a` | `5e9cfd4006ca4034ed5c430c510f191308effa7992ab05644ce9a19a1786dee3`<br>`46ae94fcc84df9b75991357fb85c192a5852c2c818909168d69c7152611e007b` | watchdog 60.215s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-5a50878715738ef3a0d23ff2b87eba8f/767e18138edf966e1910c31ba11b226e3f283a7f35416970815077fc4fc1b868<br>worker_log artifact-1971aa6296629f78b9e8412c71113468/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-permanent_bench` | `case-rev-de45507c79a38a726fbf7b118b2fb488`<br>`de45507c79a38a726fbf7b118b2fb4889744fb332fe9e8d5c0b0fc05a3e96f81` | `job-91f29023-25ea-4162-bc34-c7cafb80c946`<br>`attempt-60c6a360-026d-436a-b048-058f55b9a3f4` | `4b9519cb49325f341b6a2f8e73b4d44d0053266203f0b5d60cc03ac846389893`<br>`21170f787afbaee07c90bfb19e8a46a3dd6f8b290f5962429f2c2e879a4a8b4c` | watchdog 60.198s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-5242ab07113cb56c578dbdbbf97d1e67/8a30373532fe94dba6e04b194dfad2c2412bcd6aa82bd8fb45479f471c19ab5d<br>worker_log artifact-e3803b57ea10c30138715d18158e136f/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-temporary_bench_blocker` | `case-rev-9d4bf1a790479d59b4479b0065767bcb`<br>`9d4bf1a790479d59b4479b0065767bcb3eb7f4eb96f05b491d8648f80bf5c14c` | `job-150c16fd-b634-4192-a040-cf679875af96`<br>`attempt-693cb48c-99d1-4301-a72f-eb4e0ff7259e` | `79a4b2fb9458c4e89e286004e4cc9a64de21a3ef2519b2aebe311c03d0719be8`<br>`42ebedc8e9050f81e03c148b5cebb002d12fc566a6226a054f1377e582b3baf9` | watchdog 60.159s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-21cfb99a756ace8dce487d47779cb01c/058db84f3233f638faad2710b86b0ce0b638b9ba96708670ad9e76864aa8a694<br>worker_log artifact-570c58cbd67753d39c3a39befd3fdd4d/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-protected_metamod` | `case-rev-7e669213b40d59cb0a2b4bf1de211734`<br>`7e669213b40d59cb0a2b4bf1de21173403ea04c9766edbdb0dcab7582523195c` | `job-e2cd396c-8d73-4a18-9110-7f4c219549a5`<br>`attempt-a92782a8-8950-4eb7-9a21-b18533dad0d5` | `4b15a7972405f832b85f679f92fc8d598b6943d03333b8b438822afbfbb7328e`<br>`f875f9a434d20d1115f0af66dc7037fee2944af4e109ca6b29b993576f9375b4` | watchdog 60.158s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-98f6eea66f86b0d6a6b1b73f2014415c/d8ed254587bb6f8b3e62760018e92e1f0f1bedb3d41f01416e6dea78255ca713<br>worker_log artifact-755526e970ef0f8e65f200053666336d/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-multimod_finish` | `case-rev-ecaf357cdd372d0db461be093a0b4d5b`<br>`ecaf357cdd372d0db461be093a0b4d5b68160462272dc08a1a132b2721da5da0` | `job-4de71786-b8bf-4c19-bcdf-50e833b18daf`<br>`attempt-e6046497-0f6a-4aa3-a087-dd992f654e1b` | `c0c9a2346e49961ec3afb4d32ab8faca5b2688ff8278d2cdf404abc4d27b6a3f`<br>`9cef0553b974be4d8b83c9dbf665e584b50f006703eb45d118e3ea79c52d08fe` | watchdog 60.204s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-242d5657dcc76b469aaafd059daa7c62/9142fd80e463a6340c0a5a2e0b591b9bd6a551f7d80fa5fb63a1fec0da87ab45<br>worker_log artifact-1d15b102ea50cc487963e0a65fa51afa/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-imprint` | `case-rev-ee59a4eec9b98bc8d734bbf5b6ee7823`<br>`ee59a4eec9b98bc8d734bbf5b6ee7823e8f4920aaab521877f0650e871559d10` | `job-30209659-5c4a-4902-8b59-e37288156316`<br>`attempt-5a49dad1-c0ba-4d2c-b9f7-4ed9d6e42fb1` | `47397cdbe41e3dc1dd3f23d0a253a497f8bcba0f210a1788343342cc3d33b8b9`<br>`9beb65650dc453a3080b22b79603f9b7a513d989359e26b5f6d1734f0f7a780e` | watchdog 60.156s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-80e1089a7e94eb8bcf22496ad4dc7bad/b76d685071e71046ee7ebe91e6b02cb4f81fc244e1c4674b0c17553818f5bd55<br>worker_log artifact-422dc913a07cf05dbdd356d20c4121bf/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-constructive_renewal` | `case-rev-42cde2d904178d03c9d799c2cff190c4`<br>`42cde2d904178d03c9d799c2cff190c47cd526a2a1fe345bd8990bc786d6d570` | `job-26682758-57ea-4493-a6b3-2a9e1f3f24a6`<br>`attempt-d9d87bc9-408c-4611-b417-15582e536602` | `1e07e7d4833c290108196219f8addf4ec7b9d083ac93abf642e6628569ca13b8`<br>`230c2079442ba65bc7134a92ead77127509bb2aca8cde0e120315875ebbe141f` | watchdog 60.160s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-4a413460aa72a963db56742db16bf9b1/eadbfe6035e2185aa1c51e8aecf92f0d96046a7ac9d22d4ac905f903289f8523<br>worker_log artifact-b5ebc2f9867d1f38f2a2ece2885f2a58/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-eldritch_side` | `case-rev-b39170a4e68b0f022ede6cd745d54a79`<br>`b39170a4e68b0f022ede6cd745d54a796f1f0fee80b143b5eb8435667f0444b6` | `job-f86cd265-c3b9-438a-89f0-d96d42359dcc`<br>`attempt-37b2f348-b5b7-4dd5-9aac-9c7cd75df248` | `3f27e0fddb0f190b6bfba0685091f3682c6101348c533d393acb98bf62d0e412`<br>`9707ecc2cac0b0421c88f6d0e5097f3b4ab63ed2a3feffc376220c9aa2e2a794` | watchdog 60.221s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-fb67284f3f3be0ad4d45f96eefdcfa84/68e2e32e7950cda03729fb28f1017917a9db6626b3faee2e8a5e69e53df73732<br>worker_log artifact-3c0c2ada030d83325cfbf023c5cf13bb/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-cannot_roll` | `case-rev-834b73a86bcd5e0aa52fc4b482ed182e`<br>`834b73a86bcd5e0aa52fc4b482ed182e3fdf66be7db5c4b9d3f846cba3a1dd3a` | `job-dceca0d6-e281-46fb-9bf5-a4d0f746e378`<br>`attempt-f12e24ad-86c2-4a1a-82c5-e6d68e409f1a` | `756b5e25a6f445511f57798bed411be1a7bf688466f711de8a4df6fad0d9bf2f`<br>`c12fd96fee3dddea4eba627fded3cfb05a96d2db535ad044c0d5545f91a339c9` | watchdog 60.151s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-e88c58b01a6195e96ee62b509e0a288c/78a5ce3d6fbb14960a19493975336f741cf8731b1cabbb850263ecae2c4e2119<br>worker_log artifact-c79092dc3cd7acab38280c5beef49a77/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-singleton-veiled` | `case-rev-852fe6f623d679063cb5be3919c66c06`<br>`852fe6f623d679063cb5be3919c66c06901b39f59d5f2e977fda9e9af30db9f1` | `job-6071d719-e5b9-412b-aedf-e4c7037d24e7`<br>`attempt-eb2af48b-b996-4a37-a421-f83f4535cfc1` | `bf751c0153cb96da1cd0d320d258aafa2c1a877206a60401ccee28f9a141a36c`<br>`5dc6e93ba77f0a395cee4fa2dcf302188042361b83ef518c31b131cc0915b620` | watchdog 60.154s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-a49ba2838ee1380d886a41f755cbe8ef/f52db46209a11c3ebbb7e8c8bb3ec6e1b27aff7d37e79c0c2208b3e3b627fdc5<br>worker_log artifact-3b5d9f4afde811399bc1bf125986e82f/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-02` | `case-rev-4732e0eb28890e7e09345b7bdec5d38c`<br>`4732e0eb28890e7e09345b7bdec5d38c5ff92b623f673d0a0687d5487a2e53ec` | `job-568a55df-eee4-4054-a48c-0b0a5d801d0a`<br>`attempt-e2073199-a716-4333-b0bf-9dbd43d5b8fd` | `a5a8b7e2391b440b71f479752771d00a8811c3fdc9a1789736f75e5c8bfb9f86`<br>`779d80e634fbcfaf37b1c51b5ad86e01f3f46afa3dc4ae0c704eb490fad5a25c` | watchdog 60.198s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-2f04b47fe4438d482ee690d8e64f6bc6/63096d5e6a8fc3f860e86f9785f4353e74c6c3ba44c71c72f882eec2407e3af3<br>worker_log artifact-528b6dce264d7bd791b25fd4d9741c53/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-03` | `case-rev-0a1d5f7f08d0248070657b36814a4044`<br>`0a1d5f7f08d0248070657b36814a404454b69c3aa280c1951c7fc4328132e585` | `job-eb2e5025-16a8-4da5-87da-9e6c24721a97`<br>`attempt-15eca153-b80e-42c6-8cb0-2cae0f860bb5` | `b0c93ffa3f4301fbbb56cec267f7058d172e144e20c9234da8c34fdcf5b53a78`<br>`916055260ac950fbc47f2dd282ec3c6ebdd9100ea9d6e77d42fc840d7826073e` | watchdog 60.193s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-16e9dd0db58f72a0c6978f687ec70a71/c19235843d1c4e5143a07233cb69ce4b982004f2c8c0f7c0843542e84fc2fea9<br>worker_log artifact-283dc07aa3349f7681b16e7f3d49e02b/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-04` | `case-rev-ddda8c6ac7e86b6afaa1663ed8cd4fa9`<br>`ddda8c6ac7e86b6afaa1663ed8cd4fa9dc7b324c9c0222fe5d2dfad6c79ba6eb` | `job-65c629f9-9bf8-47c0-91bc-acf04173c551`<br>`attempt-707ded53-2f13-415d-af49-c15e4ecafa74` | `90bdad439544cd64e509381a62cadb97f3da2135395e1950274300ce04bbcfc7`<br>`b2d82b7df5d130c5b4507680140bda23acb9d78e4a48774425dc0d3e802fd36a` | watchdog 60.177s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-0d59c563edad8140439a4bb88b836016/0f08cced5974ad6082ab5dc5d6a3afa655364061062af7d32b0481ebb7e6bafc<br>worker_log artifact-63477c32840ae6be5d7fc87cec4c3d41/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-05` | `case-rev-667a31bcecb9bf1d31a3d8737026d318`<br>`667a31bcecb9bf1d31a3d8737026d3186a036ca254486454880d5bc103a1387f` | `job-a7504cb5-bfba-4988-b0af-100354855c4b`<br>`attempt-673f3a3c-f145-44a6-befd-e2c47ba0ad26` | `88a1424b0b63d0ce9df02339e92f7088993b79e18b3901cbd9d8d1b6f0576343`<br>`44618f3653f2292697c1071ecfccab8bdfc0e7d9c7df30d583bbf5997574cac7` | watchdog 60.177s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-fcd4e9805f92639681ac4853e88a884c/79f67e60a262e4d0b11f9bf2a91eb0337bf0370cac5c9e207ff43705f5853262<br>worker_log artifact-cfcf0f3c92bbbe66ace20c68b775f3a9/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-06` | `case-rev-c650bd6db9123b404d28f4b21997c21c`<br>`c650bd6db9123b404d28f4b21997c21c39ca984cb624c944877f0ba1fe5478c8` | `job-d558891b-28f0-497c-b4d1-1c33a17f8c13`<br>`attempt-d2ff700a-5506-4f57-962c-53b40e50f258` | `f3812f9a7c216def0ad11b7ae946e648c53bcf7bf15846db2ec2be4ae28f96c5`<br>`5737ab55c1edf1b5212f739c57879f07e06dc75a65e1111ea0c83fa17cb9a202` | watchdog 60.157s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-a149e5cd7000e359d83b4f7c10dcd350/18e048faca7513586426b271c77e1b80c3770a26198584c84415fddf9f06c986<br>worker_log artifact-3291a084d69fe6d60ee94377bb6fcfd7/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-07` | `case-rev-60b7be25915086a12a26983da7e42c01`<br>`60b7be25915086a12a26983da7e42c01be5fb266dc27da6fdac70754d9089d6b` | `job-d4d8e43f-0281-4e95-b68a-bf6f9ff11465`<br>`attempt-0e949ce9-5988-478b-93f4-ccfe0c757f84` | `dd57c277520ed58e97051c6c1364a55ce8df98b81362ff871df4050a5901b57d`<br>`d8ddd0ce795b322c3675de9fcd231362961e250d540170a2aed28e3d67221c75` | watchdog 60.221s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-3844e686e9a529721bfa5745296aefc2/e99a1b12cfcc11a8f4822123151c606e313de07012cf3eb9dba8207661d60f9d<br>worker_log artifact-17fe09d87232a0e00f84ff648bd1379c/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-08` | `case-rev-a9ccb69d6065b097e29771fd3193545a`<br>`a9ccb69d6065b097e29771fd3193545ab75255937cbc207dbc6467117da56a75` | `job-1e269afd-ff22-434d-a59f-5a09c95e1a9b`<br>`attempt-100fb3ef-c68d-427a-b29f-5de59cbf4a34` | `1114ebe572d5bec9f7bb26906d849ec74424503e40207821d8f7080bfba5dec5`<br>`b351185628507bc4c3ac0bac57c34a7c70394863fe829104c2e41a5390776757` | watchdog 60.167s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-f469cde6f34be0e47d52cac9d740d01a/59f5dbcc44c8c593d009bc89698b5efe6996c0fd74e67b8c8789325ace038394<br>worker_log artifact-8ef025fdc1ca7a5e672447a84013d48c/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-09` | `case-rev-5a95c9d0b1049ddaf0f310b81053dbd2`<br>`5a95c9d0b1049ddaf0f310b81053dbd225255bb670f23eac39211fa98e8e9873` | `job-c58a00fd-12c9-4b29-96fb-1e5cdea5214f`<br>`attempt-bc7001db-32bc-47f9-b533-2cee8bda082b` | `ba44290998fd1aae19e5ab5462d72db7f5f81c2f5200624c5c97b363b80d2a10`<br>`f61410ea6f1ed46c1f7312efbbce84c88e840843dc388133413a26be2aca6343` | watchdog 60.162s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-775334246567cde9d4dc3a084acd6ed5/2afacc7bed1436a74577ea67381ee38668a3f4249268ebec3a9f27e7df001bfb<br>worker_log artifact-a8157b5e8f85e4774f8a9960e13ba5aa/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-cumulative-10` | `case-rev-9456180796c2d052e4b0b9b8d359be01`<br>`9456180796c2d052e4b0b9b8d359be019b3f2e94bd5d74a4cd7003df10580f10` | `job-101ab74f-c43d-4571-b5ab-f4d17f13f26e`<br>`attempt-a2ce7a67-dfe9-4250-ab51-b4ab36096094` | `e292bb9b4e89e18a027e38cda923cde7c5a8e619c174b301967659ef2649a53a`<br>`4b3c77bb78ba0c82a1e8bfb5f2364a679d4df0d7986b14b8eb3e69aaf4cefd9f` | watchdog 60.174s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-f7f4a0efcade9363a01827ac21d44b92/9edb8d3e377eba19257f12dd63f43c1ef955e456bb1139fec8e31817c3f7a836<br>worker_log artifact-4311d6880cd176fa0bebd052379447b9/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| `generated-baseline-none` | `case-rev-ac23c2eaefb561d22f4f92f7bdd09f1b`<br>`ac23c2eaefb561d22f4f92f7bdd09f1b9c12428dff282e3ceded31b4bfb9b5aa` | `job-00c3f821-4805-4966-9b8f-6d6e14cf12ae`<br>`attempt-e0586bbc-e393-4d65-810c-d7a2a53233b6` | `35e7978710bf33261ed24ed28b9504f0de4221a3a88ebb6a4488e16cd1668dc5`<br>`b758f8d5e4941ae3e3d757d1b14a05fc6d6fe3873e2cb14ba2b8eb704655ee10` | watchdog 60.204s; source none; phase unavailable; samples 0; survivor false | supervisor_error artifact-8b2c7e15d986270fa750a6e9754ac517/a233257ef5cefc4d127d67ad67f5c04639ccfef179db2fdaf96a83651ed17202<br>worker_log artifact-3788816109d582e4fbe64403de42e76d/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |

Complete v4 component identities are represented below by hash groups; a row named in a group has exactly that component identity. This is the complete comparison across all 34 requests without repeating shared hashes in every row.

| Component | Identity groups (hash: rows) |
| --- | --- |
| `action_scope` | `62d84efa0174c4e2f766ccefc5ea77f995b954301dccc8530bf0fa10278342ab`: all 34 rows |
| `allowed_mechanic_families` | `77bdad9bb566166c6acc9705613f8a72bcb0326fdf27e59b3d707511143752b2`: all 34 rows |
| `case_without_id_or_fragment_shadow_v1` | `7638c62adadd446d6105cd0d644642ef4d23cae3839c35f2f0cfd960c6bb99c3`: `primitive-currency-only`<br>`41fa390c77c9381b6da2c4a8c5b72fe61ccdc5048591f78a90abe8a42b44e6d6`: `primitive-addback-essence`<br>`8a472236891eb54467eb613e99183d66ce67db025c6602e359077635c938109e`: `primitive-addback-fossil`<br>`b7bcfab9f084940e0d8f9f34c9b9c1926988e6fff21083a8cb017478a1b6957b`: `primitive-addback-harvest`<br>`d4be98f6c9ce58bc4691a52abf6ad80c2a84bf4a23058f65b2880a34c813d056`: `primitive-addback-bench`<br>`14df44b428808dc76ee54a0ab1e43cb2acb49497eeeba34408359c1aff32424a`: `primitive-addback-eldritch`<br>`16ba2337649f7a2fdc8af7725a7735b2008b2bbce267c89adf87cb909ff71818`: `primitive-addback-influence`<br>`7262bc34e5a2b6d16f890f9410c235ecf986e1ecfaae0e3b73bdb7ae08b5441d`: `primitive-addback-fracture`<br>`c2b5e442fc807cea965f30eebc950cba372e62c3cda7fbf5969b0b77bb268771`: `primitive-addback-veiled`<br>`59fbe72ecbc4c869a19de7b09dfce4487b6b8634d53708e0317fe62c10ef5ed7`: `primitive-addback-cleanup`<br>`c2a277a3cd648a563fbca0e77e168cde1e276bb34dfc347c7955ee5c09670ea7`: `primitive-addback-temporary_bench`<br>`44464cb3fd6d9b6cd079eeea308a26b30a389923543daa4269691a269a396fd9`: `primitive-addback-metamod`<br>`abf647ad212065878c766c1e497b8b7c5235d4b8d92d951c966917990566427b`: `primitive-addback-imprint`<br>`b19d020edab3c7b431a0d858a74c7c98bdded5dd61bbc46ff9d2cb5176a407a6`: `primitive-addback-restart`<br>`fc970837f84326a6f5c546f7098bd8417584d7f8a951676cd497692211c8dfe6`: `generated-singleton-fracture`<br>`c5cab487b19aed85af82c7db0f2396f52af41859ac163f885aefb61874aaad9a`: `generated-singleton-permanent_bench`<br>`611bd753bc9b285d2a128d80a8d358e2c39a1dfbbc0e294b673d1de7e216b4f7`: `generated-singleton-temporary_bench_blocker`<br>`8976cddba505b55af6d57b1b7ea66806e97608e653252a87a75f78202faee38f`: `generated-singleton-protected_metamod`<br>`f6f3b25ad034731b4aba9631d46f1c2dfc8dca7a46e9039ae7e1550d82756996`: `generated-singleton-multimod_finish`<br>`777d59ef0ef4a6f9fa46b276b0f2e9d5af30a06245f148ad64fcd535a9e17fe1`: `generated-singleton-imprint`<br>`21ccd2e7b9906fdf4c04913347972c15f185141b6ff352707cbd49fd7d103e6b`: `generated-singleton-constructive_renewal`<br>`f5bafff50d12d3e6f16a921ce75b7f54641c728533ab917ddf562cd06eeab2bc`: `generated-singleton-eldritch_side`<br>`287579d09efb10a9a97b82323ebecdf4315009d419e4c687bf70af7179b11164`: `generated-singleton-cannot_roll`<br>`22ea8aecb31defb39340b6363d1bdc0d5c870b0ca00e9075f33d912cdb4c4b7e`: `generated-singleton-veiled`<br>`0f4f0c7585963c7d7438ff6a0c856ad2a4b831b2cbb130d289ae11e1209c9691`: `generated-cumulative-02`<br>`0a2e9e373f6f5102dd93169bac832daba7848d03aa4a1426442623ecbc72f819`: `generated-cumulative-03`<br>`45f4eb355fe65d85d69e51f7d2a38d1bde9844383383026c1d416a834f9d6127`: `generated-cumulative-04`<br>`3c15da421cb5095883fff217fad6e4258754cf3a9bf2b572ea56d082fbc3f17d`: `generated-cumulative-05`<br>`6650dc0219ad4459fc9288716abaedbc905a30de74453698c80d192b803da1af`: `generated-cumulative-06`<br>`53563bf4fa0fe4d08b7d10d33b634a76b6a32570d9a20c9c769bb602d9407f96`: `generated-cumulative-07`<br>`a4b3249902f2e757c3c0dcaacd7401cc655d274e70fed00d8fe0483e8808d7bd`: `generated-cumulative-08`<br>`90a1c486bea96c3939ddf33a227dce5aacba6a30729a00e0b936d7fa9c0d05d0`: `generated-cumulative-09`<br>`52364cea27884da2406b7b47b9a1c309e904bb9dc19edbef06b223753c44ddb4`: `generated-cumulative-10`<br>`56c0758e575eaf4c12087e0f9709cb2681bbaad966a7104cfd5fc9f2f794bb27`: `generated-baseline-none` |
| `compiled_artifact` | `9f6ed3c635c46878234669fef25f462da1d1d80ed3ec759f8687075a97fc6f6e`: all 34 rows |
| `corpus` | `7b84a7e139232998b68a214d90965d9bb91c12fc5e8dbe0126168e5a0183245d`: `primitive-currency-only`<br>`586f4ca1c17e50eb7113db16687d3f2907ffb029d4b8f19695d72c49607f0378`: `primitive-addback-essence`<br>`cf66304c571aa7b4f8169379d8e17c6592391aaf1db2ebd9c05afad640f88daa`: `primitive-addback-fossil`<br>`94d9bd861780a1cc6830daadcbf57243942c379e56b9983f1388cb37f44e126f`: `primitive-addback-harvest`<br>`995938eb57c7ace18ccf7273f36b8a8c137005464f6fe10083cbf054e2e68721`: `primitive-addback-bench`<br>`3440d9eb348eba23f4fac80862f74469bac010b727b8071d8aea5025f8b4712b`: `primitive-addback-eldritch`<br>`ec5646302dff5c729b10e42063f0ba9328fdc7d0b82d15975b56b64aa1d3259f`: `primitive-addback-influence`<br>`63d875ec8e0604d4ddc433b0422642ecb4dab422e031e30b3bf2a239ac2f4210`: `primitive-addback-fracture`<br>`407b8e04ce3a5b269dd82e650d3aa70c492615ddbe3102d80b1445d40b80e484`: `primitive-addback-veiled`<br>`ccdc74f74f1a206a87bd3400a6cf03783a1e3d4a2dee32315b66f381986cb60e`: `primitive-addback-cleanup`<br>`289d72ccc4b36f28a74494e7f5987e5ae0442e2b0e0898477e19e340f772bbe2`: `primitive-addback-temporary_bench`<br>`b1d104b929f4e216815d88e995740c85434ef5c1506d810cd07c6890932e859e`: `primitive-addback-metamod`<br>`560b4f47ab0e48a1fcdb37c6698e9cdb709579e18fe9152a00d14aa921c33b3e`: `primitive-addback-imprint`<br>`4e3c1e418bc236187b62aa3c8bd6186fad608b33006ed4b9a0fde7415a60ac33`: `primitive-addback-restart`<br>`42b7676c64cfabb4b914cb7cf0de86f1d4b6d5f38e851b6925de2a42e461263d`: `generated-singleton-fracture`<br>`8a01a9c8bd1cd7bb22230eb85cfe154f4ac9e0c36a0a121f0575cdf46377715e`: `generated-singleton-permanent_bench`<br>`c5f1da23048543c17839c08a152bc2178a199b6596f1c1805b8169ef212dd598`: `generated-singleton-temporary_bench_blocker`<br>`0bf3ea1b92ad8ba2a274a88f85763633d01f0b67e990a0f52ac21f59a2fd285c`: `generated-singleton-protected_metamod`<br>`77375cc8290bcd4bd4ef733b2b202b8e88ea529055304cec078b3fb74a5854ff`: `generated-singleton-multimod_finish`<br>`1564d0ca7c4d614638eb8cf4237f7eaa8199d611c76627026136129d88fa7227`: `generated-singleton-imprint`<br>`68cc0e43c6ceaf28ec74cd904f41081b887efb1cc764d8ab5b56b287ff81d4de`: `generated-singleton-constructive_renewal`<br>`a7fe2e0d896762fbf0f07be70d4a19ed8ba89a9c967e4549dcc40fe85489b471`: `generated-singleton-eldritch_side`<br>`ed921c119c860d14d531248e0ed1b88bcbda7ff605e3a66cfe5b217d320e8e4a`: `generated-singleton-cannot_roll`<br>`7033dfb48a5667f3ae7aed9e6ce13c5a1ed37acee3cc6956ef41055d89ef219e`: `generated-singleton-veiled`<br>`18758fc5ddc8049fe1844ac2a7f31a53440804a440fa2a80a4abf6d963c63b85`: `generated-cumulative-02`<br>`6802f90d925401033d2bb93f241a5caad2454a1803d1b6062b11f8564b26aaec`: `generated-cumulative-03`<br>`ce99c00ad0eebc6bd6f2759d35ea22de5f9779ff88b2e43ba3d629a40001acdb`: `generated-cumulative-04`<br>`b4af3d3b348721c94f86a3af3a056e2f06418f19b16523d07eb73f2d2e781bd6`: `generated-cumulative-05`<br>`bd6e9106dd9e88572784a521e1f7d418f377caddebed8881b82f1415af691a08`: `generated-cumulative-06`<br>`9fa67b3dbf1c3c22866d2a43b6e831b4e8271f0c55d59e87653eb88ab04e0337`: `generated-cumulative-07`<br>`4581a01169f270c36a1ec816cd177f985399388513a22863ce2243b641f4d770`: `generated-cumulative-08`<br>`e349a98beb32c21868804386e0970dca5b344797a6d156c0f4efb5ab23a081ee`: `generated-cumulative-09`<br>`9db2b0214c4c1dc4484f8c611b6034a682388628916ce5696027738b257d167e`: `generated-cumulative-10`<br>`b3ce60d9630f84ba0fb9ce7fa412b357c17d874fd2652d8291a6e0c1ea199729`: `generated-baseline-none` |
| `economy` | `dfdfada9e1aa5b68ed6267899b43c08ca20b7d65f4bb58234f4f0f439d0b805d`: all 34 rows |
| `effective_disabled_action_families` | `a344bdb69d37f8412005035885bef7c48a5c65fd506000c40cfa34ef5e00f39c`: `primitive-currency-only`<br>`379cc039b8d261615c89fe8a1a19310ff60ad550deb30ff3e4ddcb466d0a4413`: `primitive-addback-essence`<br>`f8e21bbeab293ad120e91093959f9a1eda1d0b09cc9bf099ffc0537983ed87e8`: `primitive-addback-fossil`<br>`4204df3636d92944c80fde59f57393daedc0066f9b0a84cb521750c1cdced4c2`: `primitive-addback-harvest`<br>`cd1f6fbe069d341d39387f8251d80d53ee46568a93375cdef7584e9d3c8bfc0e`: `primitive-addback-bench`<br>`19c071b8a94eea07e90e1c46703e1f066933b7dc57bed010df5261534f31d33f`: `primitive-addback-eldritch`<br>`28fff412a3f5ef063db37ddf064aa667111d2c94ed8140745b341c3e5bd9303a`: `primitive-addback-influence`<br>`a0615ef96ec382e52f5c8d7a5b699fc5a4a037e7821021614b9850f6724ee4a8`: `primitive-addback-fracture`<br>`377ce5589278f534fb2c0cb7db0ce4bb136c6c7ecc30cc7050d47bd1c06af4d6`: `primitive-addback-veiled`<br>`7ea245c91b5f0f2ef73973232255e6fa065f66529306df47dc8679615b23e9b0`: `primitive-addback-cleanup`<br>`d7f13e3455eb21d2bda373e64f418df0930a8c9e30dfb2a28f2704a99810bcef`: `primitive-addback-temporary_bench`<br>`ecb802b6b022d71d942d6c5146e90ff06f503a62e3bb7bca33440bcb9885c97f`: `primitive-addback-metamod`<br>`8bb09d6322ce9bbdc8c18d7892fbddcc8aa9cb9cca79942888451b7494484b33`: `primitive-addback-imprint`<br>`42a55217f2f41491200460f96744d0316eb9e673a88bfdb25319d2330ce943d2`: `primitive-addback-restart`<br>`4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945`: `generated-singleton-fracture`, `generated-singleton-permanent_bench`, `generated-singleton-temporary_bench_blocker`, `generated-singleton-protected_metamod`, `generated-singleton-multimod_finish`, `generated-singleton-imprint`, `generated-singleton-constructive_renewal`, `generated-singleton-eldritch_side`, `generated-singleton-cannot_roll`, `generated-singleton-veiled`, `generated-cumulative-02`, `generated-cumulative-03`, `generated-cumulative-04`, `generated-cumulative-05`, `generated-cumulative-06`, `generated-cumulative-07`, `generated-cumulative-08`, `generated-cumulative-09`, `generated-cumulative-10`, `generated-baseline-none` |
| `executable` | `c931a903609340afe9f17617940447eac70ff5a71cdd798cdde6dae798d137e8`: all 34 rows |
| `explicit_imprint_scope` | `5a83bca202e341f443101647f76b6f65565aa0d1f7ace4e0bd50ba4f77205068`: all 34 rows |
| `goal` | `d9618b7383d1a10461fe80014112858cce3e45f4692a1160a620eb0f68ec664d`: `primitive-currency-only`<br>`66349087e86bda6b0ebc68ca8ff2bd10a546606651661d32933ba1c451270014`: `primitive-addback-essence`<br>`e65bc7269cee08ac1f6838f4fceeea86af90f89fca105e70e2911ec1d849ee29`: `primitive-addback-fossil`<br>`4fed390e2a0e72cea91c8662bc648194c61722910692f353bef69e705ae0a5f6`: `primitive-addback-harvest`<br>`d6a4e38ba4d0f54e4cd9d20b1b98c74f28e621ea249a87f0405da559a9758c00`: `primitive-addback-bench`<br>`77f16d289c82f6b6273b09eb0cec670d9f2378a4bb359c6ac8081ebd55fdfe48`: `primitive-addback-eldritch`<br>`08234003e1c4ce88a340250002b7a46bce825c1186a1c24b375fedeebe712307`: `primitive-addback-influence`<br>`f32e9c454102f5129f95ad48ef88a256542f808090ce2e568ba60a6f618fa933`: `primitive-addback-fracture`<br>`6af0c9e39502222969e18dec9d8fa550179f00ac352f27aaf9470815d718ff14`: `primitive-addback-veiled`<br>`4e9cc17459770a147fc4716bbdc791705a8fd124353ff2df524d2131038c3971`: `primitive-addback-cleanup`<br>`81695ea7ae9df6dd11e0085feeb19ad68027b13cc1e129cc183b161d1015a70b`: `primitive-addback-temporary_bench`<br>`ffd0f7dc77095436834d8a44e11c56fe44581e1af61c9a57849f7899763e486f`: `primitive-addback-metamod`<br>`e1bf76a2609d1d57c5d3da0f6bbde0e178c8a135c77df01a8ae10770104fe775`: `primitive-addback-imprint`<br>`22a69e13f60895ad5d72e5b547764c999f5b59c96dd7a026d9f2b7388a0d2bad`: `primitive-addback-restart`<br>`591140847afea0f0bc98612555c3c2da20180441c64ecb2b153ab81f578f7b74`: `generated-singleton-fracture`, `generated-singleton-permanent_bench`, `generated-singleton-temporary_bench_blocker`, `generated-singleton-protected_metamod`, `generated-singleton-multimod_finish`, `generated-singleton-imprint`, `generated-singleton-constructive_renewal`, `generated-singleton-eldritch_side`, `generated-singleton-cannot_roll`, `generated-singleton-veiled`, `generated-cumulative-02`, `generated-cumulative-03`, `generated-cumulative-04`, `generated-cumulative-05`, `generated-cumulative-06`, `generated-cumulative-07`, `generated-cumulative-08`, `generated-cumulative-09`, `generated-cumulative-10`, `generated-baseline-none` |
| `goal_action_list` | `74234e98afe7498fb5daf1f36ac2d78acc339464f950703b8c019892f982b90b`: all 34 rows |
| `measurement` | `ef9e35d9979f9a789c1177f6bce73f36e47732321aece2d5cf1fdd30d6e8cceb`: all 34 rows |
| `product_action_envelope` | `fdda35cb0c46ca0593efd61be61004cdd8a97e6fe3c808137c6b0c2b8cf2da4a`: `primitive-currency-only`<br>`ed75e459db038ec53f697b731286730fcb244d81af7601955b22f3b30de22b36`: `primitive-addback-essence`<br>`93a38d809828981e648622b0271b49ccf76d6759755250dd9fcb0d8b4788c91d`: `primitive-addback-fossil`<br>`343bcdaae0ce311dd991aaa0d0daec925fbd4e9ec1fa9257297478d43764a03a`: `primitive-addback-harvest`<br>`bacdd5a2ea986f2709e054dad51bea00da682544f257c83f65ee969acd666e61`: `primitive-addback-bench`<br>`20403fbce3349fcac32b150e17da3bfc36b034bbf4235c2835a821ddb80a9a69`: `primitive-addback-eldritch`<br>`be86dbf71897e675e400f17cf8b11452804f4cd4e74bf1db280f51dec51582aa`: `primitive-addback-influence`<br>`341fc9da02e735c6c71e59a0578b44af12824e2490ae2c824b2d067ca654b137`: `primitive-addback-fracture`<br>`a06f4560956d84b9aa89b36a43e535ec6105bcfd9d1e2c1745147ba417e929ac`: `primitive-addback-veiled`<br>`5e4f0086a4cc0f55dbfb5619b14e717611633c42fa659877bbc3cb9ca4c3b229`: `primitive-addback-cleanup`<br>`185169e2f96ed2c2bdd1300c820acb28fc161e5fb6ad05297cadc08c0e14704b`: `primitive-addback-temporary_bench`<br>`10343e178059e87f16455f11f130f671a4f4a714387279c4f33a3a06e05086db`: `primitive-addback-metamod`<br>`ad3e77c3994fcaf38ff1e39e210262e5650d274789f07da83620ae81015c9ccd`: `primitive-addback-imprint`<br>`d292d352467b834463a5264f37280bd7df84a344749ddcfb9d1fdb99aa6a4d71`: `primitive-addback-restart`<br>`83a9b351d18135bf32050b397fe899383ba7530482480e685a2b4629c9d5041a`: `generated-singleton-fracture`, `generated-singleton-permanent_bench`, `generated-singleton-temporary_bench_blocker`, `generated-singleton-protected_metamod`, `generated-singleton-multimod_finish`, `generated-singleton-imprint`, `generated-singleton-constructive_renewal`, `generated-singleton-eldritch_side`, `generated-singleton-cannot_roll`, `generated-singleton-veiled`, `generated-cumulative-02`, `generated-cumulative-03`, `generated-cumulative-04`, `generated-cumulative-05`, `generated-cumulative-06`, `generated-cumulative-07`, `generated-cumulative-08`, `generated-cumulative-09`, `generated-cumulative-10`, `generated-baseline-none` |
| `profile` | `ca458d9024063ce97ec07299c9f818c3ecea72a9a943e7d2bddd8c302353fbd8`: all 34 rows |
| `requested_bounded_finish` | `4a44dc15364204a80fe80e9039455cc1608281820fe2b24f1e5233ade6af1dd5`: all 34 rows |
| `scheduler` | `9b289fc81f951b0728cf752075979c53ca9aba5795cf2bb793e141e1e8e902ec`: all 34 rows |
| `solver_caps` | `8de7637db691006f3ac1c7aa4fe45a9bb49b611d514909bfe3fad0e9cf2937c4`: all 34 rows |
| `source` | `a14d25674eae09148aa14708db2c466bfd8d6a12e9c3c774ee259e7202d97544`: all 34 rows |
| `start` | `b5c04cdaa39d5722815733613ca9a2a2bcb122721575a1e313b13512e6c43cc2`: all 34 rows |
| `watchdog_seconds` | `db58b6c40698d7371bbcff35d085e1bac5fa439d0de31eb8e0da7c47d27cb2a7`: all 34 rows |

The zero-generated/full-primitive baseline is decisive: it failed before any generated-kind distinction was observable. Source inspection then located the benchmark-private scope bug in `priced_product_action_ids`: product-envelope action derivation constructed a normal envelope solver before applying the diagnostic automatic-kind mask to the actual case solver. Thus the supposed generated suppression did not cover the product-envelope registry used to derive `goal.actions`. The same pre-solve region had no partial checkpoint, and the MCP run summary omitted already-bounded operator-lineage/ledger/ladder fields even for completed runs.

The narrow in-scope correction now applies the same benchmark-private automatic-kind mask to both the product-envelope solver and the actual case solver, publishes setup/planner-construction checkpoints before synchronous registry work, and exposes the bounded native `action_control`, `automatic_candidates`, `incremental_action_envelope`, typed ledger, operator lineage, cooperative scheduler, carrier ladder, and missing-frontier objects through MCP run summaries. No public ABI, product request, mechanic, default solver envelope, admission, scheduling, proof, or publication behavior changes. Focused verification passed: benchmark build, 13-specification benchmark validation, and the new Solver Lab summary regression (1 passed). A fresh configured server and retry matrix are required before Gate 3 attribution can qualify Gate 4.
