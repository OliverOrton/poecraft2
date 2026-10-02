# KIDS: configured cluster catalogue contract

Oliver's parent delegation approved this isolated backend/data slice on
2026-10-02 after the 15:32 UTC readiness checkpoint. Baseline is
`f08facbb93204bf721048c21b94080ab582ee9f8`; its engine tree equals
`792b37e8e1d73a90c754087eda2332d7586db570`. Worktree is
`C:/Users/Oliver/Documents/poecraft2-clusters-sprint`, branch
`dot/clusters-20261002`. The integration checkout and main are untouched.

## Delivered boundary

`resolve_cluster_catalog_configuration(connection, base_metadata_path,
passive_key, passive_count)` returns immutable source metadata with explicit
`cluster_unsupported`. Exact base and per-size passive keys are required. Counts
must be integers within canonical bounds; booleans, floats and numeric strings
are not coerced. Layout lists are copied without interpretation. No raw tag,
display-name or dense-ID shortcut exists. Two passive keys sharing a tag retain
separate identity, stats and text. A notable name/key cannot stand in for a
passive-configuration key, whether or not it has an actual mod-stat link.

The resolver performs SELECTs only, supports a read-only SQLite connection and
does not change its row factory. Frozen game data, compilation format, support
flags, C ABI, native code and UI are unchanged. Native and Python crafting-session
creation still refuse cluster bases. Current and Finder gain no consumer,
activation, new bound or exactness authority. This is useful configuration-data
groundwork, not enabled cluster crafting. A fixed supplied configuration does
not require a law for randomly generating that configuration.

## Pinned data and concrete native gaps

The canonical and compiled data agree exactly on 3 sizes, 55 passive records and
309 notable catalogue records. Large has count bounds 8–12 and 17 passive rows;
Medium 4–6 and 21; Small 2–3 and 17. These ranges are source metadata, not new
claims about valid socket placement or every current gameplay variant.

Small attack-block and spell-block passive keys share `affliction_chance_to_block`
but retain different stats. Medium aura/curse records have `old_do_not_use_*`
tags; their preservation does not establish current eligibility. Of 309 notable
records, 308 have one mod-stat link; Pitfighter has none. Unique keystones also
occur in the catalogue, which must never be treated as a rollable modifier list.
The actual affliction domain has 334 prefix, 180 suffix and 18 unique mod rows.
All have one group; 17 notable suffix rows share `AfflictionNotableLargeSuffix`.
Explicit required levels are 1, 50, 68, 73, 75, 78 and 84.

Two implementation blockers are already concrete:

- The ordinary native/Python `Jewel` domain branch chooses `misc`. Removing a
  cluster refusal without an explicit cluster domain and selected passive tag
  would select the wrong universe. Native session options and WASM transport
  currently carry only base and item level, not passive key/count.
- 301 notable modifiers add `has_affliction_notable`. Their ordered generation
  rows are large 100, medium 100, has-notable 0, small 100, default 0. Existing
  first-matching arithmetic therefore suppresses another notable on a small
  signature with that tag; large/medium match earlier rows. Native DataImpl
  does not load added tags and effective signatures currently account only for
  influences. Correct native pool/action/exact/cache handling must preserve this
  order and rebuild tags across retained, added and removed modifiers. No
  size-specific shortcut or hand-coded notable-count cap is substituted.

Stored socket/count mods (`JewelExpansionJewelNodes*`,
`JewelExpansionPassiveNodes` and overrides) are unique-generation rows with empty
spawn weights. They do not supply ordinary socket/configuration laws. Base
implicits are empty. The existing ordinary rare reforge draws 4/5/6 and caps to
the session capacity; existing Jewel sessions have 2/2 side caps. Applying that
law to a cluster is not approved by this data contract.

## Exact owner-law decisions before crafting

1. Select the first fixed-input scope: ordinary non-unique clusters, admitted
   sizes/passive types and treatment of preserved old-tag variants. Define what
   supplied skill count includes, fixed socket configuration and any separate
   configuration/count/layout eligibility constraints. No random configuration
   generation is proposed.
2. State configuration preservation through each admitted craft, Scour/restart,
   item editing and persistence. Future identity must bind stable base/passive
   key/count plus item level; a shared selector tag cannot identify the item.
3. Confirm side capacities and the exact total-affix target probabilities for
   admitted reforges, at least Transmute/Alteration and Alchemy/Chaos if selected.
   Source weights/groups do not specify those currency distributions.
4. Confirm the eligible tag/domain/item-level contract for a fixed existing
   cluster: whether canonical modifier required-level cutoff suffices or any
   independent level restrictions apply. The 301-row added-tag facts above are
   pinned source; extending them to a complete native crafting law still needs
   the surrounding admitted-action contract.
5. Select the first action envelope and refuse the rest. Basic ordinary currency
   on fixed non-unique inputs is a candidate for review; influence, bench,
   Essence/Fossil/Harvest, fracture, corruption and unique transformations are
   not admitted by this work. No external game-rule research was performed.

## Qualification

The [qualification receipt](qualification.json) pins source and evidence hashes.
All **53 focused pytest cases pass**, zero failures, in 2.40 seconds. The final
read-only frozen-data probe resolves all **182 catalogue tuples** across 55
passives, rejects 110 out-of-bounds counts plus the unlinked Pitfighter notable,
and confirms all three Python cluster-session refusals. Original SQLite,
manifest and both compiled payload hashes match before and after.

The compatible preimplementation read-only inspection passed 48 checks,
including baseline/engine equality, complete canonical-to-compiled equality,
18 Python plus 18 native cluster refusals (three sizes at levels
1/50/68/75/84/86) and native level-zero refusal. That native evidence uses the
unchanged frozen DLL; it is not a new build or gameplay qualification. Source/diff
review found and corrected a local newline-formatting issue before validation;
there were no failed game tests. Final whitespace/diff checks pass.

Commands, logs and the frozen-catalog probe are under `out/clusters-sprint/`.
The receipt records their paths and hashes, explicit passed/failed/unrun scope
and process status. The parent CPU hold was honored; no test started until the
explicit release. All owned commands have exited with no survivor.

The source fixture covers all three sizes, immutable/stable identity, shared
tags, legacy preservation, invalid counts/keys, unlinked notables, ordered
generation weights, added tags and unchanged session refusals. It builds only
temporary synthetic databases/artifacts through existing ingest/compiler tools.
No original database/runtime artifact is rewritten. Full acceptance, native or
WASM rebuilds, solver/Simulator and rendered UI checks are unrun because their
layers and activation are unchanged.

## Isolation and continuation

No agents, installs, refreshes, pushes, deployments or heavy solver runs were
used. Protected root `0` is excluded from the initial sparse checkout and was
never inspected, staged, changed or deleted. Existing historical 6 native + 2
worker budgets remain exhausted and are not this pass's ledger. Builds/tests
must honor the parent's serial CPU coordination. The approved data slice is
complete and ready for parent review; cluster gameplay is gated by the explicit decisions above, not implicitly selected.
