Perform a documentation-only restructuring and knowledge-preservation phase for poecraft2. Do not resume solver implementation or change product/engine behavior.

Start from commit d5e38e3.

Read first:

1. AGENTS.md
2. C:\Users\Oliver\.codex\attachments\587eda38-020e-4321-98b6-47dbb1c4fc4a\pasted-text.txt
3. docs/README.md
4. docs/direction.md
5. HANDOFF.md
6. docs/active/bestiary-and-solver-capability-plan.md
7. docs/implementation-plan.md
8. docs/archive/README.md

Oliver’s decisions for this phase:

- Archive the current active plan intact.
- Replace it with no active implementation plan.
- HANDOFF.md should be nearly blank afterward: no solver history, optimization menu, or speculative next task.
- Preserve unresolved R4–R6, S8.5 and later work in a short non-executable future roadmap.
- Build a complete mechanics library covering every mechanic supported by Emulator, Solver, or Calculator.
- Mechanics content must only transcribe implemented code behavior and existing dated Oliver rulings. Never research, infer, or invent mechanic rules.
- Rename stable reference files where shorter, predictable names improve navigation.
- Plans, audits, reports, handoffs and other point-in-time documents should live in clearly dated archive folders.
- Reorganize and verify stable documentation against current code, not merely move files.
- Do not implement documentation lint tooling now. Leave only a clearly labelled future placeholder; do not create a fake executable that silently passes.
- Create lightweight NOTES.md files and a raw notes inbox, but no Claude-only or Codex-only sorting automation.
- Add verification stamps to stable documents that are genuinely checked against code.
- Create permanent decisions and evidence indexes.
- Use multiple safe local commits.
- No product test pipeline is required.

Desired final state:

- docs/README.md is the primary documentation entry point.
- docs/direction.md is short, approximately 80–100 lines.
- HANDOFF.md says only that no implementation boundary is currently active and Oliver must choose the next chunk.
- docs/active/ contains only a minimal README stating that no plan is active.
- No new large execution-plan document replaces the archived one.
- Completed history remains available in dated archives.
- Stable contracts are reachable top-down through area README files.
- Every document has an obvious owner, lifecycle and parent.
- Future sessions can find mechanics, architecture, decisions, evidence, deferred work and historical audits without reading plan archaeology.

Use subagents deliberately. Parallelize only disjoint areas so agents do not edit the same files:

1. Mechanics agent:
   - Enumerate the complete supported mechanic/action vocabulary from engine, solver and product code.
   - Map existing Oliver rulings and implemented behavior to mechanic families.
   - Draft docs/mechanics/README.md and one file per mechanic family.
   - Mark anything requiring Oliver confirmation explicitly.
   - Never use external research.

2. Engine/foundation/WASM agent:
   - Audit foundation and engine documentation against code.
   - Propose and perform approved short-file renames.
   - Create the engine and foundation area READMEs.
   - Add a code-based WASM capabilities reference covering:
     - build flags and memory configuration
     - exported C ABI
     - cooperative solve begin/step/finish/abandon support
     - worker integration
     - cancellation/progress behavior
     - memory statistics
     - known native-versus-WASM unknowns
   - Do not run performance tests or make engine changes.

3. Solver/product/economy agent:
   - Audit stable solver, product and economy references against code.
   - Create their area READMEs and shorter reference names where useful.
   - Separate stable contracts from historical phasing.
   - Identify unresolved roadmap material that belongs in future docs rather than active plans.

The root agent owns:

- the complete move/rename map
- archive naming and snapshots
- decisions/evidence/glossary extraction
- direction, HANDOFF, active placeholder and root documentation map
- AGENTS.md and CLAUDE.md reading-order updates
- integration, link repair and final review of every subagent’s work

Before editing:

1. Inventory every Markdown document, its size, purpose, authority and inbound links.
2. Classify each as:
   - stable reference
   - current orientation
   - mechanic authority
   - future/deferred work
   - active plan
   - historical plan
   - audit/report/evidence
   - notes
3. Produce a proposed destination and filename for every moved or renamed document.
4. Identify duplicated facts and choose one permanent authority for each.
5. Ask Oliver before proceeding if any document’s authority, mechanic content, or destination remains ambiguous.

Naming rules:

- Stable references use short descriptive names without dates.
- Historical phases, plans, reports, handoffs and audits live under:
  docs/archive/YYYY-MM-DD-or-YYYY-MM-topic/
- Do not repeat dates in filenames when the containing archive folder already supplies the date.
- Use descriptive type names such as plan.md, audit.md, report.md, evidence.md or handoff.md.
- Use git mv for tracked moves.
- Update all repository-relative links after moves.
- Avoid redirect stubs unless a known external link requires one.
- Archived wording may be preserved, but its status must clearly say it has no current sequencing authority.

Handle the current large documents as follows:

- Archive docs/active/bestiary-and-solver-capability-plan.md intact in a dated S8/B1 archive.
- Archive a snapshot of the current HANDOFF.md before replacing it.
- Archive docs/implementation-plan.md after extracting its surviving future roadmap.
- Archive completed phasing/history from docs/solver/crafting-solver-plan.md; turn the stable solver architecture into docs/solver/README.md.
- Treat root SOLVER-IMPROVEMENT-PLAN.md as a dated solver audit/plan input: archive it rather than leaving it at repository root, and extract only genuinely unresolved recommendations into future/solver-roadmap.md or area NOTES.md.
- Preserve detailed R3A measurements in their existing fixture evidence JSON. docs/evidence.md should index and summarize them, not duplicate the entire report.
- Preserve the attached documentation-restructure plan as a dated historical plan in the final documentation-cleanup archive.

