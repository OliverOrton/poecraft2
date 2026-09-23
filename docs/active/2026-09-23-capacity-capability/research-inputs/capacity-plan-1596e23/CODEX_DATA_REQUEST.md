# Data request: identify the next native capacity boundary

This is a bounded local evidence task. It can run alone before any implementation. No new solver runs in the first pass. Do not inspect protected root `0`, alter old receipts, or send whole multi-megabyte reports into the model context.

## Inputs already identified by checked-in receipts

1. `out/ordinary-capability/M3-final-CB06/report.json` — current compact actual Calculator Ring-two report. Expected SHA-256 `11fe39c44809784e52e449b8306ce2ea6c5309678f6e33baf4a522dab1b39267`.
2. `out/ordinary-capability/C3-ring-repair/cases/cb06-cross-base-product8-long240.json` — compatible native production control before the transport-only change. Bind its executable, request, data and source identity from the adjacent runner ledger and checked receipt; do not assume arbitrary later edits are compatible.
3. `out/dirty-state-continuation/control-ring` — historical native-wide Ring-two run. Discover its report and strategy paths from its existing ledger. The checked-in compact authority is `docs/active/2026-09-11-dirty-state-continuation/native-wide-control.json`. It is not a current same-profile baseline.
4. Current C5 early-Finish report: resolve the raw path from `docs/active/2026-09-22-ordinary-capability/evidence/M3-final-CB01.json`. The raw path is `out/ordinary-capability/M3-final-CB01/report.json`, SHA-256 `1c52f66a3e44b4e534b91b5f996975d0e7dfcc1764ff045d22587a0d01e9bfb5`. It is not a full-duration run.

Missing local artifacts get explicit `unavailable` disposition. Read checked compact evidence instead; do not substitute a different same-named run. New outputs below are requested destinations, not already-existing artifacts.

## Return `out/capacity-capability/P0/projection.json` plus a short explanation

Preserve source path/hash, main/executable/WASM identity, full resolved action and price identities, limits, timing origins, transport, stop, U/L, policy identity and omission counts.

For the worker raw report, the actual schema supplies `request`, `resolved`, `solve_summary`, `trace.worker`, `graph`, `graph_sha256`, and `final_telemetry`. Begin with the existing `final_telemetry.memory`, `cache`, `work`, `incremental_action_envelope`, `policy_refinement`, `compilation`, `timings_ns`, `diagnostic_cost` and `states` sections. Extract only fields actually present. The top-level memory section is a selected-allocation estimate, not process heap and not a guaranteed phase breakdown. Final values may follow rollback.

Answer these questions separately:

- What was the first named refusal, its owner/function, candidate/action/kernel identity, phase and committed-work frontier?
- Was it a prospective reservation failure, an actually-held allocation limit, a state/transition limit reported through another owner, or an exhausted cumulative work budget?
- At that instant, what were limit, live accounted bytes, proposed additional/overlap reservation, and scratch releasable on refusal? Which figures are missing?
- Which owner buckets dominate simultaneously: calculator states/maps; distributions/options and private children; sparse rows/payload/index arrays; proof/refinement; compiler/evaluator; artifacts/diagnostics; active coroutine scratch? State the attribution basis and unclassified remainder. Never add overlapping category estimates or independent component maxima.
- How many expanded and discovered states, complete and incomplete rows, transitions, private contexts and pending pairs existed? Do not derive them from one another.
- Does an independently checked fallback exist before refusal? After legitimate rollback, is there a concrete different useful admissible work item that fits? Identify it through native facts, not its historical node ID.
- Is the historical C1620 controller fully available, what are its leading actual operations/resource totals, and where does its construction differ? Keep wide/current scope and compiler semantics separate. Its bytes are diagnostic only, not search input.

Keep arrays bounded, preserve omitted counts, and keep large graphs on disk. Use the existing reporter/ledger/worker interfaces rather than build a new evidence service. A local one-off projection script is acceptable; it must not mutate or canonicalize the original evidence.

## If the first-refusal data was never recorded

Do **one** short native CB06 diagnostic using the existing runner and a bounded failure-only observation at the actual first refusing owner. Record the admission equation before unwind plus post-rollback live ownership. Use existing counters and read-only accounting; do not serialize full telemetry per step. Preserve the unchanged fallback and report diagnostic overhead. No larger capacity or longer deadline is needed to locate the present early stop.

A current unattended C5 compact run is permitted only when no compatible full-duration receipt exists and its result will decide the next branch. Use the existing `default_finish` probe, original 240-second request, and serial supervision. This is not a repeated cohort or a fresh wide Ring campaign.

## Decision supplied with the data

Recommend one of: optional-work deferral; specific storage/lifetime correction; measured fresh-private-layout restriction; or no supported bounded repair. If the witness requires a new semantic representation or cannot account for missing authority, return the witness and exact missing prerequisite rather than guessing an implementation.
