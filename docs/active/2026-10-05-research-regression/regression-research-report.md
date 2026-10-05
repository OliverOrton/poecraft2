# Regression continuity and independent worker qualification
## Research packet — 2026-10-05

**Decision:** keep the existing corpus/Lab/shared-worker/reporting owners. Add a small policy-quality tier to the normal changed-layer qualification process, with a separately declared broader release cohort. Spend C$0 on new hardware now. Trial the owned M1 only after macOS build, numerical and process-identity qualification; consider the optional Windows PC only after its owner's agreement. Compare performance within each host.

**Review correction, October 5:** the existing economic gate compares surviving analyzable cases, not the complete independently declared planned cohort. Its current pass must be preceded by an expected-cohort/status prerequisite; symmetric omissions can otherwise pass. Main's process supervision also has unbounded post-termination pipe drain and parent-only survivor evidence, so the proposed 3 × 105s = 5.25-minute candidate reservation is not an enforced current bound. Sections 2.2/2.5 preserve both source witnesses and section 9 adds the required negatives for every admitted host. This corrects the first report version; no production/test code or runtime qualification is changed.

This is a source-grounded planning report. It implements no engine, product, runner or canonical documentation change. It starts no builds, tests, solver, Simulator, dependency installation, machine connection or deployment. All cadences, budgets and experiments below are proposals requiring later selection and execution authorization. No prior run allowance is renewed.

## 1. Identity and evidence boundary

Remote main was verified through the connected GitHub ref API at the beginning of this investigation as **7252027c80856628ed16734583bfc9d6e166458b**. The same full SHA was rechecked before report preparation at 20:37:53 UTC. The normal local checkout is **7eb16ac3d63834fd5d3256ab42483f47d264b764**; main's objects are available there, so source inspection used explicit git-show/git-grep at the full main SHA. Relevant tracked documentation/source status showed no changes; status emitted permission warnings for three pytest caches, which were not inspected. There was no checkout reset.

The isolated solver checkout was observed at **fb59476f54601f81dc18a8bf2457d8eb7d3eb36b**. Its remote branch dot/sol61-solver-causal-20261005 was independently observed at **23dcc5aea8828dc535466613479493bc379d4e7b**. Local later receipts and the unbuilt extension therefore are available local evidence, not presumed published remote evidence. This report copies neither their source nor private-path-bearing raw receipts. The new extension's owners remain untouched.

During the review amendment, remote main was rechecked at 20:50:28 UTC and remained the same full SHA. The exact reporter and worker source witnesses below were read at that SHA. The amendment starts from this report's own published branch head 745633b9da67804d17f65feddb47eab970eb6b74; it transfers no later solver-source or isolated-supervisor qualification to main.

Bootstrap reads: AGENTS.md, research-standards.md, current-status.md, HANDOFF.md, research.md sections 4/5 and solver-research-handoff.md. Changed HANDOFF/status text was reconciled against actual main. The applicable .agents inventory exposed no skill files, and the scoped source tree exposed no nested AGENTS.md for the inspected owners. Additional reads were targeted source/math/manifest/receipt excerpts. Four imported Pro reports were reused through their import index and relevant sections, not treated as four independent corroborations of the same observations. No recursive archive read, protected root path inspection, frozen-data/economy refresh or bulk trace dump occurred.

Classification throughout:

- **Observed source:** what the pinned implementation/manifests/workflows say.
- **Measured receipt:** a preserved native or hosted result under its exact recorded identity.
- **Derived:** algebra or arithmetic under stated premises, not a fresh execution.
- **Hypothesis:** an untested explanation or expected benefit.
- **Proposal:** an implementation/qualification decision for a later approved programme.

Source links in section 12 are pinned to main unless explicitly local-only or historical. A source check, historical artifact, successful functional check and matching policy-economics receipt provide different authorities.

## 2. Findings that change the practical plan

### 2.1 The green release checks do not supply a quality cohort

