# Continuation notes — what survived the recent discussion

**Purpose:** give the fresh implementation session the decisions and caveats from the conversation after v1.1, without turning them into extra prerequisite projects. The governing implementation remains [implementation_plan.md](implementation_plan.md). Original research is [research_report.md](research_report.md); its primary references are in [source_register.json](source_register.json).

## 1. Source and implementation status

On September 10, 2026 the remote `main` query returned `44fdca8ebf88b9b87b2bd832360cb62128e3968a`. AGENTS and HANDOFF were read at that pin. The branch has not exposed the reported M0 work to this research session. Oliver says M0 was implemented and the session then stopped. This is owner-reported local progress, not a claim that this packet inspected or tested it.

The fresh session must check the actual local work and use it. Do not require a push, previous thread, previous Goal state, or perfectly tidy worktree before continuing. Do not interpret absence of M0 on remote main as evidence it needs to be implemented again. Equally, do not invent the exact file or flags the earlier session wrote.

The selected goal is first-policy recovery on the existing CB06 Ring-two and CB08 Onyx-three, followed by genuine cost/proof use and cross-base/product qualification. M0 is not a successful completion of that goal.

## 2. What we already know, and what is only a hypothesis

The prior cross-base record measured no-policy results for both primary cases. Its B1/B2 frontier-priority variants were removed: more selected-frontier service alone did not recover a policy. The proposed native mechanism has not yet been shown to be their cause.

The exact two-state research example demonstrates the distinction. At s, a first-listed cost-1 row goes to t; a second cost-2 row finishes with probability one-half and goes to t otherwise; t returns to s for cost 1. A strict acyclic rank cannot assign either, and assigning first rows cycles forever. Nevertheless the exit/return controller is proper and costs 5 at s, 6 at t. The report identifies how a finite-Q repair can miss it when both members are temporarily infinite.

That finding must be tested with actual native helpers, not only the supplied Python mirror. If a complete existing native path already solves the example, keep that negative attribution and use the captured real failure. Do not force a favorable toy by disabling working machinery.

The safe-region/positive-progress construction is standard finite almost-sure reachability. It generates a candidate. Native selected mass, observations, properness, cost and the emitted graph still need the existing checks. Unknown exits are not goals. A partial-view negative is not native infeasibility.

## 3. Reforge exploration is now an explicit diagnostic, not a presumed diagnosis

Oliver suspects that useful admitted reforge families do not receive enough consideration. The earlier discussions did not prove which family is omitted. Catalogue presence, admitted source/operator eligibility, a complete row, a valued continuation, an executable policy and an optimality-retired action are different stages.

M1 now includes a compact source/operator table at the same captured boundary. Use the existing action ledger and native resolved IDs. Inspect the root and candidate-relevant partials, not every state in the repository. Report why a row did not progress, and distinguish it from a complete row whose successors lack a proper controller.

This is necessary to falsify the upcoming selector hypothesis: it cannot help an action whose full native row does not yet exist. Conversely, building more rows does not solve a selection/closure defect automatically. Do not use a fixed quota or equal wall time for every family without a measured reason, and do not remove uncomputed actions from the exactness ledger.

## 4. Resources: distinguish matched recovery from capacity scaling

Oliver questioned the continuing 1 GiB restriction and asked to understand how caps affect useful capability and runtime. That is a valid research direction. A cap changes the explored workload; it is not an asymptotic-complexity parameter that magically makes the underlying algorithm linear or exponential.

The matched primary recovery experiment remains the existing 1 GiB, 240-second requested finish, 300-second native watchdog and 315-second outer cleanup profile, with all other state/row/transition/refinement/evaluator limits intact. Do not silently enlarge it or use a larger-resource treatment against the old smaller-resource control to claim an algorithmic win.

A separate native capacity experiment is a **conditional follow-through**, not M0/M1 preflight: it is relevant when an identified necessary row or continuation is blocked by a named resource. Use host RAM/CPU and local approved reservation information; this research session does not know Oliver's total RAM or comfortable per-process ceiling. A 4 GiB diagnostic, and later 8 GiB only if still useful and safe, are suggestions from the conversation—not a claim that this host supports them or a blanket instruction to allocate them.

If a safe owner-approved allocation is already available locally, record a separately named scaling profile and perform the smallest discriminating pair at a coherent checkpoint. Otherwise leave the exact capacity question in HANDOFF and continue independent main work. Do not turn unknown RAM into a reason to stop native helper/selection work. Raising only solver bytes cannot remove a strict-state or evaluator limit; report the next limiting owner before selecting another change.

Use retained bytes plus requested scratch and first cap identity. A refusal before allocation may have a realized retained peak well below its cap. Keep process memory, solver ownership, evaluator memory and host reservations distinct. No broad cap/clock/worker-count grid is part of this continuation. Capacity results are reported separately from the unchanged-budget acceptance.

## 5. Zero-goal outcomes: preserve the preference without inventing semantics

