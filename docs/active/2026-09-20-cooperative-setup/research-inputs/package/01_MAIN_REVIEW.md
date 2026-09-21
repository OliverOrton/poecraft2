# 1. Fresh main review and decision

## 1.1 The baseline actually moved

Main is `0f376f4249789e0bbe4680494d8c01609c84a765`, “Enforce retained-policy ownership across suspended verification.” It is two commits beyond the previous review: `75504e4ee2d5700629b23e2ed06eec63812cbd78` repairs validation/provisioning, followed by the retained-pool lifecycle change. [R01–R03](SOURCES.md)

The current programme reports M0–M3 complete on the selected Windows scope. Its earlier `validation.json` is the M1 checkpoint and still says M2 was not started; that historical receipt is not the current overall status. Oliver's September 20 non-Windows waiver remains a waiver. Later hosted Ubuntu knowledge success adds evidence for that workflow, not full non-Windows native qualification. [R02–R04, R21](SOURCES.md)

## 1.2 Preserve what just landed

The validation changes resolve one Python executable, declare pytest, collect both TestCase and top-level pytest tests, prepare data before dependent tests, address the `support` observation by ID, and preserve stored fixture bytes through scoped Git attributes. The immutable upstream route reproduces the twelve frozen RePoE inputs and all three runtime files in the local record. These are not missing features anymore. [R03, R06–R09](SOURCES.md)

The retained vector is private to `IncumbentPortfolio`. Admission, deduplication, four-entry replacement, pruning, const views and complete-bundle transfer use the owner. Verification moves an entry into coroutine-owned storage while a logical identity slot remains in the pool. A detached in-flight entry stays alive and charged; restoration occurs only if its slot survives. The selected mutable pool alias is gone. Output and pending aliases are intentionally outside this completed boundary. Do not claim that all of `SolveWork::Impl` is now encapsulated. [R03, R10](SOURCES.md)

The memory change is explicitly accounted: two active-entry fields add 16 bytes, one alias removes 8, and the fast/full alias compensation changes from four pointers to three. The recorded total charge rises by 16 bytes. This is preferable to hiding real storage to preserve an old number. [R03, R05](SOURCES.md)

Focused retained-selection, bounded-Finish, return-bridge and proof-handoff tests pass in the committed record. The broader native API fixture happened to include its existing 10,000-run test; that is not a reason to add a new simulation campaign now. [R03]

## 1.3 Current policy and delivery evidence

The matched native default-240-second pair preserves the same graph bytes, original expected cost **85558.70618560436**, expected primitive count **8407.202314771383**, independent lower **405.3694021063399**, success probability 1 and off-policy mass 0. The graph has 469 nodes and 1,117 edges. It remains bounded feasible with requested Finish and a separate `refused_unsupported_action` status. A one-row count difference at a wall-clock stop is recorded; not every intermediate counter is identical. This is one serial pair, not a speedup experiment. [R03–R04]

The actual Calculator component, EngineClient, Node worker and freshly rebuilt WASM give:

| Measurement | New record | Interpretation |
|---|---:|---|
| Finish intent → usable UI | 3571.2889 ms | Pass against 10,000 ms |
| Worker request → usable UI | 45185.1174 ms | Pass against 65,000 ms in this run |
| Native begin interval | 12598.9964 ms | Fail against 250 ms |
| Largest native step | 369.0194 ms | Fail against 250 ms |
| Setup cancellation → release | Not rerun | Earlier 23081 ms / 1000 ms remains open |

The outlier still crosses `ladder_scheduling` to `compilation`, cursor 293→295, quantum 8. The new Calculator graph equals the independently evaluated A7 graph under the existing attached-economy relation. Raw comparison to the native graph is false because of display positions; the receipt separately establishes that positions/economy are the only differences. This is not permission to discard arbitrary metadata in future comparisons. [R04]

Historical A7 delivery failure remains historical. The fresh 45.185-second pass does not prove the ownership refactor accelerated WASM; no matched WASM timing experiment isolates that effect. Rendered visual review remains unperformed. [R03–R04]

## 1.4 Fresh hosted CI exposes a different remaining defect

The **knowledge** push run `35543052164` succeeds. The **Windows** push run `35543052184`, job `106164064854`, builds successfully but fails Test at the frozen runtime-manifest check. It now uses the selected Python 3.12.10 and pytest; all twelve source fetch/hash checks, database validation and six spec fixtures complete. Its database data hash is exactly the corpus-pinned `76375e02fc21b0bc0d5709ab589aede8b1967b9a2d53b25aaf517a206f592000`. The compiler then reports 39,292 modifiers and 4,974 bases before the manifest guard refuses. The later Python/native/web suites are not reached. [R06, R09, R21–R22; CI record](evidence/ci_observations.json)

The failed manifest itself and payload hashes are not present in the retrieved log. The run has no downloadable artifacts. Consequently we cannot assert which field differs or that the runtime payloads match, only that earlier identities do. **Do not disable the guard or replace the expected hash.**

The strongest inexpensive hypothesis is the shell boundary: default `ConvertFrom-Json` can create a DateTime from the lock's timestamp, and a `[string[]]` argument binder passes the converted value to `--generated-at-utc`. The compiler stores that argument in manifest JSON. Microsoft documents timestamp conversion and a string-preserving option, but an actual hosted manifest/argv comparison is still needed. [R06, R08, P01](SOURCES.md)

## 1.5 What the source says about the next substantial change

The constructor still performs goal-cover and retention preparation synchronously, before `initialize_owned_bytes_ledger()`. Ordinary non-high-impact cases retain lazy cover preparation to preserve early root-row cap attribution. [R11]

`prepare_goal_cover_cost` still marks readiness before constructing its tables. Lower/phase consumers call preparation themselves. `prepare_native_retention_lower` depends on cover proposals, constructs and validates its full probability model, and catches broad exceptions as optional refusal. These are safe only under their present synchronous ownership assumptions; adding a few yields without separating started/staged/committed states is not a safe conversion. [R12–R13]

The existing coroutine type can be reused, but supports checkpoints rather than direct child-task awaiting. Its destruction is synchronous. The cancellation API also snapshots and serializes telemetry **before** destroying work. The historical 4.8-second cleanup portion must therefore be attributed to snapshot, serialization, calculator rollback and actual destruction separately; its cause is not established by seeing one large destructor or one large table. [R14–R17, R29]

## 1.6 Selected direction

Select a narrow identity repair followed by two-owner cooperative setup and release. This turns the last package's queued work into a concrete execution programme. Keep the ordinary compilation outlier visible and separately attributed; the first programme need not become a rewrite of finalization, evaluator, compiler, scheduler and every diagnostic at once.

The previous Ring observation and response experiments selected neither implementation. No smaller private global layout or reusable native interior was established. Preserve their conditional mathematics and reopening gates rather than repeat the same experiments by momentum. The rough structural audit's counts included uncommitted edits and heuristic measurements; they are not a new complexity census of this pin. [R20, A01–A02](SOURCES.md)

**Confidence:** high on changed source, committed qualifications and hosted stop location; hypothesis-only on timestamp as the hosted cause; no measured speedup prediction for cooperative setup. The source audit is targeted, not exhaustive.
