# Persistent multi-preview Hinekora programme

## Targeted currency UI/error follow-up (source checkpoint)

Oliver selected normal-currency integration and reported an engine error using
Lock. The isolated `dot/lock-currency-ui-20261003` worktree starts from published,
remote-verified `72448deaacca2797058de0559378685ba71a983f`. The engine information
state API remains separate from ordinary solver action admission; its UI now
uses the normal Basic currency grid and action handler, with active preview and
commit status beside currency controls. Applying another Lock is disabled while
one is active. The old standalone application panel is removed. Existing busy
and pending-Unveil disabling now covers the normal Lock action control.

Unchanged published real-worker contracts pass on WASM `0b800f57...`. Targeted
baseline reproduction records exact requests, full input snapshots, phases and
error codes in `out/lock-currency-ui/baseline-errors.json`. Modern independent
application and supported cross-request observations pass. A confirmed legacy
compatibility bug returns **engine error 4**, `Unsupported or inapplicable Lock
observation; no currency was consumed`, when the UI observes the *same* Exalt
request of a saved `fixed-currency-lock-v2` Lock. The shared UI always calls
Observe, while native Observe previously accepted only independent Locks.
The source repair returns the original cached legacy outcome for the same
normalized request; different legacy requests still refuse. This draws/spends
nothing and changes no coupling model. New native and worker regression fixtures
cover that witness. Native inapplicable/unsupported previews retain their refusal
and show a specific unchanged-Lock status in the UI; unexpected errors retain
their original code/detail. The user's particular error is still unclassified
pending the exact message and phase; this reproduced bug is not asserted to be
their error. The corrupt-input scratch case used an unsupported edit field and
is excluded from conclusions pending corrected fixture preparation.

Source diff checks pass. Changed-source native/shared/WASM, worker/web/TypeScript
and rendered normal-currency flows are **unrun**. Both baseline worker processes
have ended; LOCAL is released for the waiting Calculator owner while source is
prepared. Obtain a fresh parent grant before qualification. No user stash/history,
main files, data/prices, existing server, push or deployment has been changed.
The prior qualified receipts below remain historical and do not qualify this delta.

Current qualification: two-job native Engine/shared/header and matching WASM
builds pass; all **64** focused fixed/independent Lock Python tests, the real
worker/Emulator contracts, all nine `test:lock` files and TypeScript pass.
Every owned heavy process ended at 07:40:31 UTC and the serial slot is released.
This supersedes the historical pending-slot checkpoints below. Integration and
publication remain with the integrator; the independent model supplies no
adaptive exact solver authority.

Selected by Oliver, resumed at 04:00 UTC on October 3. His 04:12 reply
`1.yes\n2.yes` approves the proposed initial models; the first yes approves
independent cached Lock requests, explicitly approximate/simulation-only.
No remaining coupling-selection gate applies. This supplies no exact adaptive
solver authority. The parent owns resource coordination and integration.

Baseline and launch HEAD: `3ab862fece9d52ca733ad01a0869fd06cec84810`,
verified against live GitHub main via the connected read-only commit action.
Direct git network verification failed through the configured local proxy;
local HEAD, main and origin/main matched. Planning baseline `dbc142a2` is
superseded. The prepared `dot/lock-multi-preview-20261003` source was copied
without changing its worktree into fresh execution branch
`dot/lock-multi-preview-exec-20261003` in
`C:/Users/Oliver/Documents/poecraft2-lock-multi-preview-exec`. Applicable rules:
root AGENTS; no nested AGENTS or local `.agents`/`.codex` skills were found.
Work is sequential without subagents. Protected root `0` is untouched.

## Interface and engineering decisions

`pc_hinekora_lock_apply` applies one paid Lock before selecting any request.
`pc_hinekora_lock_observe` resolves complete native parameters, reveals one
applicable cached outcome and selects it without changing the input or spending
currency. Ordinary application of a supported request commits its reservation;
selected commit consumes the selected result. A successful currency use ends
all foresight even when visible state is equal; refused actions preserve it.
Raw item modifications and eligible deterministic changes retain existing
invalidation/no-refresh semantics. Copies cannot acquire original native identity.

The versioned simulation model is `independent-cached-lock-v1`. Native mechanics
remain the owner of each request's marginal. Application reserves the complete
finite normalized allowlist domain in deterministic order: supported ordinary
currencies, all runtime essences, distinct Influence Exalt codes and Eldritch
tiers 1–4. Each reservation uses fresh native outcome sampling from the next
private random stream positions; failed/refused requests publish no outcome
and do not advance that stream. Request aliases share native parameters.

