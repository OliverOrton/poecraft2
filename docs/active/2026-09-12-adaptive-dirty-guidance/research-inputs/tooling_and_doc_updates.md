# Bounded M0A and authored documentation changes

M0 already exists. Current `docs/foundation/tooling.md` and AGENTS at 23e03ba do not yet include the explicit no-idle-model-polling amendment. Add this narrow change, then continue solver work.

## AGENTS — short addition

> Do not routinely poll long-running solver processes through LLM turns. Prefer blocking/event-driven execution or the longest supported empty-input wait, with existing deterministic supervision. Return to the model for completion, an actionable failure or a decision that changes the experiment. Keep native cancellation, watchdog and telemetry polling active. If the client forces short yields, state that limitation and minimize re-entry rather than claiming a shell timeout fixed it.
>
> Batch independently specified runs, related reads and checks when practical. Timed solver cases may run serially inside one deterministic batch. Preflight resolved inputs and comparison identity; reuse compatible completed evidence. Return compact results, exclusions/errors and artifact paths, keeping bulk evidence on disk. Do not introduce another generic execution/comparison layer or a second LLM as a polling proxy.

## Tooling map — append a compact section

### Waiting and batching without idle inference

Decide the independent cases, executable, activation and capacity arms before launch. Use the corpus runner or Lab wait/matrix owner, then an existing compact reporter. Adaptive engineering decisions form a later batch; an arbitrary 30-second interval is not a decision.

The worker's internal process polling checks cancellation and deadlines without model inference. Preserve it. Inspect both the terminal tool and any outer code-mode wait contract. A returned session/cell ID means work may still be running; keep that handle. Do not relaunch after a yield or send dummy stdin as a wait.

Current official configuration documentation lists `background_terminal_max_timeout` in milliseconds for empty `write_stdin` waits (documented default 300000). A supported project override such as 900000 requests a 15-minute maximum; it is not the solve's deadline, an automatic choice of wait duration or a guarantee about another outer tool. A deterministic multi-case batch may need a longer supported finite maximum. Inspect installed behaviour before applying a narrow wait-only configuration change; do not change approvals, sandbox, credentials, model tier or global security policy. A logged completion event is not necessarily an active-task notification.

Prefer one blocking invocation, a supported event notification, or a long empty-input await returning immediately at child completion. If the outer client imposes a shorter clamp, use its best supported route, record the limitation and continue the solver task. Do not replace the client or start another agent service as a prerequisite.

Source checked September 12, 2026: [official configuration reference](https://developers.openai.com/codex/config-reference/) (redirects to ChatGPT Learn). No installed client or billing telemetry was inspected by this remote review.

### Compact output and preflight

Resolve controls and identities before the first expensive run. The prior dirty-state qualification had fourteen completed comparisons excluded by integer `315` versus float `315.0` in its adapter. Use the existing typed resolver to prevent that difference in new requests. Do not change mathematical target semantics or weaken structural identity checks. Preserve raw artifacts/exclusions; any metadata-only recovery requires an explicit derived comparison and proof of identical effective execution.

Routine output: per-case status, actual mode, final and first/best verified U, compatible L/gap, stop owner, candidate/reforge coverage, peak/retained/checker memory, phase times, graph identity, failures/exclusions, paths. Do not dump a full report or catalogue for a summary question. Native/private estimate versus independent graph value must remain visible. Parse raw data programmatically for a named question. A sampled trajectory can miss events; use actual recorded verification times rather than backdating final results.

One short command success/failure/cancellation test and the next required real batch qualify the operating path. No long soak or full solver rerun solely for these rules. Investigate at most three additional concrete wastes encountered here, such as repeated immutable parsing or redundant builds. Independent native evaluation, probability coverage, retention and process cleanup are not waste merely because they are expensive.

## Mathematical/search owner — update for the new baseline

### Private cost improvement versus adaptive guidance

Dirty-state mode already performs sparse fixed-policy evaluation and cost improvement over its completed private action graph. These values are not full-scope native lower bounds. Its coarse estimate can defer a candidate without proving global dominance; its independent emitted-graph check supplies upper authority.

An adaptive completion estimate is an additional way to choose which incomplete action or continuation to construct next. Candidate coverage must exist before ranking can select it. An estimate cannot legalize a protection action in a context the native kernel refuses, nor fill an unknown executable tail. Capped/invalid candidates produce effort or coverage evidence, not labels for optimal crafting cost.

After an estimate update, a fixed in-flight candidate retains its old decisions and observation map. The new ranking applies to future work. Deferred candidates remain revisitable on meaningful changed evidence. Any permanent retirement still needs the original compatible lower/upper proof.

### Entry ownership and spend

A root-only verified dirty graph is not a vector of entry certificates. Its cleared parent decision bindings preserve namespace separation. Query or retain actual private/native entry evidence through the proper owner; never reconstruct those bindings from mismatched parent IDs.

Expected immediate action spend is additive under the evaluated controller. Expected visits multiplied by a tail value is not additive spend. Old occupancy times estimated advantage may prioritize local experiments, but the changed controller can alter those visits. Independently evaluate the whole resulting controller to claim a root cost improvement.

## Other docs

Keep current numerical-closure and representation results as implemented facts; do not repeat their old pending hypotheses. Update resource docs only if actual limits/activation change. Put experiment results in one living record and existing research-series sources. HANDOFF should identify actual completed/pending work and current local/push status; do not rewrite historical 'not pushed' statements in archived receipts.
