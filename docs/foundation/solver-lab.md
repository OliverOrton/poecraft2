# Native Solver Lab

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

The Lab is the existing local native experiment workbench. It wraps the benchmark and owns orchestration, identities, resource supervision, and evidence integrity. It does not own mechanics, policy search, proof values, or strategy evaluation.

This is a task-directed operating reference. Do not launch qualification runs simply because their commands are documented here.

## Purpose And Authority

The structured CLI, optional PySide6 GUI, persistent SQLite catalogue, and supervisor use the same typed service. Each solve runs in a separate native benchmark process through the corpus-worker adapter. No repository-specific MCP adapter is required; that transport was removed while the existing service/CLI/evidence contracts were retained.

The Lab catalogue is separate from canonical game SQLite. Neither it nor an immutable attempt artifact should be edited directly to repair or retry a job. Use the service/CLI lifecycle.

## Install And Launch

Use the repository's normal native build when the benchmark is missing or stale. Install optional GUI dependencies only when that surface is needed. The documented local module invocation avoids dependence on the console-script directory being on PATH:

```powershell
$env:PYTHONPATH = "tools/ingest;bindings/python"
py -3 -m poecraft_ingest.solver_lab --root . --help
```

An installed console entry is `poecraft-solver-lab`. Optional GUI setup uses the existing `tools/ingest[solver-lab]` extra, and the GUI command is `... solver_lab --root . gui`.

Default ignored work locations are `build/solver-lab/catalog.sqlite3`, `attempts/`, `cases/`, `matrices/`, and `bundles/` under that root. They are working evidence, not new canonical mechanic data.

## Locked v0 Profile

`native_allflame_no_imprint_v1` records the pinned Allflame economy, native `calculator_product_v1`, generated Imprints off, voluntary/economic Restart off, native paid Fracture-miss recovery retained, goal-progress gating, junk-free success, and independent native strategy evaluation.

Profile and resolved request identity are authoritative for a particular run. A diagnostic override creates a different visible request; it does not silently redefine every frozen case. Simulator is opt-in and follows the current owner-approved policy in `AGENTS.md` when genuinely required.

## Case Authoring And Revision Identity

Frozen cases are read-only. A local draft can be edited, natively validated with the benchmark's genuine `--validate-only`, and saved as an immutable content-addressed revision. Queued jobs bind the revision and canonical request, not the subsequent editable draft. Unchanged saves reuse the revision; changed content creates a new one.

`derive-case` clones a frozen/revision source and applies registered bounded JSON-Pointer replacements. Unknown, overlapping, duplicate, or oversized edits are refused. Goal edits preserve the native envelope controls; deriving a case is not permission to rebuild a broader action catalogue.

Calculator's Copy Lab case bridge exports its concrete start, goal, and pinned economy. Unsupported flags/checkpoint state are refused rather than dropped. The Lab does not become another graphical modifier editor or mechanic validator.

Execution identity separates actual disabled native families from the profile's explicit Imprint setting, and binds allowed families, product envelope, goal action list, artifact, prices, executable, and other declared dispatch controls. Those components must not be conflated.

## JSON CLI

Existing focused commands include:

```powershell
py -3 -m poecraft_ingest.solver_lab --root . profiles
py -3 -m poecraft_ingest.solver_lab --root . cases
py -3 -m poecraft_ingest.solver_lab --root . attempts
py -3 -m poecraft_ingest.solver_lab --root . run --revision-id REVISION_ID --wait --summary-fields status,phase,lower,upper,states,rows,memory
py -3 -m poecraft_ingest.solver_lab --root . strategy-summary --attempt-id ATTEMPT_ID
py -3 -m poecraft_ingest.solver_lab --root . export-bundle --attempt-id ATTEMPT_ID --idempotency-key EXPORT_KEY
```

Place common root/catalogue/attempt/executable/artifact/corpus/profile overrides before the operation. Inspect operation help when needed rather than guessing flags. These are the existing JSON operations, not a newly implemented human-table/`--json` interface.

Low-level mutations use canonical-request idempotency and accept the documented dry-run path. `run` derives its key from the complete resolved request when one is not supplied. With `--wait`, changed status goes to stderr and one compact structured result goes to stdout. It either owns a supervisor restricted to the submitted job or observes the legitimate existing dispatcher.

Use targeted job/summary/compare/bundle reads. Full raw reports stay in immutable artifacts; do not dump the entire historical catalogue into a model's context.

## Matrices

`solver_lab_matrix_v1` definitions use one frozen/revision base, ordered pointer axes, replicates, and priority. The current bounds are eight axes, 20 values per axis, 100 coordinates, and 1,000 jobs. Every coordinate is natively validated. One immutable resolved manifest is written before submissions and binds definition, source, executable, artifact, profile/economy, patches, revision, request, job, and replicate identities.

