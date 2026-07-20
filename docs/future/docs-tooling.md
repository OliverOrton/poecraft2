# Documentation Tooling

**Status: future placeholder only.** No documentation lint, sorting command,
or silently passing executable exists in this phase.

Parent: [Future work](README.md)

A later, explicitly selected tooling chunk may automate checks that were run
manually during the 2026-07-19 cleanup:

- repository-relative Markdown links resolve;
- non-template documents are reachable from `docs/README.md`;
- each area README owns every document in its folder; and
- stable references carry meaningful code-verification stamps.

Any implementation must report real failures, document its exemptions, and be
reviewed before joining the product test pipeline. Notes classification remains
manual unless Oliver separately chooses a repository-neutral workflow; no
Claude-only or Codex-only sorter is planned here.