**Observed source:** Windows runs on push and pull_request. Its build-and-test identity remains intact. The conservative ci_changes classifier skips native validation only for known root Markdown files and docs/*.md; unknown, mixed, executable, unavailable or empty changes require native validation. Solver knowledge runs independently on Ubuntu. This report is a single documentation Markdown addition, so the existing classifier can recognize its layer without a new CI exemption. [S1–S4]

The native release script and CMake corpus targets use **validate-only**. The product living record explicitly says the full pass did not qualify the real nine-case quality cohort. Engine unit tests and real worker/web checks provide substantial functional evidence, but they do not mean routine solve discovery achieved a stated checked cost by a stated budget. [S2, S5, S6]

**Measured hosted observations:** actual main Windows run 37356886718 and knowledge run 37356886722 are completed/success at the full SHA above. Exact-source branch runs 37351930481 and 37351930573 also succeed. These were read as live GitHub metadata, not inferred from HANDOFF's earlier pending snapshot. The main Windows run was created at 18:33:52 and updated at 19:11:15 UTC; that 37m23s interval includes workflow overhead and is not an isolated test CPU measurement. The integration record separately reports a prior matching-engine build of 424s, Test of 1656s and all 18 CTest targets in 760.39s. Those numbers retain their earlier source scope. [R1–R3]

**Decision:** preserve functional acceptance. Add economic continuity beside it. Do not weaken assertions, relabel validate-only as a solve, or run the entire functional chain after each documentation edit.

### 2.2 Existing economic gate needs an explicit planned-cohort prerequisite

**Observed source:** solver_reports.compare_runs compares typed request/runtime identities, extracts independently evaluated costs and flags loss or increase. Its economic_gate requires nonempty surviving pairs, no exclusions, no missing independent cost in those pairs, no cost regression and equality of the two surviving case-ID sets. The CLI's economic-gate option returns nonzero when that comparison fails. **This does not establish complete planned-cohort coverage.** load_run at lines 606–617 skips ledger records that are neither completed nor marked as having usable partial observations. Lines 619–625 can also omit unavailable analyzable reports when allow_missing_reports is selected. compare_runs at lines 958–963 compares only the loaded cases; its requires_complete_matched_cohort label is not enforcement against an independently declared expected set. summarize_run exposes ledger statuses separately, but their visibility does not make them a comparison-pass prerequisite. This remains the appropriate implementation owner; fix or extend its acceptance boundary rather than add a parallel comparator/database. [S7]

**Derived counterexample from the source, not an executed test:** declare planned eligible cohort E = {A, B}. In both ledgers, A has a compatible completed independently evaluated policy with the same cost; B is memory_budget_refused or a watchdog before any usable observation. load_run supplies only A from each arm. The two surviving sets are equal and nonempty, A has no input/economic mismatch, and compare_runs can return economic_gate.passed=true despite B failing in both arms. If both arms instead have no surviving cases, bool(pairs) is false; the demonstrated hole requires at least one surviving qualifying pair. Asymmetric loss can already fail set equality, so that existing check does not cover this symmetric failure.

**Proposed prerequisite, required before T2/T3 economic acceptance:** freeze E from the authorized manifest/case selection before execution, independently of whichever reports survive. For each arm, account for every selected ID and its raw ledger status, report availability, actual report ID, permitted native termination and process-release outcome. All expected IDs must be present in the selected ledger projection and loaded comparison; unexpected IDs must be explicitly excluded as unselected before acceptance, not silently replace expected cells. A quality pass needs each expected cell to meet its predeclared completed-measurement/status and checked-cost contract. Failed, refused, absent, malformed, no-report, host-censored and unresolved-cleanup cells remain nonpassing with their original categories; usable partial trajectories remain diagnostic/censored evidence. A native bounded/cap policy can still be a valid upper, but any gate acceptance of that termination must be explicit in the predeclared protocol. It cannot repair a missing completed measurement after the outcome is seen.

The acceptance logic is therefore **expected-cohort coverage/status/identity prerequisite AND existing matched economic comparison AND any separately selected capability ceiling**. The manifest's corpus hash alone does not prove all selected cells ran. Until the existing reporter/selected gate owner binds E and statuses, the current CLI economic-gate result is necessary economic evidence, not sufficient release acceptance. Preserve immutable ledger/status records and all planned cases in the denominator.

The independent-cost adapter requires completed/matched evaluation, converged, cost_complete, zero_off_policy_mass, cost_reconciled, finite nonnegative cost and success at least 1−1e−9. Its cost increase criterion uses the existing max(absolute tolerance, |baseline cost| × relative tolerance), defaulting to 1e−7 and 1e−9. It separately reports wall-time changes above both 20% and 100ms, and peak-memory increases above 10%. Those thresholds are observed reporter diagnostics, not newly approved timing release ceilings.

**Limit:** equal expensive fallback costs pass non-regression while both runs fail a separately declared capability target. There must also be a qualified control-quality ceiling where the intended feature promises better discovery. The numerical tolerance is not a license for economic degradation. The canonical bounded-quality argument already makes this distinction. [S8]

### 2.3 Historical corpora have useful coverage and incompatible pins

**Observed manifests:**

| Existing corpus | Membership and role | Operational meaning |
|---|---|---|
| fixtures/solver-benchmarks/v1 | 13 entries: small oracles, renewal fragment, ES cases, refusal/cancellation/stress | General contracts; release script validates it rather than solving the entire corpus |
| solver-quality-ladder/v1 | 18 entries: Conquest 1–5, Spine Bow 1–5, Amethyst Ring 1–4, four armour partial/fractured starts | Nested historical product quality observations; exact evaluation, sampled runs 0 |
| ladder qualification-1024 | 9 entries: clean 4/5 armour and bow, clean ring4, four armour partial/fractured starts | Different work policy: 1024 versus 8, worker step ceiling 20,000ms; case IDs still include product8 |
| cross-base recovery core | 12 entries, all 240s Finish/300s native/315s outer: armour, Bow, Ring, Amulet, Foil, helmet and ES | Broad continuity; declared native retention reuse, fixed inputs, already-exposed strata |
| solver-lab/v1 | 7 selected entries including same-side controls, Bow4, a partial armour control and renewal fragments | Lab profile owns normalization; importing there is not automatically identity preserving |

The quality ladder, cross-base core and Lab manifests pin ABI 2 and source family repoe-e4eaf06c20e1ddb4, with game hash af41b8f4…; the S7 manifest pins ABI 3 and repoe-b458174874efbe64, with game hash 604e36ae…. Current production-source lock pins runtime manifest **82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d** and b458 source. An unchanged historical case filename is not a current-law/data qualification. [S9–S13]

**Decision:** preserve these historical manifests unchanged. Before first routine current-law run, the existing fixture/integration owner must produce an explicitly versioned current-law resolution using the already frozen production inputs, exact descriptors/goal semantics and action envelope. This is provenance reconciliation, not data refresh and not hash substitution to silence validation. Record which premises survive, which change and which comparison is no longer historical matching. If materialization is unavailable or pins disagree, stop before execution.

The **nine-entry qualification-1024 corpus is not the unresolved nine-case release-quality proposal**. No source-grounded resolved membership or approved real quality ceilings were found for that proposal; prior imported research also explicitly requires its owner to supply them. Calling the former “the nine-case release gate” would transfer an incompatible work profile. [S9, S14]

The old S7 manifest still includes a 10,000-Simulator-runs owner sentence. It is historical; current AGENTS specifies 1,000 only when fresh strategy-execution qualification is genuinely required. Ladder/core quality verification uses 0 sampled runs and independent exact evaluation. This report authorizes no simulation, old or new.

### 2.4 Current Conquest negative is a retention and service regression sentinel

**Measured local receipt, source 5a74619339b5283a22074b79026ea3c0177cc0ca:** matched treatment and control both cost **101311.35474896732**; success 1, complete prices, zero off-policy mass, zero cost delta; bounded Finish, cap mask 0. Strategy 6,557 bytes, 26 nodes/39 edges, SHA-256 **d65787d11ad895d5925ce9dd4d0aefde483e53bc25df3a80a6a9dfe7a0d7e053**, byte-identical to control. Native host wall 122.2160041s, independent evaluation 790.0814ms. 33 upper requests were rejected, zero started, zero joint attempts. The declared supervision owner differed, so “matched” refers to native input/economic identity with that host difference disclosed. [L1]

The active primitive candidate cost **627313592.5067186**, while the retained independently checked winner remained 101311.35474896732. The publication record's candidate-kind label alone therefore cannot identify the financial authority. A quality report must carry active candidate, retained best checked artifact and delivered artifact independently. [L1, L2]

**Measured finite receipt:** 179 checks/3 cases pass in 330.640ms on the tiny conditional internal fixture. Complete checking adopts cost 13→3; missing-successor and shared-owner controls retain 13. Normal constructor inactivity is explicitly preserved. This is an excellent finite behavior sentinel, but not proof of normal Conquest discovery, current main availability or current extension qualification. [L3]

**Negative evidence kept:** historical graph repriced at 85970.67 is not a matched historical source run or automatically admitted current product policy; the private roughly 210k candidate remains inferior; the earlier generic plain-fill reconstruction assertion failed. Neither additional machines nor a passing 13→3 fixture resolves those economics. No 3.1 GiB replay is proposed.

### 2.5 Main does not yet enforce the proposed host reservation

**Observed source:** solver_worker.terminate_process_tree at lines 522–524 returns immediately when the parent has already exited. Its Windows taskkill and POSIX killpg paths therefore need not run for surviving descendants after parent exit. run_isolated_process uses bounded communicate calls during ordinary polling, but lines 707–710 and 731–732 perform **unbounded communicate()** after timeout or cancellation cleanup. Lines 744–761 report survivor by polling only the parent. A descendant that inherits and holds stdout can keep communicate waiting after the parent's exit, leaving the parent-only survivor projection insufficient. These are source-supported lifecycle gaps on Windows, Linux and macOS, not solely a Darwin /proc issue and not an executed reproduction in this task. [S17]

The recent parent's native receipts used the separately qualified f532 worker from the isolated d485 CI-supervision worktree. That broad worker is **not merged into main**. Its owned-Windows-job/drain receipts qualify their selected adapter/source and host scope; they do not establish that main solver_worker has those properties or that another host is automatically qualified. Main's scheduler-test failure cleanup repair is likewise separate from this broad process-owner gap. [L1, S18]

**Required before calibration on every admitted host:** select the exact existing supervision owner and source; qualify finite pipe-drain deadlines, descendant ownership and release, including parent exit with descendant-held stdout, timeout, Cancel, failed identity/kill/drain, and pipe EOF failure. Unknown ownership or incomplete drain must remain an explicit failure, halt batch admission and retain its evidence. A selected already-qualified adapter may be reused only within its compatible scope and with its difference from main disclosed. Otherwise harden the existing owner before actual calibration. Do not add a new supervisor or infer bounds from a watchdog argument.

Accordingly, 5.25 minutes below is a **proposed nominal candidate reservation of 3 × 105s**, and 10.5 minutes a fresh pair. They become dependable admission envelopes only after the selected owner's complete bounded cleanup/drain path is qualified. Native, host, pipe-drain and final release deadlines must be explicit; any additional final-drain allowance belongs in the total reservation once, not outside it as hidden unlimited time.

## 3. Precise mathematical contract for the quality tier

### 3.1 Fixed-policy correctness and complete positive support

Let I specify original root, native transition law/data, goal terminal/extras/tier rules, native control/carrier state, full permitted action/programme envelope, prices, observed-choice timing and numerical/evaluation contract. Let π be an admitted controller for I. For a finite reachable nonterminal physical/control state space, let Qπ be the exact declared nonterminal transition matrix and cπ the immediate expected **paid** reward vector.

If π is proper and all relevant expectations are finite, then

Jπ = cπ + Qπ Jπ, and Jπ = (Id − Qπ)^−1 cπ.

The inverse expression is explanatory mathematics; it is not a proposal to build a dense inverse. Properness/transience ensures the Neumann series converges. Complete legality, probability, goal and cost correspondence then makes Jπ(root) a feasible policy upper V*(I) ≤ Jπ(root). Native numerical checking must still satisfy its existing residual/reconciliation/flow requirements; floating equality does not prove symbolic equality or exact MDP closure. [S8, S15]

For a finite native programme word w with complete positive-probability exits z, expected paid word cost Cw and admitted continuation H(z),

Jw(s) = Cw(s) + Σz P_w(z | s) H(z).

This requires every positive exit, actual setup and acquisition, mandatory cleanup, failed draws, paid fresh-base recovery, dependencies and control/observation ports. It excludes silent conditioning on successful draws and post-observation choices before the observation exists. A checked original-root scalar is not a statewise continuation table; unknown H(z) stays unknown.

**Derived counterexample:** a word costs 1, exits to a zero-cost goal with probability .99 and to a mandatory paid-recovery tail costing 10,000 with probability .01. Complete expectation is 101. Omitting the positive recovery exit yields an apparent 1; conditioning on the successful branch does not create a valid cheap policy. A “rare” positive branch is not safely negligible. If the missed branch is instead a closed nonglobal-success class, the properness premise can fail completely.

**Derived scalar counterexample:** a root routes equally to continuations costing 1 and 101, so the root scalar is 51. Reusing 51 as the latter continuation understates its cost by 50 despite exact checking at the root. The regression fixture must preserve graph-only upper authority while continuing to reject unqualified statewise values.

### 3.2 Retention versus capability versus closure

For budget B and identity I, define

U_ret(I,B) = min{ Jπ(root) : π is a compatible fully checked artifact retained and returnable by B }.

If the set is empty, record no qualifying policy, not a finite sentinel. A capability threshold U_cap(I) must be tied to an admitted current-law compatible control. The canonical test is

U_ret(I,B) ≤ U_control(I)(1 + δ_quality) + τ_existing.

δ_quality is a predeclared owner decision; τ_existing is the existing numerical allowance. This report recommends zero **economic** allowance for the deterministic tiny witness. It does not choose an arbitrary real-case percentage. Until owner-approved controls/ceilings exist, real quality is unqualified.

The delivered checked upper must be the cheapest eligible retained artifact at delivery. Candidate validation, suspension, error, cap, Cancel and Finish must preserve the older checked artifact when its contract allows return. Failure to meet capability does not make a complete costly fallback mathematically invalid. Conversely a graph validity pass does not prove discovery quality. Current's TargetNeutralZero profile retains zero global lower/no exact closure even when a tiny fixture meets its known optimum; Finder and authored evaluation have no MDP-optimality authority from this gate.

**Derived retention invariant:** if compatible checked artifacts are added without invalidating previous identities, retaining the minimum makes U_ret nonincreasing with artifact arrivals. This does not prove that two distinct bounded executions have monotone observed values, nor does it certify a now-incompatible artifact. It is a useful finite selection/ownership assertion.

### 3.3 Wall budgets and logical work answer different questions

A product trial asks “what checked policy is delivered under this actual elapsed envelope?” Use budget tuple B = (Finish deadline, native watchdog, host cleanup, logical work, states/rows/transitions, compiler/checker caps, solver-owned memory, host admission). Setup, step transport, final checking and delivery all consume wall time where their owner says they do.

A mechanism experiment may ask “what outcome follows the same actual logical-work allowance?” Fixed-eight is a request quantum ceiling, not a fixed number of completed operations. A faster machine can complete more work before the same Finish; a large 1024 call can delay controls. Versions V1/V2/V3 physical reforge effort and logical_work_v1 are not interchangeable runtime scores. Do not normalize elapsed time by CPU frequency or assume eight requests establish identical work.

**Derived counterexample:** A performs 10 units before a 60s deadline and finds a cost-100 graph; B performs 20 units and finds cost 80. This supports B's same-host product budget benefit under matched premises; it does not isolate a cheaper-per-unit search rule. At equal completed work 10, both may return 100. Conversely a work-efficient method can lose product quality if it spends too much wall time serializing/checking.

Report first observed feasible artifact and first observed control-quality artifact separately. Sample timestamps begin validity at the observed time; a final checked policy cannot be backdated into earlier sparse trace samples. Watchdog with usable atomic partial trajectory is censored evidence, not a completed case; absent trajectory, crash, OOM, failure and Cancel retain their own categories. Keep all planned eligible cells in the denominator.

## 4. Tiers that can be run routinely

This is a **selection proposal**, not an approved run plan or new schedule. An actual execution programme must state exact current-law resolved cases, consumer, activation, ceilings, source, build/module and remaining allowance before launch.

| Tier | Selection and cadence after authorization | Question and stop |
|---|---|---|
| T0: existing functional/identity gate | Existing changed-layer CI; focused finite selector only when relevant source changes | Mechanics, contracts, honest failure, current pins; a quality solve is not implied |
| T1: deterministic quality/retention witness | Once per change to constructor/service/retention/publication; in existing finite native test owner | Complete cheap candidate reaches consumer, checks, retains and exports; negative controls still refuse |
| T2: routine three-case continuity | One serial candidate arm per final solver-affecting candidate; daily latest eligible candidate only while an approved active programme needs daily qualification | Current-law resolved armour5, Bow4 and Ring4; preserve class continuity and missing-policy failures |
| T3: broad release continuity | One serial resolved nine-case candidate cohort before solver activation/release; at most weekly during an explicitly active qualification programme | Broad current-law noninterference, original long budgets; no hidden mode/budget Cartesian product |
| T4: cross-platform or causal diagnosis | Triggered only by an actual mismatch/new platform/owner change, separately bounded | Same-host matched confirmations; transport/correctness correspondence across hosts; exact stop witness |

T2 would reuse the historical 60s Finish/90s native request shapes for conquest-lamellar-allflame-clean-5-goal-product8, spine-bow-allflame-clean-4-goal-product8 and amethyst-ring-allflame-clean-4-goal-product8, **after versioned current-law resolution**. Proposed host cleanup is 105s per cell, i.e. 15s beyond native. It creates a new profile to the extent that host deadline/reservation differs from old records. It does not replace the causal Conquest 120/150/165 result or claim ordinary Calculator 240s qualification.

Why these three: armour tests multi-goal recovery under the current programme, Bow supplies a nonarmour multi-prefix/side pressure control, and Ring exercises a separate carrier/capability boundary. They are test strata, never production dispatch conditions. Smaller one/two-goal cases are valuable functional/oracle controls but insufficient sentinels for the present quality failure.

**Draft T3 membership for owner review only:** cb01 armour5, cb02 armour4, cb03 armour partial3→5, cb04 Bow4, cb05 Bow5, cb06 Ring2, cb07 Ring4, cb08 Amulet3, cb09 Ring3. These are exact existing core case IDs with suffix cross-base-product8-long240, not a claim that this is the previously approved nine. Keep cb10 Foil4, cb11 helmet3 and cb12 ES1 in a separately selected rotating diversity check or a monthly 12-case pass replacing that week's nine. Original frozen-test/validation labels remain “already exposed”; nothing here becomes an unseen holdout.

Bow4 has a specific blocker: the preserved dirty Bow4 is not established as Oliver's separately improved request. Owner selection must bind the actual improved request or report it unavailable; substituting the clean core Bow4 does not discharge that separate requirement. Armour additions from the earlier candidate-construction report likewise remain exact request obligations, not “any armour case.”

**Two actual-worker quality smokes:** choose the exact approved current-law Conquest and Bow request identities only after the integration owner resolves them. Use existing Calculator delivery probe with explicit current, default_finish, repeat=1, adaptive, compact, normal, supplied manifest. Normal suppresses probe-only trace capture. Two original 240/300/315 cells would reserve at most 630s per arm. An early-finish probe measures first-result service and is not the final-quality smoke. Retain cancellation/Finish cleanup and actual exported cost. The LinkeDOM plus real worker/module probe is transport evidence, not rendered visual acceptance.

Finder qualification is a selected additional consumer cell where the touched owner affects Finder. Preserve actual conditional/private grammar, attempt/ranking controls and work budget. Do not silently multiply every Current case by every grammar. Authored checking is useful for unchanged graph compatibility but cannot stand in for search discovery. The tiny witness must exercise actual Current/Finder boundaries where claimed; a shared helper success alone is insufficient.

### 4.1 Baseline reuse and admission

Reuse a completed baseline only if its exact target/law/data/economy/scope/consumer/work-profile/checker/budget/machine and activation are compatible. Reprice or reevaluate a historical graph only as a labelled graph compatibility/economic observation; never relabel it as a fresh matched discovery baseline. Incompatible or missing baselines need an explicit bounded control slot in the later programme. Do not replay compatible controls merely because this is a new session.

All candidate comparisons use distinct immutable output directories. Existing resolver supplies argv, paths, reservation and watchdog; existing runner binds resume to corpus/case/economy bytes, artifact, executable, machine, configuration and treatments. Runtime paths in provenance are legitimate locally; a portability adapter must retain originals rather than rewrite a saved ledger to force a pair.

Before invoking comparison for acceptance, apply section 2.2's expected-cohort/status prerequisite to both raw ledgers and loaded cases. Runner resume identity, printed failure summaries and equality of surviving reporter IDs are not substitutes for that check. Do not treat the current economic-gate CLI exit zero as complete T2/T3 qualification until this prerequisite is bound by the selected existing owner.

The existing reporter includes comparison_profile, session/start/goal/caps/economy, envelope, mechanics, verification, overrides, checker caps, law version, generation, corpus, resolved product_action_ids and runtime corpus/artifact/machine/configuration. Executable is an intentional treatment. Source/law semantic correspondence still needs audit: changing code can change mathematics without changing JSON or the action ID list. An equal price total alone does not establish matching pricing; expected action/resource use, charged base/recovery costs and missing-price decisions remain visible.

## 5. Resource and cost model

### 5.1 Static time arithmetic

These are **derived nominal reservation arithmetic**, excluding build/provisioning, full functional CI, baseline recalibration and any separately approved native-suite/worker budget; they are not measured runtimes, predictions or bounds enforced by main. Section 2.5's bounded drain/descendant prerequisite applies to every admitted host. Any separately added release/drain allowance must be included in the selected total once.

| Corpus/selection, one arm | Finish sum | Native watchdog sum | Proposed nominal host sum |
|---|---:|---:|---:|
| Routine hard three (60/90/105 each) | 180s = 3m | 270s = 4.5m | 315s = 5.25m |
| Small explanatory triad armour3/Bow3/Ring2 (30/30/20 Finish) | 80s | 170s | 215s = 3.58m |
| Entire historical 18 ladder | 645s = 10.75m | 1185s = 19.75m | 1455s = 24.25m |
| Historical nine qualification-1024 | 450s = 7.5m | 720s = 12m | 855s = 14.25m |
| Draft nine long core (240/300/315 each) | 2160s = 36m | 2700s = 45m | 2835s = 47.25m |
| Entire twelve long core | 2880s = 48m | 3600s = 60m | 3780s = 63m |
| Two long actual-worker smokes | 480s = 8m | 600s = 10m | 630s = 10.5m |

For ladder rows, the host column proposes native+15s per case; the corpus's original comparator requires its own compatible declared host identity. A fresh pair doubles the relevant row; reused compatible control does not. Thus routine hard-three initial calibration reserves 10.5 nominal host minutes for a fresh pair, while a complete nine-case pair reserves 94.5 minutes. These are not enforced current maxima: main can block on descendant-held stdout. Sequential deadlines must include bounded pipe drain/release after selected-owner qualification; a host timeout does not manufacture native completion.

Minimum useful active cadence: functional checks on eligible changes, finite T1 on touched capability owners, T2 on every final solver-affecting candidate and at least one latest-candidate check per working day in an explicitly approved daily programme, T3 before release. Calendar frequency alone is not evidence: record candidate SHA coverage and prevent new source slipping past a yesterday-only pass. If no eligible code changes, reuse compatible results; do not solve nightly to produce activity.

**Synthetic monthly capacity example:** 20 T2 candidate arms plus three weekly nine-arm cohorts, one replacing twelve-arm monthly diversity pass, and four pairs of worker smoke cases: 20×315 + 3×2835 + 3780 + 4×630 = **21,105s = 351.75m = 5.8625h** of nominal serial host reservation. This explicitly excludes builds/provisioning, full functional CI, baseline recalibration and any additional selected final-drain margin. It is conditional on complete bounded-supervision qualification, not a measured or presently enforced monthly maximum. Pairing every cell would double this subtotal to 11.725h; functional CI can dominate these minutes. The programme must approve such cadence rather than inherit it from this arithmetic.

No hardware purchase is needed to test this proposal. Incremental electricity equals measured average incremental watts × occupied hours /1000 kWh; for illustration only, 5.8625h at 10W is .058625kWh and at 100W is .58625kWh. No tariff, laptop/PC draw, battery life or speedup was measured, so no electricity price or payback is claimed.

### 5.2 Complexity and memory

Resolving a cohort of k cases requires streaming hashing of selected manifest/case/economy/artifact/executable bytes, linear in those bytes. Existing comparison does typed canonical identity checks and summaries per cell; no cross-case policy solve is introduced. Portable artifact validation is likewise linear in transfer bytes.

Controller checking can still dominate. For n reachable physical/control states, m transition edges and q numerical iterations, a sparse fixed-policy equation pass is O(m+n) per iteration with O(m+n) storage, before native programme entry construction and its independent cost. A dense fallback can require O(n²) storage/O(n³) arithmetic on its admitted component; the proposal does not replace it or authorize enlarging domains. Positive-entry checking costs the sum of the actually reached entry obligations, not only the root graph's node count. Do not truncate support to meet a tier deadline.

The native 1 GiB solver cap and 1 GiB candidate/exact evaluator cap are distinct authorities; simultaneous ownership, data, copies, Python/Node/WASM buffers and serialization may exceed either. A host reservation is scheduling bookkeeping, not an RSS hard limit or complete native allocation proof. The inspected worker MemoryReservation explicitly calls itself a host scheduling reservation.

**Conservative proposed initial worker admission:** keep native solver and checker caps at their original 1 GiB; reserve **3 GiB total per process**, using existing worker-headroom-bytes=2 GiB beyond the solver cap. One serial worker and no overlap with compilation/browser qualification. Admit only if measured available physical memory plus agreed OS/user headroom can support that reservation; on the M1, also require green memory pressure and no sustained swap growth. This 2 GiB extra allowance is an engineering safety proposal, not a proven peak bound. If qualification proves insufficient, refuse/defer or obtain a changed resource decision, not silently increase the native budget.

On Windows, a qualified owned-job memory limit may add an enforceable process-tree boundary when the selected existing owner supports it. Main's runner scheduling reservation alone does not provide that. On macOS, process RSS/pressure monitoring is initially admission/censoring evidence, not a claimed equivalent Windows job memory limit. Preserve peak native owned, checker owned, process-tree working set/commit, WASM linear memory and browser total as separate observations. Unit tests that assert tiny work caps cannot be replaced by larger limits because a secondary host has more RAM.

## 6. Independent workers without a new orchestration system

### 6.1 M1 MacBook Air, 8GB: qualification before scheduling

**User context:** owned M1 Air 8GB is confirmed. Apple specifies the M1 8-core CPU (four performance/four efficiency) and 8GB unified memory. Exact macOS version, installed compiler/Python/Node, free disk and actual idle headroom are unverified. No connection or setup was made. [X1]

**Observed portability premises:** engine CMake builds shared/static/header targets with C++20, PIC and GNU/Clang floating contraction disabled. Python binding lookup includes libpoecraft_engine.dylib. These are source support signs, not ARM64 qualification. The engine has deterministic double-double WideFloat for recurrent policy systems, but solver_eval also contains a long-double dense path. It would be wrong to attribute portability failure to a wholly long-double solver or to infer platform-identical numerical qualification from WideFloat alone. [S5, S16]

The additional Darwin-specific blocker is the existing shared worker's process identity: Windows uses creation FILETIME; the non-Windows branch reads /proc/PID/stat for start markers. observe_process_identity's fallback may label absent /proc/PID as proved_absent once a prior token exists. On a native macOS host, /proc-based ownership is not established, and a newly launched child can produce no token. run_isolated_process does create a new POSIX session and signal its group, then checks the parent, but parent absence is weaker than a complete descendant census. Lab retry/resumption/cancellation must not promote an unknown identity to safe cleanup. Section 2.5's unbounded-drain/parent-exit gap independently requires qualification on Windows and Linux as well as Darwin. [S17]

**Bounded later qualification milestones:**

1. Owner-approved isolated project folder and exact source/input bundle; record Darwin arm64, macOS release, compiler/version, CMake/build configuration, Python/Node/runtime and a worker-specific stable ID. Build the existing native targets with Release, no fast-math, initially one compiler job (two only after measured safe admission). Explicit arm64 configuration avoids an accidental Rosetta comparison. CMake documents architecture selection; a macOS build command remains proposed, not executed. [X2]
2. Qualify header/shared/static loading and a small exact native reference plus deterministic properness/cost and numeric-refusal fixtures. Record actual existing tolerances; retain a refusal instead of relaxing 1e−18 residual logic for platform success.
3. Extend only the existing process-identity owner with a Darwin-specific identity/survivor adapter, or explicitly refuse unsupported resumable/background operations. Apple's published proc_bsdinfo includes PID, process-group and start seconds/microseconds, providing an investigation premise for a native start-marker adapter; its SDK/API availability, permissions, races and post-exit semantics need actual qualification. No dependency install or new process service is presumed. [X3]
4. One bounded process-lifecycle batch: normal exit, assertion failure, watchdog, Cancel during child work, parent exits before descendant while the descendant holds stdout, stale/reused identity, inaccessible identity and pipe-drain failure. All owned children and pipe drain must finish under explicit deadlines; unknown observations stay unknown. Do not kill by an unverified PID or broad executable name. Exercise Lab and corpus only where claimed. The same bounded-drain/descendant acceptance is mandatory for every admitted Windows/Linux worker, using that host's exact selected owner, not deferred as a Mac-only gate.
5. After steps 1–4 and selected-owner bounded supervision qualification, one initial matched T2 control/candidate pair on the M1, serial and on external power under the same background load/thermal regime. This is a proposed six-cell calibration with a 630s nominal reservation for the three-case 105s profile, plus one separately bounded lifecycle batch and one source build. Any added drain margin must be included explicitly; main does not currently enforce this maximum. It grants no larger benchmark campaign.

A process-identity adapter should update solver_worker and the directly dependent Lab supervision tests, versioning affected provenance rather than creating a second launcher. Failure to qualify it stops automatic Mac scheduling. The Mac can remain a later manual foreground correctness target if that restricted contract is explicitly selected; no automated safety is inferred.

Apple's Activity Monitor documents pressure, compressed and swap memory, not just “free RAM.” Use those observations to detect interference and pressure during later qualification. Thermal/power drift is a plausible confound; it was not measured here. Do not mix cold startup, warm repeated module and sustained workload as one population. [X4]

### 6.2 Optional Windows x86 worker and spare-parts PC

**User context:** a possible approximately 13th-generation i5 PC with 32GB RAM and RTX3080 is reported; exact CPU and its owner's agreement remain pending. No access, setup, remote-control configuration, self-hosted runner registration or unrelated-file inspection is authorized by this planning context. Public report language deliberately omits relationship/personal information.

**Recommendation:** first trial existing owned hardware at C$0. If the optional PC is later agreed, use a dedicated project-only directory/account scope, approved off-hours, one serial solve worker, unchanged 1 GiB native/checker caps, initial 3 GiB reservation and a bounded agreed process-tree cap only through a qualified existing owner. Start compilation at two jobs outside solve timing. The 32GB capacity can make admission easier but does not justify a 3.1 GiB solver experiment, simultaneous builds/solves or using all cores. The audited native solver owner exposes CPU computation; no CUDA/RTX solver implementation or benefit was established.

A spare-parts x86 PC without confirmed specifications should initially be classified only after CPU/OS/RAM/storage and stability are supplied, not costed as a finished “free server.” If it has less available RAM than the same reservation and user headroom need, assign finite checks or artifact analysis after qualification; do not dilute completeness to fit it.

Correctness portability can transfer a verified immutable graph/input bundle for independent checking after target/ABI/law/scope matching. It cannot transfer a Windows executable to macOS, assume identical graph discovery under a wall budget, pool CPU effort into a single run, or aggregate the machines' RAM into one solver-owned cap. Each attempt is owned and evaluated on one host. Independent workers partition predeclared cells or do same-host control/candidate pairs; they do not form a distributed policy search. Before any calibration, every admitted host must qualify section 2.5's finite pipe drain and owned descendant cleanup at its exact selected supervisor source; Windows capacity alone does not satisfy that gate.

### 6.3 Host identity and comparisons

**Observed source limitation:** machine_provenance currently records system, OS release, architecture, processor/environment descriptor, logical CPU count and Python version. It lacks a stable worker ID, detailed compiler/load/power state and guaranteed unique physical-host identity. On some platforms processor() may be sparse. Two similar hosts can therefore share recorded descriptors. Do not claim the reporter itself proves same physical host.

Propose a small versioned worker-ID/compiler/load/power record in the existing provenance owner or a bound supplemental receipt, with machine baselines tied to that ID. Never rewrite old ledgers to add guessed IDs. Current strict runtime machine/configuration comparison remains intact. Cross-platform correctness review is a distinct evidence disposition, not dropping machine mismatch from performance comparisons.

Recalibrate a host baseline after source semantics, compiler options/version, OS/runtime/module, power policy or resource-admission changes that matter. Recheck only the affected cells under a new declared programme. Initial three alternating matched pairs can be a diagnostic seed for a suspicious timing result if separately selected, but six samples do not establish a universal 95% performance claim. Use raw paired results and ranges; stop if variability/load/identity prevents an attributable result. Confidence claims require an appropriate declared statistical method and further authorized exposure. No pooled Windows/M1 geometric mean is proposed.

## 7. Quiet windows and failure cleanup

The current research task starts no local heavy work at any time. During the supplied gaming windows it also avoids local CPU/IO research:

- October 5, 19:00–22:00 America/Vancouver = October 6, **02:00–05:00 UTC**.
- October 6, 06:00–09:00 America/Vancouver = October 6, **13:00–16:00 UTC**.

Connected GitHub/web/Library reads or a pause are the allowed research routes then. A future worker plan must additionally respect each host owner's agreed availability and stop immediately when the owner needs the machine. Off-host work during gaming is a potential later arrangement, not permission to connect either machine now. Conservative initial policy avoids scheduled experiments in these windows until availability/ownership is selected.

Before dispatch, admit only if the full watchdog plus release/pipe-drain allowance fits before the next quiet boundary. For a 315s cell and a proposed separate 30s aggregate batch cleanup margin, last admission is at least 345s before the boundary. This margin is a planning envelope; do not add it twice if already included in a selected owner deadline. Near a boundary, defer the next cell rather than launch and hope for early completion.

Reuse corpus/Lab/shared worker cancellation and deterministic serial batches. No daemon, job broker, new scheduler service, LLM polling proxy or “pooled memory” architecture. An existing OS scheduler/manual calendar could invoke a predeclared finite batch after later approval; no automation was created here.

On failure: retain actual exit/native status, typed stop cause, cap mask, original/partial report, graph when actually complete, output hash and original argv. Kill only verified owned process trees/groups through their selected owner; prove bounded pipe drain plus parent and descendant release on every admitted host; reserve capacity until release completes. Main's parent-only projection and unbounded post-timeout/Cancel communicate cannot establish these obligations. A failed test summary is not enough if a thread remains. Main's October 5 test lifecycle correction fixes the demonstrated assertion-failure thread hang; it does not qualify the isolated f532 broad worker architecture or solve the original hosted five-second timing cause. The parent's separate f532/d485-worktree receipts remain that adapter's evidence, not merged-main qualification. Preserve these distinctions. [S17, S18]

A crash/OOM cannot become a passing quality cell. Refusal/no-policy remains in the planned cohort. Atomic report presence and at least one observation are required for censored analysis. A cleanup survivor halts further batch admission, with the owner notified through the parent workflow. No unchanged automatic retry, deadline enlargement or new aggregate allowance follows from failure.

## 8. Artifact transfer and retention

Existing Lab export-bundle, immutable corpus ledgers and reporter are adequate. Transfer only a selected project bundle: exact source SHA, build/module manifest and hashes, current frozen input/economy IDs and hashes, case/cohort bytes, resolved argv/treatment/checker caps, worker ID/environment, ledger, compact summary, actual graph and bounded raw log/partial report when needed. Each file gets byte size and SHA-256; verify locally before acceptance. Keep shared cloud references and consumer-local paths distinct.

Do not copy a whole user profile, arbitrary workspace, credentials, personal files or archives. Preserve raw originals once; compression of large original reports is allowed where already owned, but not a reason to dump them into model context. The 19MB trace and later much larger result/partial files were not read here; the compact 7KB native summary answered the relevant question.

GitHub workflow artifacts can preserve failure/candidate evidence; GitHub documents SHA256 digest output and download validation. A digest mismatch produces a warning, so the evidence acceptance owner must explicitly reject mismatched bytes rather than treat successful download as scientific acceptance. Build artifacts are platform specific; graph JSON and exact input/evidence bundles can be portable within their contracts. [X5]

The full report is prepared as one UTF-8/LF local artifact and one unique documentation-only branch addition. Local Library metadata identity is retained separately from report content. The new owned Library direct-create route preserves a successful returned ID even if Windows xattr persistence fails; it must not duplicate an uncertain/successful write. No Library sharing or message to another person is part of the deliverable.

## 9. Minimal falsification experiments and bounded implementation

No experiment below was run by this task. All counts are fresh **proposals**, not inherited execution allowances.

**F0 — static identity falsifier, before any solve.** Resolve one current-law routine case through the existing resolver, then mutate a copied case goal threshold, economy bytes, proof-profile activation, action scope or host identity. Resume/comparison must reject that variant before expensive computation or report it as an excluded nonpass. Preserve old raw output. This tests a concrete identity failure, not a tautological formatting check. The historical ABI/data-family mismatch is already a source witness motivating F0.

**F0b — symmetric-missing/censored-case negative test, before accepting the reporter as a gate.** In a later authorized focused reporter test, freeze E = {A, B}. Supply matching completed/checkable A records and equal A costs in both arms; mark B as memory_budget_refused in both raw ledgers with no report. Also cover B absent from both ledgers and B watchdog-expired before its first usable observation. All three variants must fail expected-cohort/status acceptance and name B, even though surviving IDs equal {A}. Preserve the original statuses rather than inserting a fake completed report. Companion controls: complete matching A/B with acceptable completed/native statuses passes; asymmetric missing B fails; B with usable censored partials in **both** arms remains visible and fails the completed-quality prerequisite even if the partials contain matching checked costs; both arms entirely empty fails. Also refuse a loaded full report with unacceptable native/process status despite matching cost. The new acceptance must consult raw expected coverage/status as well as existing economic comparison, and retain the planned denominator. This report supplies the test specification only: no fixture, production or test code was changed and no test was run. A minimal extension belongs in solver_reports/load_run/build_report or its existing selected acceptance owner, with focused test_solver_reports coverage; no new comparison service is justified.

**F1 — finite capability/retention falsifier.** After integration owner supplies source, use the existing native finite selector. The unchanged conditional 13→3 case must prove the cheap graph through the actual selected consumer and deliver it, while retaining old fallback on missing-positive-successor, exhausted shared owner, unchecked/incompatible/stale graph, constructor-inactive and invalid statewise-table controls. Repeat both graph representations only where implementation claims both. One source-matched finite batch under a proposed 60s host bound; any missing semantic premise stops dependent work. Existing main tests must remain intact. The fb59476f extension is owned elsewhere and unbuilt in the observed snapshot.

**F2 — deliberately missing candidate test.** Disable only the finite cheap candidate offer in an approved diagnostic treatment with unchanged legality/evaluator. It should still return a checked expensive fallback but fail the declared zero-allowance capability ceiling; restore and require the cheaper candidate. A test that only checks “some compiled graph exists” will incorrectly pass both arms. This distinguishes policy validity from feature discovery without a 120s Conquest rerun.

**F3 — actual routine budget noninterference.** Only after F0–F2, current-law baseline resolution and F4b qualification of the selected host/owner's bounded drain and descendant release, one declared serial T2 candidate arm (three cells) and controls only where compatible baseline is missing. Measure active/retained/delivered identities, exact returned cost, first observed feasible/quality time, actual logical work, complete entry status, native and host memory/time, and cleanup. Three cells are a regression signal, not proof of general economic improvement. Unknown Bow improved identity blocks its specific claim; it does not erase independent Ring/armour cells.

**F4 — Darwin ownership falsifier.** In the existing shared owner, a fixture whose parent exits while a child remains must not be accepted merely because parent.poll() says absent or /proc is missing. Exercise PID reuse/inaccessible identity. One later approved small process batch; stop automatic Mac use until it passes. This is the smallest source-supported new-platform gate.

**F4b — bounded drain/descendant falsifier on every admitted host.** In a later approved process fixture, let the parent exit and an owned descendant keep the inherited stdout open. Invoke the exact selected owner under a short predeclared deadline, then test both timeout and Cancel. Acceptance requires finite output drain, verified owned descendant cleanup/release, honest timeout/Cancel status and bounded return; a parent-only survivor=false is insufficient. Add inaccessible identity/termination failure and EOF-never-arrives controls. Run this small qualification on each admitted Windows/Linux/macOS owner and retain exact source/platform scope. Main's early return at lines 523–524 plus unbounded communicate supplies the source counterexample motivating this test; no process fixture was run here. Reuse the existing worker owner or a compatibly qualified isolated adapter, with its difference disclosed. Do not adopt the d485/f532 adapter wholesale as if it were main or qualified across platforms.

Implementation order and bounds:

| Milestone | Existing owners | Concrete completion and stop |
|---|---|---|
| M0: identity and cohort agreement | Fixture generator, solver_lab contracts/resolver, integration programme record | Exact versioned current-law case/cohort pins, true Bow request and ceilings; no runtime before missing inputs resolved |
| M0b: finite supervision qualification | Existing solver_worker and dependent Lab/corpus lifecycle tests, or exact compatible qualified adapter | Explicit finite drain/release and descendant ownership on every admitted host; F4b passes before calibration; preserve unmerged f532/d485 source boundaries |
| M1: economic continuity wiring | solver_reports/load_run/build_report and focused test_solver_reports; finite native tests, publication/retention owner | Bind frozen expected cohort and raw statuses before economic acceptance; F0b detects symmetric omissions; add only needed capability-ceiling/retained-delivered projections; F1/F2 fail the intended faulty behavior |
| M2: routine tier | Corpus runner/Lab shared worker; CI changed-layer owner | Only after M0/M0b/M1: one explicit three-case batch per eligible candidate, immutable outcomes for every expected ID and section 2.2 prerequisite; qualify the proposed reservation, no new queue or full test repeat at each microstep |
| M3: Mac qualifier | CMake/binding + solver_worker process identity + Lab lifecycle tests | Native arm64/numerics, owned cleanup, measured admission and same-host baseline; no broad Mac rollout on partial success |
| M4: release cohort/worker surface | Integration owner, Calculator delivery probe and existing real WASM/web chain | Resolved nine membership, actual selected consumers/module, two approved smokes; no activation from native-only checks |

Report a native improvement promptly if found; do not hide it behind an unlimited broad campaign. Release still needs the selected full gate. Every milestone has separately stated allowed invocations, source/build identities and cleanup ownership in the later programme. One changed premise can justify a reviewed correction; a new chat does not restore exhausted Conquest/P0–P9 budgets.

## 10. Applicability matrix

| Surface | Applies now from source/evidence | Required before new claim |
|---|---|---|
| Native Current | Existing profile/scope, checked upper retention and reporter; local causal negative has exact narrow identity | Actual selected consumer, complete positive-entry checking, best retained/delivered artifact and same-budget control; extension cannot inherit old fixture pass |
| Native Finder | Existing constructor/proposer and graph checking; independent work-owner negatives remain relevant | Exact grammar/ranking/attempt exposure, parent debit/lifetime and actual consumer test; no upper-service or optimum transfer from Current |
| Authored controller evaluation | Proper fixed-policy legality/cost checking under original root/scope | Current-law occurrence binding, every positive entry, prices/recovery; compatibility does not prove native discovery |
| WASM native module | Shared native source plus separate memory/transport/activation obligations | Matching actual module/MJS hashes and runtime, numeric/memory behavior, exact selected worker mode and Finish/Cancel cleanup |
| Calculator/browser | Existing probe exercises DOM/client/worker/module transport; functional release is green | default_finish final quality under product adaptive controls; actual browser evidence for browser-specific claims; rendered acceptance remains owner-specific |
| M1 macOS | CMake/.dylib source signs, Darwin /proc gap and all-host bounded-drain gap | ARM64 build, numeric/refusal gates, finite pipe drain and Darwin identity/descendants, memory admission and own baseline |
| Existing/optional Windows or Linux worker | Host-specific source/receipts; main has parent-only/unbounded-drain gap | Exact selected supervision source with bounded descendant/drain qualification before calibration; optional host additionally needs agreement, exact hardware/runtime and dedicated project scope |

No row creates new positive MDP lower, exact closure, hybrid mode or default activation. Base-specific test requests remain tests; production dispatch must continue to derive from native descriptors, legality, control/carrier context and programme contracts.

## 11. Canonical integration map and doc-ready proposals

Canonical documentation was not edited. After parent coordination, integrate only selected new findings into these exact existing owners:

| Report material | Canonical destination | Proposed integration |
|---|---|---|
| Functional versus quality, routine tiers and complete planned denominator | docs/solver/benchmarking.md — Corpus roles/result kinds, Experiment identity, Existing reporting | Describe T2/T3 as separate selected cohorts; state current economic_gate's symmetric-omission hole and required expected-cohort/status prerequisite; disclose unresolved nine/old data pins |
| Quality ceiling versus retention/delivery | docs/solver/mathematics/numerical-closure.md#bounded-quality | Add retained-active-delivered distinction and finite counterexample; preserve existing δ/τ separation, zero-only and closure caveats |
| All-positive exits and scalar continuation counterexample | docs/solver/mathematics/policies.md#policy-difference | Add concise complete-word expectation/rare-recovery and wrong-state scalar example only if not duplicate |
| Wall-versus-work and machine identity | docs/solver/benchmarking.md — Observation/Experiment identity; docs/solver/resources-resume-replay.md#measurement-boundary | Preserve logical_work_v1 and actual clocks; host reservations are not complete peak or pooling |
| Darwin process identity gap | docs/foundation/tooling.md and solver-lab.md relevant supervision section | Mark macOS automatic ownership unqualified until the existing owner adapter/lifecycle batch passes |
| All-host pipe drain, parent exit and descendant gap | docs/foundation/tooling.md — Waiting/batching/identity preflight; docs/solver/resources-resume-replay.md#measurement-boundary | State main's unbounded post-termination drain and parent-only survivor limit; require finite owned descendant/drain qualification on every admitted host, without transferring isolated f532/d485 receipts |
| Independent host qualification and transfer | docs/foundation/tooling.md — Waiting/batching/identity preflight | Exact worker baseline, serial caps, project-only bundles and existing exporter; no new supervisor |
| Programme selection/remaining work | Existing coordinated active programme README; short HANDOFF pointer | State unresolved request/ceilings, separately proposed allowance and platform qualifiers; no duplicate status database |
| Capability summary after actual qualification | docs/solver/current-status.md | Update only measured consumer/platform rows; this planning report alone changes no qualified capabilities |
| Intake disposition | docs/solver/research.md section 4 | Preserve report once; classify source facts, derived argument, open controls and scoped negatives |
| Claims/generated views | Existing relevant claim histories / solver_reports generated views | No new accepted theorem or hand edit of research-state; use actual owners only if a new proposition/evidence warrants it |

Doc-ready benchmarking paragraph:

> Functional acceptance and policy-quality continuity are separate outcomes. A validate-only corpus check does not establish discovery quality. Each selected quality tier binds exact current-law requests, prices, action/programme and proof scope, consumer activation, build/module, actual work policy, checker controls, host and elapsed envelope. The current reporter can drop unreported failures from both arms and pass the surviving equal-ID subset. Before economic acceptance, bind the independently declared expected cohort to both raw ledger statuses and actual loaded reports, retaining missing-policy, refusal, failure and censored outcomes as nonpassing planned cells. Extend the existing owner and require the symmetric-missing-case negative test; do not create another comparator. Historical case IDs and nested 1024-work qualification fixtures do not qualify a different release cohort.

Doc-ready bounded-quality paragraph:

> Retain separate identities for the active candidate, cheapest compatible independently checked returnable artifact and delivered artifact. A valid expensive fallback can pass graph acceptance while failing a predeclared capability ceiling. A checked root graph without statewise authority cannot supply arbitrary continuation values; every positive programme exit and paid acquisition/cleanup/recovery remains required. Preserve the older qualified artifact across rejected candidates where its contract allows return.

Doc-ready tooling paragraph:

> Independent workers own whole attempts under machine-specific baselines and serial admission. They do not pool RAM or transfer wall-budget qualification across hosts. Main's post-timeout/Cancel pipe drain is unbounded and its survivor projection polls only the parent; qualify bounded drain and owned descendant release on every admitted Windows/Linux/macOS host before calibration. The current non-Windows identity path additionally uses Linux /proc and is not macOS ownership qualification. Admit an M1 worker only after ARM64/numerical and Darwin lifecycle evidence. Isolated f532/d485-worktree receipts are separate from main; optional machines require owner-selected project scope and availability.

Disposition: routine T2/T3 design **proposal**; functional-versus-quality and comparator ownership **source-grounded**; first-version assertion of complete planned-cohort enforcement **corrected/contradicted by source**; symmetric-omission hole **source-derived counterexample, unrun**, expected-cohort/status prerequisite and F0b **required proposal before acceptance**; all-host unbounded-drain/parent-only ownership gap **source-supported open qualification**, F4b **required before calibration**, nominal reservation figures **conditional proposals, not enforced bounds**; static historical pin mismatch **observed**; retained-candidate distinction **measured local evidence plus canonical argument**; Darwin identity gap **additional source-supported open qualification**; current-law cohort/quality ceiling/improved Bow identity **blocked pending existing owner selection**; historical repricing and conditional fixture recovery claims **not accepted**; extra hardware purchase **deferred, C$0 recommendation**.

## 12. Source and evidence index

All main links below use 7252027c80856628ed16734583bfc9d6e166458b.

- [S1 Windows workflow](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/.github/workflows/windows.yml).
- [S2 Release validation script](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/scripts/test.ps1).
- [S3 Documentation classifier](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/ci_changes.py).
- [S4 Solver knowledge workflow](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/.github/workflows/solver-knowledge.yml).
- [S5 CMake targets and flags](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/CMakeLists.txt).
- [S6 Released product receipt](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-04-sol61-product-integration/README.md).
- [S7 Reporter comparison/independent cost/economic gate](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_reports.py): [load_run omission boundary, lines 606–625](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_reports.py#L606) and [surviving-set economic pass, lines 958–963](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_reports.py#L958), rechecked during the review amendment at unchanged main.
- [S8 Numerical closure, bounded quality](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/numerical-closure.md#bounded-quality).
- [S9 Quality ladder generator](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/generate_solver_quality_ladder.py) and [manifest](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/fixtures/solver-quality-ladder/v1/manifest.json).
- [S10 S7 corpus manifest](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/fixtures/solver-benchmarks/v1/manifest.json).
- [S11 Cross-base core manifest and exact exposed roles](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json).
- [S12 Lab corpus/profile links](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/fixtures/solver-lab/v1/manifest.json).
- [S13 Production frozen-source/runtime lock](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/fixtures/repoe/production-source-manifest.json).
- [S14 Main HANDOFF: unresolved cohort and owner sequencing](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/HANDOFF.md).
- [S15 Policy mathematics](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/policies.md#policy-difference).
- [S16 WideFloat](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_solve_types.hpp#L334), [dense evaluator path](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval.cpp#L4828), [binding library discovery](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/bindings/python/poecraft_engine/_binding.py).
- [S17 Shared worker, process identity, reservation and resolver](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_worker.py): [parent-exited early return, lines 522–541](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_worker.py#L522), [unbounded drain/parent-only survivor, lines 667–763](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_worker.py#L667); [corpus resume/serial admission](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/tools/ingest/poecraft_ingest/solver_corpus_runner.py).
- [S18 CI lifecycle living record](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-04-sol61-ci-supervision/README.md).
- [S19 Benchmark contracts](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/benchmarking.md), [resource boundary](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/resources-resume-replay.md#measurement-boundary), [tooling map](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/foundation/tooling.md).
- [S20 Actual Calculator delivery probe](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/test/calculator-delivery-probe.ts).
- [R1 Main Windows success](https://github.com/OliverOrton/poecraft2/actions/runs/37356886718), [main knowledge success](https://github.com/OliverOrton/poecraft2/actions/runs/37356886722).
- [R2 Exact-source branch Windows success](https://github.com/OliverOrton/poecraft2/actions/runs/37351930481), [branch knowledge success](https://github.com/OliverOrton/poecraft2/actions/runs/37351930573).
- R3: integration receipt S6 owns its prior build/Test/CTest timings; not relabelled as R1.
- L1: local-only reviewed receipt docs/active/2026-10-04-sol61-armour-recovery/checks/root-treatment-20261005/summary.json at observed checkout fb59476f; implementation source 5a746193. Compact costs/status/graph hashes copied above, private paths omitted.
- L2: same directory incumbent-lineage-audit.md and compact projection, local-only at observed checkout; source/receipt distinctions retained.
- L3: programme README's conditional-native acceptance and checks/root-only-joint-conditional-native-20261005/summary.json, local-only later than remote head. 179-check scope is not extension qualification.
- Prior Pro input reuse: local research-inputs/README.md imports causal-research.md (Library libfile_15b9cf673c388191853b867e5d40b75b), economic-gap-research.md (libfile_f41959d282d48191bd75f47cf145fc99), complete-candidate-validation.md (libfile_f0d21fa811788191acf82f3642e02c37), candidate-construction-audit.md (libfile_4f4809787f288191a027e3b5108526c9). Relevant planning/obligation portions only; no new byte materialization or independent replication claim.
- [X1 Apple M1 Air specifications](https://support.apple.com/en-us/111883): CPU and unified-memory premise; no host performance prediction.
- [X2 CMake macOS architecture selection](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_ARCHITECTURES.html): proposed build identity premise.
- [X3 Apple xnu process-info header](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/proc_info.h): proc_bsdinfo start/group fields, prospective Darwin adapter premise.
- [X4 Apple memory-pressure/usage documentation](https://support.apple.com/guide/activity-monitor/view-memory-usage-actmntr1004/mac): pressure, compression and swap observations.
- [X5 GitHub workflow artifact validation](https://docs.github.com/en/actions/tutorials/store-and-share-data#validating-artifacts): transfer digest checks and warning semantics.
- [X6 Python subprocess documentation](https://docs.python.org/3/library/subprocess.html): session/process-group spawning premises, not complete platform cleanup certification.

External use is limited to primary documentation/source. A benchmarking-paper search found Kalibera/Jones's primary ACM record, but full ACM text returned 403 and the institutional copy was unavailable; no statistical recommendation here is attributed to unread paper contents. The mathematical arguments above are explicit derivations applying repository contracts, not externally borrowed new exactness results.

## 13. Completion, blockers and next two actions

Completed: pinned-main/remote audit; targeted corpus/release/comparison/ownership reads; preserved local conditional and matched-negative evidence; static budget arithmetic; current-law identity hazards and macOS blocker; tier/cadence/resource plan; exact documentation integration proposals. **Zero builds, tests, solves or Simulator runs** were executed. No connected machine was operated. No live owned compute process is left by this task.

Remaining blockers for execution: existing owner must resolve the actual current-law routine/release requests and quality ceilings, bind the expected-cohort/status acceptance prerequisite and F0b, qualify bounded pipe drain/owned descendant release through F4b on every admitted host, distinguish the improved Bow request, select remaining bounded allowance, qualify macOS identity/numerics/admission and obtain optional PC owner agreement. These block new qualification claims, not delivery of this research packet. The parent supplied the full independent review and both substantive corrections are incorporated here; no implementation/test fix is claimed.

Next actions for parent coordination:

1. Review the corrected comparator prerequisite/tier proposal and ask the integration owner for the exact current-law expected cohort/status protocol/controls/ceilings; preserve the causal programme's latest source and its own remaining gate. Do not plan implementation on the first version's overstated completeness guarantee.
2. Pending user approval, select bounded cohort resolution and gate hardening first: expected set plus acceptable completion/native/process status, symmetric-missing/censored negatives and all-host bounded supervision. Only then select actual same-host calibration and any separate Mac qualification slice, with declared invocations/stop rules. Do not start a broad campaign or connect optional hardware from this report alone.

Delivery identity, Library result and exact new branch head are recorded in the completion response and local delivery metadata after publication; this report does not preclaim a successful write.