Checkpoints store full reserved outcomes, including not-yet-viewed requests,
plus selected request, input/current identity, session/runtime/configuration
identity and no-refresh status. This implementation choice preserves unseen
request consistency across Undo/reload without serializing RNG seeds/state.
Restoration validates the whole request domain before retiring any live handle.
Changing observation order cannot refresh a request. These are replay records,
not probability or reachability certificates. Eager reservation is a bounded
engineering choice within the approved approximate model, not a game joint law.
The former fixed-request APIs and `fixed-currency-lock-v2` checkpoints remain
supported for compatibility and retain their prior constrained behavior.

Emulator exposes Apply Lock, then free Preview currency controls, then Apply
foreseen currency. The approximation is disclosed visibly. Payment is recorded
only on application and actual currency use; each preview creates a zero-cost
history event. Shared transport changes are minimal Lock operations and fields;
Calculator goals and donor/persistence schemas are untouched.

Current/Finder, ordinary Calculator, authored Simulator, pending Unveil and
donor-dependent preview admission remain held. No Divine/roll optimization,
new currency law, data/economy refresh, solver benchmark, dev-server restart,
independent push/merge or deployment is introduced.

## Validation and coordination

Source/interface review and `git diff --check` completed. Focused new Python
contracts cover switching/aliases, item identity, seen/unseen checkpoint replay,
atomic refusal/import, decline and equal-visible consumption. Existing fixed
Lock tests remain. Real worker coverage adds selection-free application,
normalized requests, switching, atomic refusal, replay from pre-observation
checkpoints and spending; Emulator controls cover separate payment/preview/commit.

Builds, test execution and matching WASM are **unrun pending parent's serialized
heavy-slot grant**. Recombination owns the slot at launch; the parent was asked
before any build/test/WASM invocation. Compiler maximum: two jobs. Expected
qualification is the focused Lock Python suite on the newly built shared DLL,
Lock worker/UI tests on matching WASM and TypeScript. No broad suite, long solve
or Simulator trial is required by unchanged solver/strategy vocabulary.
No heavy process or handle has been started by this programme.

## Fresh execution source review

Live GitHub main was rechecked as full `3ab862fece9d52ca733ad01a0869fd06cec84810`.
Prepared source remains unchanged in its original worktree. This execution
corrects model-label preservation during cloning, catches observation exceptions
at the C boundary, and atomically rejects unselected imports containing selected
request/outcome fields. Focused fixtures now cover distinct Essence, Influence
and Eldritch settings, failed application without RNG consumption, and free
observation/restoration without RNG consumption. The malformed Harvest refusal
fixture was corrected to a complete request during source review, before tests.

`git diff --check` passes. [Source receipt](qualification.json) records exact
changed implementation hashes and frozen runtime identity, with every executable
check explicitly unrun. No produced DLL/WASM identity or passing test count exists
yet for this delta. Parent clearance remains required by the launch instruction;
the asynchronous slot request is pending. No compiler, test process or solver
handle exists. Remaining work after clearance: native Engine/shared/header at
two jobs, focused Python Lock tests against that DLL, matching two-job WASM,
the existing nine-file web `test:lock` chain and TypeScript. Heavy work will be
serial and the slot released immediately after those checks complete.

## Alignment and prepared qualification (awaiting slot)

Parent selected qualified main `0187f3d8334c3b9deb9730a6fa89899c051e9931`.
The isolated branch merges that main without rewriting `e792c041`; the only
conflict was the two prepended HANDOFF entries, both retained. The native/WASM
inputs from main are unchanged by this web scope-adapter patch. Applicable
AGENTS and local skill availability are unchanged. Multi-goal now owns the
serial qualification slot; Lock is next and awaits an explicit parent grant.

Prepared independent worker fixtures now serialize application, observation and
consumption history, Undo before any request, observe Chaos/Exalt repeatedly in
changed order, and Redo the consumed state with separate Lock/currency counts.
Incomplete imports must preserve the current live Lock byte-for-byte. Existing
native fixtures cover complete configured requests, equal-visible consumption,
refused/no-op actions, decline, exact item identity and no RNG use on restoration
or observation. Source review also renamed a pytest parameter that conflicted
with its reserved `request` fixture. No test has executed at this checkpoint.