Replaying an unchanged resolved request is idempotent. A changed definition or execution input creates a new visible identity. Legacy matrix includes union explicit IDs and roles, exclusions apply last, and an empty programmatic include selects the full frozen corpus; it is not an empty experiment.

Existing response limits include bounded attempt, comparison, and trajectory samples. The source contract records 1,000 listed attempts, 20 compared attempts, and 256 returned bound samples. Truncated reads do not establish a complete historical population.

## CLI Automation And Supervision

`supervise` runs a durable dispatcher; `run-until-idle` handles a batch and exits when it drains. Other clients can submit, inspect, compare, cancel, and export without a GUI.

Only one durable dispatcher owns a catalogue. Another verified-live dispatcher remains the owner; a second client does not multiply the worker limit. Replacement requires proving the old process identity absent and reconciling its attempts before new dispatch. Target-filtered one-shot dispatch keeps the same ownership, preflight, watchdog, and lease rules.

Retrying a terminal immutable job revalidates dispatch identity. A changed executable or artifact can yield `dispatch_refused` without starting a worker. Use a new revision/job when a new treatment is intended; do not mutate the old one to make it run.

A private observational control can have unequal full request identities while requiring equal declared core-solve components and ordinary-result components. That special comparison must be explicit; it is not a general exemption from experiment identity matching.

## Statuses, Resources, And Recovery

| Status | Meaning |
|---|---|
| `queued` / `blocked` | Awaiting dispatch or host admission; no solver proof result implied |
| `running` / `canceling` | Live owned process and reservation |
| `finalizing` | Evidence validation/hashing and terminal transaction are pending |
| `dispatch_refused` | Submitted identity no longer matches; no new worker |
| `orphan_quarantined` | Worker is live or absence cannot be proved; reservation remains |
| `completed` | Final worker report, possibly exact, bounded, or native-capped |
| `partial` | Valid partial observation under its retained failure/watchdog history |
| `canceled` | Process-tree cancellation and terminal accounting completed |
| `failed` | Explicit runner/process failure |

Native `max_solver_owned_bytes`, host headroom, worker reservation, and global safety reserve remain separate. The source default host headroom is 512 MiB; only the native cap constrains solver proof ownership. Queue pause prevents new dispatch, not live numerical work.

Recovery checks PID creation identity and evidence integrity. A proved-absent worker with a valid final report can be recovered as completed. Possible-live workers remain quarantined; their reservation is not released merely to permit a retry. Retry creates a new attempt ordinal and directory.

Closing the GUI or interrupting normal supervision stops new dispatch but is not automatic cancellation. For a shorter shutdown, pause dispatch, cancel the selected live work through the service, verify terminal acknowledgment and released reservation, then close. Copy the catalogue and its WAL/SHM siblings only after dispatcher ownership is released.

## GUI Workflow

The optional surfaces are Cases, Queue & Run, Compare, Strategy, and Matrix. They use the same service and preserve stable identities and selection. Cached report aggregation and parsing remain off the Qt UI thread. Activity & Errors retains identity context and tracebacks.

The final canceled state is displayed only after the worker is gone and the reservation is released. The GUI's absence does not disable queue automation. Source activity alone cannot establish whether Oliver still uses the GUI, so this document does not select its deletion.

## Unattended Qualification

The retained qualification harness has accelerated lifecycle checks and a separately declared low-duty soak. They test orchestration, identity, recovery, and cleanup—not solver optimality. They are not ordinary preflight or a recurring requirement for research imports.

The soak contract requires at least 21,600 elapsed seconds. An interrupted or owner-waived soak is not a pass. Earlier owner-waived evidence remains labelled that way; no new soak qualification is implied by this rewrite. Use the harness's current help and existing archive only when that behavior is the actual question.

## Artifacts And Limitations

Attempts retain the available final/partial report, worker/error logs, strategy files, and indexed hashes. The supervisor verifies required files before atomic terminal publication and lease release. Terminal readers check owned path, size, and hash; legacy unindexed records remain disclosed rather than trusted by assumption.

Bundles provide bounded summaries, identities, events, reproduction arguments, and log tails. They do not copy arbitrary local files. Keep new research reports in the existing evidence/intake workflow; do not add a second catalogue.

The Lab does not itself provide live strict-solver checkpoint/resume, running pause, remote workers, automatic cap tuning, or an alternative numerical backend. Nor does it determine whether PDR or another case is currently exact: that is a versioned solver result under the specific native request, not a permanent Lab limitation.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-lab.md) and [2026-08-29-native-solver-lab-cli-first-mcp-removal-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-29-native-solver-lab-cli-first-mcp-removal-v1/README.md), [2026-08-29-native-solver-cli-workflow-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-29-native-solver-cli-workflow-v1/README.md), [AGENTS.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/AGENTS.md). Mathematical links refer to the companion draft chapters and provisional claim IDs; they do not declare those claims accepted. Local implementation correspondence must be reconciled during integration.
