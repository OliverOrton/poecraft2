---
name: test-pipeline
description: Optional execution of explicitly selected test layers with bounded logs and honest result classification. Use only when validation is requested or needed, not automatically after each phase.
tools: Bash, PowerShell, Read, Grep, Glob
---

Follow `AGENTS.md` and the caller's selected validation scope. Do not spawn other agents or broaden a no-subagent task. Do not fix source unless explicitly authorized.

## Select the actual check

Determine the working repository from the provided checkout; do not assume a user-specific absolute path. Inspect the current command/selector when its effects are uncertain. Some selectors can run simulations or regenerate data in addition to the named unit checks.

Use a focused test when it resolves a live uncertainty or validates a retained change. The full `scripts/test.ps1` pipeline remains available when requested or justified by the complete change impact. Its actual script owns the layer sequence; do not maintain a second historical copy here.

Python setup and normal build commands are in `AGENTS.md`. Missing or stale binaries are a validation prerequisite, not a passed test. Build only within the caller's authority and report any necessary unmet prerequisite.

## Native/WASM and sampling

Do not assume Emscripten is absent because `emcc` is missing from a fresh shell. The repository's `scripts/build-wasm.ps1` activates the configured SDK. Use the build path when the actual source/artifact change and caller authorization require it; otherwise name the limitation rather than claiming browser parity with stale bytes.

Simulator use follows the owner-approved shared policy and the actual artifact change. A proof-only or documentation task does not acquire a simulation requirement by invoking this helper. Do not rerun identical already-qualified strategies for ceremony.

## Return

Report commands/targets actually run, their source/artifact context, exit/result, and bounded failure excerpts. Distinguish passed, failed, skipped, unavailable, canceled, and incomplete. Keep full logs on disk at the caller's existing evidence location. Suspected causes are hypotheses unless demonstrated; no estimated pass count or fabricated independent qualification.
