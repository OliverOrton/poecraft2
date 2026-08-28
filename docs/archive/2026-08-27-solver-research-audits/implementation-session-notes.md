Yes. In that case, run it as a **single long-horizon Codex task**, but keep the implementation scope limited to **Native Solver Lab v0**. Do not ask it to implement the Lab, replay, verified options, RCASSP, and learned guidance in the same overnight run.

OpenAI’s long-horizon guidance uses a living implementation plan, milestone-by-milestone validation, and a continuously updated execution log; its published example notes that this structure supported more than seven hours of work from one prompt. Codex Cloud also provides dedicated environments intended for longer tasks. That makes an overnight-sized run realistic, though no exact duration is guaranteed. ([OpenAI Developers][1])

The research reports were read-only and pinned to `769c3deb...`, so Codex should record and revalidate the actual current HEAD rather than treating every old measurement as current implementation authority.

## Add this near the top of the Codex prompt

```text
## Long-horizon execution contract

This is a long-running IMPLEMENTATION task, not a planning-only task.

After creating the active plan, continue immediately into implementation.
Do not stop after presenting the plan. Do not ask me to approve each gate,
phase, file, dependency, or local commit.

Continue autonomously until one of these occurs:

1. every selected gate is complete and accepted;
2. a hard stop condition in the active plan fires;
3. the execution environment or Codex session ends.

Use the repository's active plan as the living source of truth. Maintain:

- docs/active/<selected-boundary>/plan.md
- docs/active/<selected-boundary>/execution-log.md
- HANDOFF.md

The execution log must always record:

- actual starting commit and dirty paths;
- current gate and status;
- decisions made and why;
- files changed;
- commands and tests run;
- exact failures;
- remaining work;
- the next executable step.

Do not spend the run repeatedly re-planning or re-reading the entire archive.
Perform enough initial inspection to validate the plan, then implement.

Do not wait for input on ordinary engineering choices. Choose the smallest
conservative design consistent with the locked requirements and document the
decision. Stop only for:

- a required Path of Exile mechanic ruling;
- a soundness or authority conflict;
- an irreconcilable current-tree contradiction;
- a hard acceptance failure explicitly named in the plan.

At the end of each gate:

1. run the focused tests for that gate;
2. diagnose and repair failures before proceeding;
3. update the execution log and HANDOFF.md;
4. create a local checkpoint commit when the gate is coherent and passing;
5. continue directly to the next gate.

Do not push, open a pull request, create an issue, or perform another public
action. Local commits are expected so a long run cannot lose all useful work.

Do not run the entire repository acceptance pipeline after every gate.
Follow AGENTS.md: use focused checks during development and the selected final
acceptance once at the end.

When a gate hard-stops:

- do not begin later behavior-dependent gates;
- remove or disable incomplete behavior changes;
- leave the repository buildable;
- preserve neutral infrastructure already qualified;
- use the remaining run to isolate the failure, add focused diagnostics or
  regression tests where appropriate, and write a precise handoff;
- never weaken an invariant or expected result merely to proceed.

If the complete boundary cannot fit in one session, finish the deepest coherent
passing milestone possible. Leave an exact continuation point that another
Codex session can resume without repeating the investigation.
```

## Use this overnight gate order

I would slightly simplify the large prompt so Codex has a realistic vertical path through the night:

```text
Gate 0 — Current-tree baseline and active-plan activation
Gate 1 — Factor the existing corpus-runner process substrate
Gate 2 — Persistent SQLite catalog and immutable experiment/job/attempt model
Gate 3 — Supervisor, memory admission, watchdog, cancellation, and recovery
Gate 4 — Stable JSON CLI and investigation-bundle export
Gate 5 — Thin PySide6 Queue and Run Detail GUI
Gate 6 — Compare and Strategy Summary GUI surfaces
Gate 7 — End-to-end small-corpus acceptance and documentation
```

The first usable checkpoint should be **Gates 0–4**. That gives you a persistent overnight queue and an LLM-friendly JSON interface even if the GUI is unfinished. Gates 5–7 then make it pleasant to use manually.

Keep these outside the overnight task:

```text
scheduler-aware checkpoint/replay
PDR proof-memory repair
verified option/subgoal system
new solver ordering
RCASSP
ML or GPU integration
MCP server
Imprint
web/WASM redesign
```

The JSON CLI is enough for Codex or another LLM to operate the first version. An MCP adapter can be a small follow-up after the operations are stable.

## Best way to launch it

### On your Windows machine

This is likely the most reliable choice because the milestone includes the native Windows process lifecycle, PowerShell build path, and PySide6 GUI.

Save the full focused prompt as an untracked file in the repository root:

```text
CODEX_NIGHT_TASK.md
```

Then run from the repo:

```powershell
codex exec --sandbox workspace-write --json `
  "Read AGENTS.md and CODEX_NIGHT_TASK.md, then execute CODEX_NIGHT_TASK.md fully. This is a long-horizon implementation task. Do not stop after planning." `
  | Tee-Object -FilePath codex-night-events.jsonl
```

`codex exec` is the non-interactive Codex mode, `workspace-write` allows it to edit and run commands inside the workspace, and `--json` records a machine-readable event stream during the run. OpenAI recommends explicit `workspace-write` rather than unrestricted full-access mode for this kind of automation. ([OpenAI Developers][2])

Before starting:

* disable Windows sleep and automatic restart;
* ensure the repository builds successfully once;
* install or otherwise make PySide6 available;
* ensure CMake, the compiler, Python, PowerShell, and the existing artifact are accessible;
* close unrelated high-memory applications;
* use a clean local branch or worktree;
* verify no command in the normal build path will pause for user input;
* ensure the Allflame data and benchmark executable do not require a network download.

Workspace-write mode can normally edit and execute within the selected workspace without interrupting for every command, while operations outside the workspace or requiring network access may still trigger approval depending on configuration. Preinstalling dependencies avoids an unattended run stalling on that boundary. ([OpenAI Developers][3])

### In Codex Cloud

Codex Cloud is suitable for long unattended work and runs the agent in a dedicated checked-out environment with a configurable setup script. ([OpenAI Developers][4])

For this particular milestone, Cloud should not be the only acceptance environment unless its configured environment can reproduce your native build. It can still implement most Python catalog, supervisor, CLI, tests, and GUI structure, but Windows process-tree behavior and the final native GUI smoke should be validated locally afterward.

## One final sentence to add at the bottom

```text
Use all available productive execution time. Do not stop merely because a
reasonable partial implementation exists; continue through the next selected
gate while the repository remains coherent, tested, and within scope.
```

So: **yes, set it up to run overnight—but give it the focused Solver Lab prompt plus the long-horizon execution contract, not the entire multi-year roadmap as one implementation instruction.**

[1]: https://developers.openai.com/blog/run-long-horizon-tasks-with-codex?utm_source=chatgpt.com "Run long horizon tasks with Codex"
[2]: https://developers.openai.com/codex/non-interactive-mode "Non-interactive mode | ChatGPT Learn"
[3]: https://developers.openai.com/codex/agent-approvals-security?utm_source=chatgpt.com "Agent approvals & security"
[4]: https://developers.openai.com/codex/environments/cloud-environment?utm_source=chatgpt.com "Cloud environments"