Target structure:

docs/
  README.md
  direction.md
  glossary.md
  decisions.md
  evidence.md

  mechanics/
    README.md
    <one file per supported mechanic family>

  foundation/
    README.md

  engine/
    README.md
    data.md
    items.md
    pools.md
    weights.md
    bitsets.md
    wasm.md
    NOTES.md

  solver/
    README.md
    NOTES.md

  product/
    README.md
    <short stable references>
    NOTES.md

  economy/
    README.md
    <short stable references>
    NOTES.md

  notes/
    inbox.md
    ruling-needed.md

  _templates/
    area-readme.md
    reference.md
    mechanic.md
    notes.md
    decision-entry.md

  active/
    README.md

  future/
    solver-roadmap.md
    docs-tooling.md
    <other deferred designs>

  archive/
    README.md
    <dated historical folders>

This is a target, not permission to force poor groupings. Adjust names after the inventory when a clearer structure is justified.

Fractal navigation rules:

- Each area README explains that area completely at a high level.
- It links one level down to every owned reference.
- Each sub-reference has a Parent link to its area README.
- A document should not require readers to jump two levels to understand its place.
- Stable references describe current implemented contracts.
- Execution history belongs only in archives.
- Deferred proposals belong only in future/.
- Notes are not authority.
- mechanics/ is authoritative for Oliver’s mechanic rulings.
- decisions.md is authoritative for durable engineering decisions.
- evidence.md is the index for pinned cases and measured history.
- HANDOFF.md is authoritative only when an implementation boundary is active.

Mechanics file structure:

- Status and Parent
- Scope
- Implemented behavior
- Dated Oliver rulings
- Engine coverage and code pointers
- Emulator support
- Solver support
- Calculator support
- Explicitly unsupported behavior
- Open questions requiring Oliver

Do not turn action IDs into hundreds of tiny files if they belong to one coherent mechanic family. The mechanics index must nevertheless prove complete coverage of the supported vocabulary.

Notes structure:

- General plus subsystem sections where useful.
- Dated entries, newest first.
- Every entry is open, promoted with a link, or rejected with a reason.
- Allowed starting tags:
  #ruling-needed
  #debt
  #perf
  #idea
- notes/inbox.md remains raw and easy to edit.
- notes/ruling-needed.md manually indexes current unresolved mechanic questions.
- Do not create sorting automation in this phase.

Verification stamps:

Use:

Verified against code: 2026-07-19 @ d5e38e3

Only apply this where the document was genuinely checked against implementation. Record a narrower scope if only part of a document was verified.

Extraction-before-trimming rule:

Do not remove material from direction, HANDOFF, implementation plans or solver plans until it has a permanent destination:

- mechanic ruling → mechanics/
- durable engineering decision → decisions.md
- measured/pinned result → evidence.md or linked fixture evidence
- stable implemented contract → appropriate area reference
- unresolved future work → future/
- completed narrative/phasing → dated archive
- vocabulary → glossary.md
- loose idea → area NOTES.md

Do not duplicate full historical narratives in stable docs. Link to the archive or evidence source.

WASM audit requirement:

The new docs must accurately establish what the current WASM product can and cannot do based on source inspection. Specifically inspect:

- scripts/build-wasm.ps1
- bindings/wasm/wasm_api.cpp
- exported release module surface
- C ABI solver functions
- web worker protocol and solve stepping
- memory/cancellation/progress handling
- any product caps or browser-specific defaults

Clearly distinguish:

- implemented capability
- exported but unused capability
- native-only evidence
- WASM evidence
- unknown/unmeasured behavior

Do not infer runtime performance from native results.

Commit sequence:

1. Inventory, scaffolding and dated archive snapshots.
2. Mechanics library and durable authority extraction.
3. Stable area READMEs, code verification and renames.
4. Future roadmap, notes, decisions, evidence and glossary.
5. Root cleanup: docs/README, direction, active placeholder, HANDOFF, AGENTS and CLAUDE.
6. Final link/reachability review and archive index update.

Each commit is local only and ends with:

Co-authored-by: Codex <codex@openai.com>

Validation:

- Do not run engine, web, ingest or product test suites.
- Perform a one-off Markdown link audit after moves; do not commit lint tooling.
- Confirm every non-template Markdown file is reachable from docs/README.md through its area/archive indexes.
- Confirm every area README links every owned document.
- Confirm no tracked links reference old moved filenames.
- Confirm no stable document presents archived sequencing as current.
- Confirm no mechanic statement was researched or inferred.
- Confirm working-tree changes are documentation/process files only.
- Preserve unrelated user changes.

Final acceptance:

- HANDOFF.md is minimal and has no selected next implementation task.
- docs/active/ has no execution plan.
- direction.md is at most about 100 lines.
- The former active plan, HANDOFF history, implementation plan, solver phasing, solver audit and docs-restructure plan are preserved in dated archives.
- Stable architecture is organized by area and verified against code.
- Supported mechanics have complete indexed draft coverage.
- Decisions, evidence, glossary and notes have permanent homes.
- Current WASM capabilities and unknowns are documented from code.
- All links resolve.
- No engine or product behavior changed.
- No solver optimization work began.
- The repository is ready for Oliver to choose a fresh next phase without inheriting the old plan’s sequencing momentum.

At the end, report:

- the new top-level documentation map
- all renamed/moved files
- all archived plans/reports
- mechanics coverage and questions requiring Oliver
- WASM capabilities and remaining unknowns
- verification scope
- commit hashes
- confirmation that HANDOFF and active planning are intentionally empty