After the explicit grant, run these existing owners sequentially from this
worktree. Use the already installed web dependencies; do not install, refresh
data/prices, restart the dev server or run broad/long solver work. Capture output
under `out/lock-multi-preview/` and retain any failed invocation. Check exit
status after each command before continuing dependent work.

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = "2"
powershell -NoProfile -File scripts/dev-engine.ps1 -Task Engine -Jobs 2
. scripts/engine-build-common.ps1
$lockCMake = Find-PoeCraftCMake
& $lockCMake --build build/engine --target poecraft_engine_shared poecraft_header_smoke --parallel 2
& ./build/engine/poecraft_header_smoke.exe
$env:PYTHONPATH = "tools/ingest;bindings/python"
$env:POECRAFT_ENGINE_LIBRARY = (Resolve-Path build/engine/poecraft_engine.dll).Path
py -3 -m pytest bindings/python/tests/test_hinekora_lock.py bindings/python/tests/test_hinekora_multi_preview.py -q -p no:cacheprovider --basetemp out/lock-multi-preview/pytest-tmp
$env:EMCC_CORES = "2"
powershell -NoProfile -File scripts/build-wasm.ps1
Push-Location apps/web
node ../../scripts/build-data-bundle.mjs
npm run test:lock
npx tsc --noEmit
Pop-Location
```

Record source/engine tree, all changed native/facade hashes, the tested DLL and
matching WASM/MJS SHA256, frozen runtime identity and test outcomes in the same
receipt. Generated bundle metadata is setup from the frozen selection, not a
data refresh. Release the heavy slot promptly after executable checks complete;
documentation and local source/artifact commits need no continued heavy slot.

## Qualification authorized; serial slot still pending

The parent supplied Oliver's explicit approval to finish build/test qualification
and fix failures within Lock, multi-goal Calculator, recombination and feeders.
The exact approval is retained in `qualification_approval` in the same receipt.
This resolves qualification permission; it does not grant the current heavy slot.
Priority solver cap repair owns that slot and Lock is queued next. No build,
test or WASM command may start until the parent's explicit slot grant arrives.
Preserve aligned source checkpoint `69b60e81781dffc3d16e1ce1bcf08e491f738a1a`.
Observed main `0fecbc404925548dc978ecdcf4d2aa0e62c91e1a` adds only solver-test
diagnostics and documentation since `0187f3d8`; it changes no Lock/native/WASM
implementation input. No heavy command, process or handle has started here.

## Passing native qualification checkpoint

Native Engine/shared/header builds pass at two jobs from source `3a7b80b9`,
engine tree `38f7455bd83fd912beeae2ad607599bcd5c4f308`. DLL SHA256:
`195e7d3da273d861f9e7c424ff0171c3bfa8be2afe4bd5cee6d2075905c72a35`.
All 64 focused Python Lock tests pass in 17.01 seconds, including legacy primitive
marginal comparisons and the new lifecycle/request/persistence contracts. The
first invocation stopped during collection: the new test used an absolute
instead of package-relative helper import. That one-line test-only repair is
retained; native/facade source and DLL are unchanged. The failed log and both
invocation results remain under `out/lock-multi-preview/` and in the same receipt.
Matching WASM and product qualification remain pending at this native stage.

## Qualified isolated product checkpoint; heavy slot released

Matching two-job WASM build passes in 297.72 seconds. WASM SHA256:
`ce4a8bc6c744fd16a0c792cee8728a4f25be95896037e8e4fac8678c85e1995b`.
MJS SHA256 is unchanged:
`c3f63141397c06cf966487c0ad5787a338000401dc46f707bba522c959296e9c`.
The frozen bundle owner validates runtime/ABI without refreshing data or prices.
The complete nine-file `npm run test:lock` chain and TypeScript pass on that module.
Real-worker cases cover selection-free application, normalized aliases and repeated
cross-request observations, failed import atomicity, unseen reservation replay,
serialized Undo/Redo, invalidation and separate Lock/currency spending. Emulator
controls show the model disclosure, read-only full numerical preview and exact
selected request; absent prices stay visibly unpriced. Legacy refusal, no-refresh,
context cleanup and strand/editor checks remain passing.

No production native/facade correction was needed after the source checkpoint.
The only execution repair is the one-line test helper import. Existing WASM enum
and range-comparison warnings remain warnings; the build succeeds. The initial
Python collection failure stays recorded separately from the 64 passing tests.
All source/runtime/artifact identities, commands and logs are in the same receipt.
The full repository suite, rendered UI review, long solver benchmarks, authored
Simulator and combined-feature integration qualification remain unrun. Those are
not claimed by this isolated acceptance. No push, main integration, deployment,
server restart or data/economy refresh occurred. All heavy handles have completed
and the slot was released immediately; only local artifact/receipt commits follow.