Oliver is comfortable restricting exotic salvage after an ordinary reforge produces no satisfactory goal modifiers. The product already has a goal-progress-gated retry concept. Do not reimplement it as if absent or silently widen that restriction in this continuation.

Ignoring a salvage decision is not ignoring the failed outcome's probability or cost. For a synthetic action costing 2, useful-exit probability 0.1 and exit continuation cost 50, with otherwise identical retry:

    V = 2 + 0.1 * 50 + 0.9 * V = 70.

The failures can share a valid retry representation; they cannot be discarded to obtain 7. A zero-goal Normal item, a Rare item, and an intentional setup/control state need not have the same allowed next actions. Keep actual reset costs, kernel context, persistent flags and observed choices.

A restricted policy optimum is not an unrestricted one: if Pi_R is a subset of Pi, then V*_Pi <= V*_{Pi_R}. User willingness to restrict salvage is not proof that its effect is small. For this programme preserve the frozen existing scope. Further simplification would need its own explicit resolved policy-class change and comparison, not an unnoticed modification to make the seed pass.

## 6. Adaptive estimates alongside certified lowers — preserve as an option, do not block recovery

The later discussion proposed separating:

    L(s): certified lower on expected completion cost;
    V_hat(s): adaptive, potentially inaccurate completion estimate;
    U(s): independently verified proper-policy cost at the same entry.

Only compatible certified endpoints establish L(s) <= V*(s) <= U(s). Estimates may propose an action or work order and may move in either direction. Multiplying, averaging, or clipping an estimate does not make it an admissible lower. A bad known policy is an upper, not a label for the optimal cost; a missing continuation is not evidence of an expensive optimum.

A synthetic decision example: an incumbent costs 10,000. Alternatives A, B, C have certified lowers 100, 150, 2,000 and estimates 20,000, 1,500, 4,000. Trying B first is reasonable. If it yields a verified 1,400 policy, C can be retired against that compatible upper. A cannot: it could truly cost 800 despite its estimate. Soft deferral must remain revisitable; it is not exactness evidence.

The adaptive-estimate idea is **not a newly authorized separate model-training or scheduler-rewrite milestone here**. Recover a functioning native controller path first. If later evidence shows ordering rather than missing support/properness is the remaining obstruction, record a bounded ablation proposal keeping the certified lower, checkers and case resources fixed. Use existing estimates/cheap features before an NN. Keep predicted crafting cost separate from predicted computer effort. Do not export the estimate as a public lower, prune permanently with it, rewrite transition probabilities, or borrow a root upper at an unsupported entry.

These are reasoning/design notes from the conversation, not fresh native measurements or a proof of performance. The prior proper-policy research and its arguments remain the basis for M1–M6.

## 7. Knowledge survives through the existing workflow

Preserve the original report and synthetic files once if not already imported. Incorporate native-confirmed or falsified findings into the relevant policy/properness chapter and existing claims under their real preconditions. No automatic claim promotion follows from this package. The normal completion reply identifies canonical destinations; no extra receipt registry is needed.

The existing audits are advisory, were pinned before M0, and do not authorize repeating already-landed build/lint work or deleting safety infrastructure. They distinguish useful adapters from duplicates and live continuation from checkpoint replay. The current task uses their operating lessons rather than launching another audit.

## 8. State carried to the next session

- M0: owner-reported complete; preserve/reuse locally.
- Solver hypothesis: selected, not yet established on CB06/08.
- Reforge coverage: integrated into the M1 diagnostic.
- Larger native capacities: conditional and separately labelled; no unobserved host assumption.
- Adaptive estimates: retained follow-on idea; not a prerequisite or replacement.
- Zero-goal simplification: user preference acknowledged; existing product scope unchanged here.
- Exact closure: remains the primary project goal. Two new policies are coverage progress, not automatically new exact closure or the previous programme's separate numerical gate.
- Work policy: one sequential session, no subagents, no old time deadline or automatic restart, no push; preserve unrelated work and protected path `0` without inspecting it.

## Pinned repository references for reconciliation

These are navigation links, not claims that local M0 files match the remote commit:

- [Remote checkpoint](https://github.com/OliverOrton/poecraft2/commit/44fdca8ebf88b9b87b2bd832360cb62128e3968a).
- [Shared instructions at that checkpoint](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/AGENTS.md).
- [Historical HANDOFF at that checkpoint](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/HANDOFF.md).
- [Current cross-base evidence and removed B1/B2 variants](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/docs/active/2026-09-09-cross-base-capability-recovery/README.md).
- [Existing action-scope/ledger distinctions](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/docs/solver/request-action-scope.md).
- [Existing resource owners and replay limits](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/docs/solver/resources-resume-replay.md).
- [Existing ordering versus proof](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/docs/solver/scheduling-bellman.md).
- [Existing lower authority](https://github.com/OliverOrton/poecraft2/blob/44fdca8ebf88b9b87b2bd832360cb62128e3968a/docs/solver/lower-pruning.md).
