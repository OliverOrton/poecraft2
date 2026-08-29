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

## Gate 1 — In Progress

- First owner: replace the falsely named `disabled_families` disclosure with
  truthful canonical action-envelope components and regressions.
- Second owner: reproduce and repair the currency-only low-cap access violation
  without changing the requested envelope.
