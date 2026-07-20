Docs Restructure Plan
Status: finalized 2026-07-19 and parked — Oliver resolved all open questions but the doc pass is not scheduled yet. Nothing is implemented; no repo files have been changed.

Purpose
Restructure docs/ into a fractal, traversable library: every area has a main doc that explains the whole area and delegates one level down; game-rule rulings get a permanent authoritative home; quick ideas get a per-area notes file with an auto-sorted inbox; tooling and process keep the structure true after every phase of product work.

Goals
Every area answerable top-down: "how does X work" resolves by descending README → system doc, never by reading plan archaeology.
Oliver's mechanic rulings live in one authoritative, dated place that survives plan archival and wins conflicts with all other docs.
Ideas capture in seconds and land in the right area without ceremony.
Docs stay verifiably current: stamps, lint, and an archival extraction checklist prevent silent drift and buried knowledge.
Non-Goals
No change to active/, future/, or archive/ lifecycle behavior.
No change to HANDOFF.md / AGENTS.md roles or the authority chain.
Does not touch or block R3A solver work; every phase lands independently.
No static-site generator, YAML frontmatter, or per-doc ceremony beyond the status header and a verification stamp.
Target Layout
docs/
  README.md              root map: how the whole product fits together,
                         links each area's main doc (replaces flat index)
  direction.md           vision + current chunk + direction, ~80 lines
  glossary.md            project vocabulary, one paragraph per term,
                         links to the owning doc
  decisions.md           dated append-only engineering-decision log
  evidence.md            pinned-case registry with measured history
  mechanics/             authoritative game-rule truth (Oliver's rulings)
    README.md            folder contract + index
    fracture.md          one file per mechanic: Behavior / Ruling log /
    imprint.md           Engine coverage / Open questions
    metamods.md
    ...
  engine/
    README.md            main doc: engine end-to-end
    (existing five references, filenames unchanged)
    NOTES.md
  solver/
    README.md            reworked from crafting-solver-plan.md
    NOTES.md
  product/
    README.md, (existing docs), NOTES.md
  economy/
    README.md, (existing docs), NOTES.md
  foundation/
    README.md            architecture-plan + codebase-structure merged
  notes/
    inbox.md             zero-format quick capture, sorted by /sort-notes
    ruling-needed.md     rollup of open #ruling-needed items (skill-maintained)
  _templates/            skeletons: area README, system doc, mechanics file,
                         NOTES.md, decision entry
  active/  future/  archive/   unchanged
Structural rule that makes it fractal: a doc explains its level fully and delegates one level down, never two. Sub-docs carry a Parent: link back to their area README.

Phases
Ordered so every phase is a safe, independently landable commit: pure additions first, then tooling, then per-area moves, then root trims last (after extraction). Run the link audit after every phase that moves anything.

D1 — Scaffolding (pure additions)
No existing file moves or changes.

Create docs/mechanics/README.md stating the folder contract: only Oliver's rulings enter; every ruling is dated; agents never research or infer entries; on conflict with any other doc, mechanics wins.
Build mechanics files with complete coverage: every mechanic the Emulator, Solver, and Calculator support gets a file (Oliver's direction, 2026-07-19). Enumerate the coverage list from the engine action/strategy vocabulary so nothing supported is missed. Content is transcription only — implemented behavior plus already-recorded rulings — and every file is flagged draft until Oliver confirms it. The richest seeds are:
fracture.md from the 2026-07-18 fracture-usage direction.
imprint.md from the B1 checkpoint/restore contract and the R3 magic-as-creation-condition ruling.
metamods.md from the S8.2 preservation-behavior corrections.
Create NOTES.md in engine/, solver/, product/, economy/ with the agreed format: sections per subsystem plus General; dated bullets, newest first; states open / promoted → link / rejected + why; optional tags (#ruling-needed, #debt, #perf, #idea).
Create docs/notes/inbox.md (raw-lines capture file, no format).
Create docs/glossary.md seeded with the core solver/product vocabulary (carrier, exact kernel, admission, fixed option, focused expansion, junk class, abstract identity, product path, ...), each entry linking to the owning doc. Pool terms point at mod-data-and-pool-semantics.md, which stays authoritative.
Create docs/decisions.md seeded by extracting the engineering decisions currently only in direction.md narrative and handoffs (handle release/rebuild-on-repricing, retained-cache-mode deferral, 0.9942 not-relabelled, no-cap-raised, ...), each dated with a context link.
Create docs/evidence.md pinned-case registry: one section per pinned case (starting with conquest-lamellar-mirage), listing what it pins and its dated measured history with commits (63,479 states / ~30 s eager product; 23.75 boundary; 433 MB → 22.7 MB; ...).
Create docs/_templates/ with the five skeletons.
Acceptance: all new files exist; every supported mechanic (per the engine vocabulary enumeration) has a mechanics file; all are linked from docs/README.md (temporary "Restructure additions" section until D6 rewrites the index); and Oliver has reviewed the mechanics drafts.

D2 — Tooling and capture
Docs lint script in scripts/ checking: every link resolves; every doc under docs/ has a status header; every doc is reachable from docs/README.md; every area README links every .md in its folder. notes/inbox.md and _templates/ are exempt from header/reachability rules.
/sort-notes project skill in .claude/skills/: reads notes/inbox.md, classifies each line to an area/subsystem, appends it dated to the right NOTES.md, rewrites the inbox with → filed to ... receipts (cleared on the next run), leaves ambiguous lines flagged for Oliver rather than guessing, and regenerates notes/ruling-needed.md from all open #ruling-needed tags.
CLAUDE.md additions: the "note:" capture rule (ideas prefixed note: get filed to the owning area's NOTES.md immediately, no discussion), and a pointer to the mechanics/ folder contract.
Acceptance: lint passes on the current tree; a test round-trip through the inbox files correctly; Oliver decides whether lint joins scripts/test.ps1 or stays standalone (open question 4).

D3 — Engine area
Write engine/README.md: how the engine works end to end — data in, item state, pools/weights, actions, bitsets — with each section delegating to the existing reference that owns it, plus a Code section pointing at the implementing directories and one mermaid overview diagram.
Add Parent: links to the five existing engine references. Filenames stay as they are (rename pass is optional polish, open question 2).
Run doc-drift on the engine area; stamp each verified doc with Verified against code: <date> @ <commit>.
Acceptance: lint passes; drift report clean or discrepancies fixed; engine answerable top-down from engine/README.md.

D4 — Solver area
Rework solver/crafting-solver-plan.md into solver/README.md: keep the formalization, state model, action model, transition provider, DP solver, and policy-compile sections as reference; move the completed S1-S7 Phasing section to archive/ alongside the S6/S7 folders.
Do not split product-construction / automatic-candidates / accounting sub-docs yet — that material is still moving under S8.4R and the active plan owns it. Mark the split as deferred until S8 lands (open question 3).
Doc-drift + verification stamps as in D3.
Acceptance: lint passes; no content lost (archived, not deleted); active plan links unaffected.

D5 — Product, economy, foundation
product/README.md and economy/README.md main docs over the existing references, same pattern as D3 (Parent links, Code sections, drift + stamps).
Merge foundation/architecture-plan.md + foundation/codebase-structure.md into foundation/README.md; per-area Code sections (D3-D5) absorb the fine-grained directory listings, so the merged doc keeps only whole-system architecture and top-level ownership.
Acceptance: lint passes; nothing reachable only through the two old foundation filenames (redirect stubs or updated inbound links).

D6 — Root docs (extraction before trimming)
Ordering rule for this phase: nothing is trimmed until its live knowledge has been extracted to mechanics/, decisions.md, evidence.md, or glossary.md.

Verify D1's extraction of direction.md's "Where Things Stand" narrative is complete (decisions, evidence, rulings all captured), then move the remaining per-chunk history into the active plan's status section and cut direction.md to ~80 lines: vision, current chunk, direction of travel, doc map.
Archive implementation-plan.md to a dated folder; confirm its surviving roadmap content is represented in direction.md's Direction of Travel.
Rewrite docs/README.md as the root map: one screen on how engine/solver/product/economy compose, links to each area README, mechanics/, glossary, decisions, evidence, notes, and the lifecycle folders; plus a short task → path table ("how a mechanic works" → mechanics/; "how a subsystem works" → area README; "did we decide X" → decisions.md; "current boundary" → HANDOFF.md).
Acceptance: lint passes; direction.md ≤ ~100 lines; a spot-check that three sampled decisions/rulings/numbers from the old narrative are findable in their new homes.

D7 — Process adoption
Rewrite the Maintenance Rules section of docs/README.md:
Archive extraction checklist: before any plan archives — rulings → mechanics/, decisions → decisions.md, measured results → evidence.md, vocabulary → glossary.md. Then the archive is never-read territory.
After a phase lands: run doc-drift on touched areas, refresh verification stamps.
Size rule: a reference doc past ~500 lines is presumed to contain two docs or archive material.
Notes lifecycle: every note ends open, promoted (with link), or rejected (with why).
Fractal rule: explain your level, delegate one level down.
Update AGENTS.md / CLAUDE.md reading-order pointers if D6 changed entry points.
Acceptance: rules published; one full dry run of the archive checklist executed against the most recent archive folder as a retroactive audit.

Risks
Knowledge buried during trims. Mitigated by D6's extraction-first ordering and the acceptance spot-check; D1 does extraction weeks before D6 does deletion.
Link breakage. Mitigated by the D2 lint script and running it per phase; moves use git mv.
Mechanics/ mis-seeded. Seeding is transcription of recorded rulings only, flagged draft until Oliver confirms; agents never author rulings.
Mid-migration confusion for sessions. Each phase is one atomic commit; docs/README.md carries a one-line migration status until D6; phases scheduled between R3A work sessions, never interleaved with solver commits.
Notes graveyard. Mitigated by the three-state lifecycle and the /sort-notes rollup keeping open #ruling-needed items visible.
Resolved Decisions (Oliver, 2026-07-19)
Mechanics coverage is complete, not lazy: everything the Emulator, Solver, and Calculator support mechanically gets a mechanics file. Coverage list enumerated from the engine action/strategy vocabulary.
Rename engine files to shorter names during D3.
Solver sub-doc split: moot for now — the doc pass is not scheduled; deferral until S8 lands stands.
Lint script stays standalone, not part of scripts/test.ps1.
Note tags (delegated to Claude): starting set is #ruling-needed / #debt / #perf / #idea.
Narrative history home (delegated to Claude): the active plan's status section, archiving with its chunk. No standing docs/log.md — it would grow unboundedly and duplicate the archive.
Parking Note
This pass is intentionally not scheduled. When Oliver green-lights it, the plan's natural repo home is docs/future/ (deferred designs) until D1 begins; until then this file lives outside the repo.