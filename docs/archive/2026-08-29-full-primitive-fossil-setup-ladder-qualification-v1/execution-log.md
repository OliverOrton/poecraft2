# Full-Primitive Fossil Setup And Carrier-Ladder Qualification v1 Execution Log

**Status: archived diagnosis-only / evidence-complete.** This log records
chronological execution evidence for the [plan](plan.md). The result preserves
the incomplete authoritative Simulator sample and deferred full suite without
converting either into a pass.

Parent: [Archived boundary](README.md)

## 2026-08-29 — Boundary Activation

Oliver selected the full-primitive Fossil setup and carrier-ladder
qualification boundary for an unattended long-running goal.

Starting repository state was verified before mutation:

- HEAD: `f975c0cb4eb2b4069473e6f9281c208d40c8f29d`;
- branch: `main`;
- upstream `origin/main`:
  `f975c0cb4eb2b4069473e6f9281c208d40c8f29d`;
- tracked worktree: clean; and
- sole status entry: protected untracked `0`.

The protected file was not read or modified. No implementation, build, test,
CLI submission, service restart, or run mutation occurred during activation.

The retained starting Fossil witness from the prior generated-envelope
boundary is:

- revision `case-rev-c1b3c87d43e16122687521d348cf38b5`;
- content SHA-256
  `c1b3c87d43e16122687521d348cf38b5db2a1c8ba748a130ef15f364ec4aae88`;
- job `job-709e39e0-5ead-4fce-9a9c-dca35cd726aa`;
- attempt `attempt-2db95f4b-ebc7-4fab-a17c-e4e55a246cb8`;
- watchdog terminal after 60.161 seconds;
- no native partial report or phase owner;
- zero progress samples;
- empty worker log identity
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`;
  and
- supervisor cleanup reported no surviving process.

This evidence selects setup attribution first. It does not yet authorize a
Fossil, action-envelope, ladder, mechanic, cap, ABI, strategy, or product
behavior change.

## Evidence Recording Contract

For every persistent run, append a bounded record containing:

- case and immutable revision IDs;
- request/content/core/component identities;
- job and attempt IDs;
- artifact paths, IDs, sizes, and SHA-256 hashes;
- selected setup, action, ladder, work, memory, bound, policy, and terminal
  fields;
- cancellation, process-tree, and reservation disposition;
- before/after comparison and conclusion; and
- the source commit and executable/artifact identities used by the run.

Raw JSON reports and bundles remain in their structured artifact locations and
must not be pasted wholesale into this log or agent context.

## 2026-08-29 — Gate 0 Reproduction And Attribution

Execution began at clean tracked activation commit
`e7b81a0143ed1b348ed9a80b28be44916cfd7a9d`; branch `main` was ahead of
`origin/main` only by that activation commit, and protected untracked `0` was
the sole status entry. The file was not read or modified. The CLI service was
healthy: profile `native_allflame_no_imprint_v1` retained content SHA-256
`876824a29d51ef8e87013639a86120315ca13235833261980b3eb28917b6bb56`,
all seven frozen cases were present, dispatcher ownership was released, and
there were no live leases or reserved host bytes.

The historical revisions used by the prior add-back matrix were inspected
through the CLI. All five relevant revisions had
`action_mode=goal_relevant`, but their nested product envelope omitted both
`fossil_mode=goal_relevant` and `requested_fossil_actions=[]`:

- full-minus-Fossil `case-rev-096465054dabdb6899368ade9d9ff928`;
- Fossil add-back `case-rev-c1b3c87d43e16122687521d348cf38b5`;
- full primitive `case-rev-ac23c2eaefb561d22f4f92f7bdd09f1b`;
- cumulative full primitive `case-rev-9456180796c2d052e4b0b9b8d359be01`;
  and
- cumulative full-minus-Fossil
  `case-rev-cdfb71c9e2403db6b20d067ea8b42e91`.

This is the already-repaired Native Solver Lab authoring defect recorded in
the archived CLI-workflow boundary: the pre-fix localization path dropped
envelope-only Fossil controls. Immutable revisions truthfully preserve the
bad request. Current checked-in product fixtures and the current localization
path both preserve `fossil_mode=goal_relevant` and the empty requested list.

The matched old full-minus-Fossil request completed normally:

- job `job-4686b8b0-7533-4dd9-b10e-d2dbb1e6c4bc`;
- attempt `attempt-67669716-d846-40dd-bc76-907723aa2c5b`;
- job/full/core identities
  `67aaaffe1f833cc1df0533d7544a05c11c93928f58a0963710a7a5ff96b9bb0f`,
  `3e2f712116ef75fe8f0d4bd74a9cc351d404562bb791c3ae018036009c2400fa`,
  and
  `952afddfe4c4d394aec29520d5112026ccae05cdb64a45f4c0d79e3b28aca3ba`;
- natural `completed/refused_resource_cap`, 32 expanded states and 212 rows;
- independently evaluated bounded-feasible strategy with three nodes, three
  edges, SHA-256
  `6ea9b6dbd6296f6c28312478525edca27cfcdd207c318a0b8c9130c4bed6555e`;
  and
- report 404,182 bytes, SHA-256
  `9de81bd69812dde225752c5cea411a7f3b417c92316531bef6de9d4998f245cc`.

The old Fossil-addback revision reproduced its synchronous setup trap under
the current source identity:

- job `job-7c06fdcb-2f64-4659-9daa-ebdef0f39a19`;
- attempt `attempt-c6f4322c-aac0-45d4-8e08-876047d6e5cb`;
- job/full/core identities
  `b7d82549c025c3bea195c6b7b01e58a2af5545fe295f44dcf0b0db5706afb7a9`,
  `7018b27c338e19491a8523edf6c779c0e37d526e231a234ebd0674db6438266d`,
  and
  `4d79ecd5ab08dd8ce00629213770b700e67d85081cfdaea47e7c3239a188d468`;
- after about 40 seconds it remained at phase 0 with zero states, rows,
  native telemetry, or compiled graph;
- its partial checkpoint proved that the product-envelope probe had already
  returned 12,959 actions: 12,950 exhaustive one-through-four-Fossil
  loadouts plus nine currency actions;
- the owner was therefore the second synchronous `pc_solver_create`, which
  rebuilt the explicit 12,959-action request before cooperative solve work;
- CLI cancellation terminalized after 48.434 seconds with 466.277 ms
  acknowledgment using
  `graceful_then_process_tree_termination`;
- worker PID 37720 was absent, dispatcher ownership released, and both live
  lease count and reserved host bytes returned to zero;
- partial report 2,767,240 bytes, SHA-256
  `c7f9ef76460947ebc6d8dc6713ab879e2179d810628b5b1a3e12c7069b438257`;
  and
- supervisor terminal record 1,805 bytes, SHA-256
  `cc4187b88c2adfc556e184c5856ad8db5e410f446d2b664920e5a39abda5d96b`.

No direct envelope edit was made. The CLI correctly refused such a patch as
outside its bounded registry. Instead, the current checked-in clean-five
fixture was imported, natively validated, and frozen as revision
`case-rev-5916e3d1908ff18c3f2b432b2a0f6cc3`, content SHA-256
`5916e3d1908ff18c3f2b432b2a0f6cc3276bf21914c4f4e22aacdfa961ffec26`.
It owns the intended bounded Fossil fields. Three registered derivations were
then validated and frozen:

- Fossil add-back `case-rev-b7eb5e639f6c7f85372bf49f5916edd5`, SHA-256
  `b7eb5e639f6c7f85372bf49f5916edd5d416d1b6c17ec622f2f034db62be96c6`;
- full-minus-Fossil `case-rev-8f1c8d5d8686b6dd250f3ce33acfceee`,
  SHA-256
  `8f1c8d5d8686b6dd250f3ce33acfceee7300e8ce2e30ed82ea07eb36c651e1cb`;
  and
- full primitive `case-rev-49bd02aa57ce7d739d21410212da2c5b`, SHA-256
  `49bd02aa57ce7d739d21410212da2c5b434ad2ea0ac23ad9682114ca40a8a78a`.

The corrected Fossil-addback request completed naturally:

- job `job-379d2766-2cf2-41c4-acc8-bc5b907a4329`;
- attempt `attempt-e7db1f73-1b9a-46cd-9c7c-b13d7ff545bc`;
- job/full/core identities
  `df0ae2f573c4b25c644370a28c92fd2b05114f46cc448d442d05aca6749fed5e`,
  `fb6a94c313b30738f0d5149667af29699b95c43567a785a48d58096c3315119c`,
  and
  `970acb8750ce0d71b71f3316edaa5f0292a0a20b8bca5633e8d81773ed3d71b0`;
- source commit `e7b81a0143ed1b348ed9a80b28be44916cfd7a9d`, executable SHA-256
  `33b00a678191e53eac057b756cc121516635c7f6f875857bc84c1b01bfeb2450`,
  artifact manifest SHA-256
  `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`,
  and economy SHA-256
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`;
- bounded Fossil accounting: 12,950 possible, 13 generated, 12,937 deferred,
  lazy requested mode in the final explicit solver, and 22 layout primitives;
- registry/layout 28.334 ms, registry generation 1.208 ms, setup owner 3.121
  seconds, solve 15.757 seconds, and total native case 16.736 seconds;
- 1,347 discovered/expanded states, 5,144 rows, 1,415,110 reforge work,
  and 11,293 transitions;
- native peak owned bytes 80,452,689;
- `completed/refused_resource_cap`, bounded feasible, lower
  `38.962958772833694`, independently evaluated upper
  `4552297.606053835`;
- exact evaluation matched with success probability 1 and zero off-policy
  mass;
- strategy 150 nodes/517 edges, 445,256 bytes, SHA-256
  `559a854ade69dcf51b3a8e718edc58a55355e3c36f2ca2265723a6ddfdea2ecb`;
  and
- report 853,068 bytes, SHA-256
  `374bac28c1cd300c76e65e71eb22102e89d974f24d00b335815673bb0f847e9c`.

Gate 0 conclusion: there is no current bounded Fossil setup defect to repair.
The apparent blocker was a stale immutable request authored before the CLI
envelope-preservation fix. Exhaustive native construction remains distinct
and valid; current bounded product construction is observable, cancellable,
and naturally terminal without a cap or mechanic change.

## 2026-08-29 — Gate 1 Exact Fossil Controls

Existing native controls already owned:

- an explicitly requested one-Fossil public-API action;
- bounded goal-relevant generation over the real artifact, including the
  qualified one-slot count of four generated and 12,946 deferred loadouts;
- a two-named-Fossil synthetic exhaustive registry with canonical order
  `fossil_a`, `fossil_b`, `fossil_a+fossil_b`; and
- a real-artifact exhaustive combinatorial count for all named one-through-
  four-Fossil loadouts.

The missing explicitly requested multi-Fossil oracle was added beside the
requested-single API control. It retains no engine behavior change and checks
the canonical Cold+Defences identity, both Fossil component price keys,
`resonator:2`, exactly one candidate, and telemetry accounting of 12,950
possible, one generated, 12,949 deferred, lazy requested mode.

Focused milestone acceptance:

- `powershell -File scripts/build.ps1`: passed;
- `poecraft_engine_tests --solver-api-only data/compiled/current`: 2,831
  checks, zero failures;
- `poecraft_engine_tests --solver-abstract-only data/compiled/current`:
  40,075 checks, zero failures; and
- `py -3 -m pytest tools/ingest/tests/test_solver_lab_cli_workflow.py -q`:
  six passed in 4.10 seconds, including the regression that preserves the
  frozen envelope's `fossil_mode=goal_relevant` and empty requested list.

The Gate 0 cancellation provides the required pre-solve teardown control:
terminal process-tree removal and complete reservation release were verified.
Gate 1 passes. Gate 2 requires no engine repair; only the missing exact
requested-multi regression is retained.

## 2026-08-29 — Gate 2 Diagnosis-Only Disposition

The measured owner was stale request authoring, not current engine setup.
Consequently Gate 2 retained no runtime repair, cap change, action restriction,
mechanic change, alternate Fossil representation, planner, ABI, strategy
vocabulary, WASM, or product change.

The only source change is the explicitly requested multi-Fossil native oracle
described above. It was committed locally as
`1029801df505299d7528754277529350686674d3` (`Qualify bounded Fossil setup
controls`) with the required Codex co-author trailer. The qualification
executable and immutable runtime identities used from this point are:

- native benchmark executable SHA-256
  `9cd60e200374b429f2731babf306114f31eff3d9d3d7b67ea9530c6b8c0cbabc`;
- compiled artifact manifest SHA-256
  `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`;
  and
- economy snapshot content SHA-256
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`.

Gate 2 therefore closes diagnosis-only. The full-envelope and ladder runs
below qualify current behavior; they do not compare a behavior-changing
repair.

## 2026-08-29 — Gate 3 Full Primitive Envelope

### Full-minus-Fossil control

Correct immutable revision
`case-rev-8f1c8d5d8686b6dd250f3ce33acfceee`, content SHA-256
`8f1c8d5d8686b6dd250f3ce33acfceee7300e8ce2e30ed82ea07eb36c651e1cb`,
ran as job `job-f5cc0271-f63f-4d74-8d06-cb17324d13c4`, attempt
`attempt-175ba5c7-179d-4781-b4a9-31598777ab5f`.

- job/full/core identities:
  `bb8e94e40b75bd9f5dcaea884c0b2549a1d04874500d8117f033cba956f392a9`,
  `f5e468e7cae2e55ea64668847f8916a1c83733786f330c2baa8b60cea103e24c`,
  and
  `e4cfd0dcaa9adcff2e9b6a5b6eeadeffc1868cc46329af0ab95ccebc19aa2154`;
- natural `requested_bounded_finish`, bounded feasible, lower
  `36.48853172876641`, upper `1550334.436668944`;
- 9,816 discovered, 4,457 expanded, 5,359 frontier states, and 24,578 rows;
- 12,950 Fossil loadouts possible, zero generated, 12,950 deferred, with 15
  layout primitives;
- ladder 31 goal subsets, two epochs, 343 candidates, 95 generated operators,
  2,251 generated rows, two joint attempts, and one joint success;
- exact evaluation matched with success probability one and zero off-policy
  mass;
- strategy 311 nodes/875 edges, 517,069 bytes, SHA-256
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  and
- report 1,229,244 bytes, SHA-256
  `e7f12df9a86d4c40de55df289cd6cf3d68857a6fb943b2d5dc60c68a7580cc36`.

### Genuinely full primitive control and replay

Correct immutable revision
`case-rev-49bd02aa57ce7d739d21410212da2c5b`, content SHA-256
`49bd02aa57ce7d739d21410212da2c5b434ad2ea0ac23ad9682114ca40a8a78a`,
ran naturally twice:

- job `job-a159b396-2dee-4ba0-8860-acdbc4750a74`, attempt
  `attempt-e47e3fb3-e16c-4c3a-9518-bd096bb3da96`; and
- replay job `job-975b4706-c43d-41b2-bb21-b7ac742a96b2`, attempt
  `attempt-3af51260-f01d-488e-a1af-20253ef24b68`.

Both own the same job/full/core identities:

- `79138e88cdf08b2c663debf2abe1d509973af619c78cfa7c727cb12582fa460e`;
- `90cbe6a803a140c2d80cca8b98f1ba4fefa9c88dc5a86463ae51c3d2106494fc`;
  and
- `16ca6af089fd958c097a3548432318bce005d81b6e6c1fc00b922d55d6288081`.

Both completed naturally as bounded feasible at
`requested_bounded_finish`, with lower `36.48853172876641`, upper
`1568614.0988592813`, independently matched exact evaluation, success
probability one, zero off-policy mass, and byte-identical 329-node/868-edge
strategies. The strategy is 515,563 bytes, SHA-256
`957d77256010ffaac583ae082b7c25cbeac2e51bf87daa77d6855cf008aa0db0`.

The first run recorded:

- 9,579 discovered, 3,817 expanded, and 5,762 frontier states;
- 21,540 rows, 1,345,828 reforge work, and 70,397 transitions;
- 12,950 Fossil loadouts possible, 13 generated, 12,937 deferred, and 28
  layout primitives;
- all ten ordinary automatic-candidate kinds in the normal full envelope, 96
  automatic operators, and 29 selected automatic rows;
- ladder 31 goal subsets, two epochs, 343 candidates, 95 generated operators,
  2,251 generated rows, two joint attempts, and one joint success; and
- two missing-frontier discoveries left open at the truthful wall stop.

The replay differed only in wall-bounded work: 9,575 discovered, 3,816
expanded, 5,759 frontier states, and 21,529 rows. CLI comparison reported all
20 core input components equal, equal terminal result, equal compiled strategy,
and equal exact evaluation. Whole ordinary-result identity truthfully differed
in the action ledger, scheduler, and public bound-provenance components because
those components record wall-time scheduling work. This plan does not require
fixed work identity.

The first and replay reports are respectively 1,226,915 and 1,226,945 bytes,
with SHA-256 identities
`5a1f898d1d7ea16aa94284d42e38cf53cc181833c32e09f643c7cc23d08d2889`
and
`de7c2ce57d953aa4e5f9e62ccdaa67ae194c7d0a715c127816add3b979427689`.

Full versus full-minus-Fossil comparison differs in exactly the four expected
core components: case-without-ID, effective disabled action families, goal,
and product action envelope. These are the added Fossil authority, not a
matched-request regression. The different full and minus strategies are both
independently evaluated and valid.

The Gate 0 stale-case cancellation remains the required genuine full setup
cancellation witness: MCP was not used, CLI cancellation removed the process
tree, released ownership, and returned leases/reserved bytes to zero.

### Investigation bundle

The authoritative full attempt exported bundle
`bundle-b6c2b8316ce7da9f46fdbcde` through the structured CLI. Its complete
idempotency-key replay returned the same bundle ID, path, byte count, artifact
IDs, and hashes. `investigation.json` is 2,788,322 bytes, SHA-256
`c87627f3565f9261885424888721fde251db22f9c0f2455282f5f89abc4d6f80`.
It contains five verified artifacts:

- ordinary finalization `artifact-92cd765003d430125742618041207d94`,
  1,601 bytes, SHA-256
  `a769bedfc8d54c0039e0a9fecabf9b382faa8efac33a284f7874acfda5c72f2d`;
- partial report `artifact-c89039c9928ad245a44860ee98a53d93`,
  1,226,941 bytes, SHA-256
  `f25439f5d16e8a1e0a7dabcd61afb4172a845180212b4906e616a5689638ae18`;
- final report `artifact-e2c62a86f67619a20936e19466049029`,
  1,226,915 bytes, SHA-256
  `5a1f898d1d7ea16aa94284d42e38cf53cc181833c32e09f643c7cc23d08d2889`;
- strategy `artifact-1839f668480b22936d9e6fb96b2501f1`, 515,563 bytes,
  SHA-256
  `957d77256010ffaac583ae082b7c25cbeac2e51bf87daa77d6855cf008aa0db0`;
  and
- worker log `artifact-0dbb766b6f631b3acffe666bec4e80bc`, 107
  bytes, SHA-256
  `40a5afc16be15b696a62ea7a23b804afa9c943a5562c33937fa2bfd479602cdc`.

The bundle preserves job/dispatch identity
`79138e88cdf08b2c663debf2abe1d509973af619c78cfa7c727cb12582fa460e`.
All artifact records have `integrity_status=verified` in the bundle.

Gate 3 passes current-behavior qualification. No Fossil setup or envelope
repair is indicated.

## 2026-08-29 — Gate 4 Carrier Ladder With Fossil Present

All controls used source commit `1029801df505299d7528754277529350686674d3`
and the executable/artifact/economy identities recorded at Gate 2.

### Same-side exact prefix

- frozen case `conquest-lamellar-allflame-clean-3-prefix-extended-product8`
  (no local revision), content SHA-256
  `4de8e1100eb2f541cf0654e3891577a725823ed418db1a1c2d9dfbfad160f758`;
- job `job-a0575bf1-c84f-4446-808b-f34bfcd430e7`, attempt
  `attempt-cd6412b7-8ca4-498a-9a90-96ae241cdb49`;
- job/full/core identities
  `3ec81004fe52db313deffc8795895ee479e7aaf8c0db11abb8c89406f3ae8e2f`,
  `8833b60ed2b81c0383cc3bb62c0c6d8bf8551e7f8770a4380314c3539187e666`,
  and
  `7cc860960ca45bc9e09e3a79bdf431879e7a951c4540c661e5f6b9ceddd263fe`;
- exact closure at lower=upper `1618.2138946963837`;
- 2,526 states, 33,725 rows, four generated and 12,946 deferred Fossil
  loadouts;
- ladder 32 goal subsets, four epochs, 2,502 candidates, 166 generated
  operators, and 10,920 generated rows;
- exact evaluation matched;
- strategy 1,603,016 bytes, SHA-256
  `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff`;
  and
- report 1,136,891 bytes, SHA-256
  `92849d25b3bf96a6b1aec0c04d14b0b76ee8d204366a8c24f0c135661a35008e`.

### Same-side exact suffix

- frozen case `conquest-lamellar-allflame-clean-3-suffix-product8` (no local
  revision), content SHA-256
  `714588ed7f2325a2204a7d6adf4d8a7935c271b8e9261a4bc77ebed11b168883`;
- job `job-50425f26-9173-4bca-bfff-0b71fa7d2e3e`, attempt
  `attempt-7000c433-1c12-4456-9818-3adb4ade5b98`;
- job/full/core identities
  `938e04579327a3fb9912a461132ad01c3002ddd7ad127facd8826985a808b505`,
  `1d0dad199a1643f995cacc3fd1bedfb45c92ae051ff4fc527dac50408acebe04`,
  and
  `f968ac603149c7e61918b27e6870e5e4ef93bb0f4614a43f9bb793a492640b49`;
- exact closure at lower=upper `1101.15648683309`;
- 3,882 states, 114,444 rows, 13 generated Fossil loadouts;
- ladder 32 goal subsets, four epochs, 3,858 candidates, 155 generated
  operators, and 13,902 generated rows;
- exact evaluation matched;
- strategy 173,592 bytes, SHA-256
  `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a`;
  and
- report 911,534 bytes, SHA-256
  `c7847b6863de17e957e0970b3717dd666da8d59df8a9be81085194daf8062573`.

### Partial four-to-five

- frozen case `conquest-lamellar-allflame-partial-4-to-5-product8` (no local
  revision), content SHA-256
  `02b3b2fa5b23b7d3f902ccdf1232969c8c590fae01960329d073df91aefe7b3d`;
- job `job-1a9ad1b8-aa0e-43c7-90ff-e7740a4ee725`, attempt
  `attempt-bab515dd-eff6-413e-aab7-69ef32551bfe`;
- job/full/core identities
  `47ab3e154825b2390f579eac0be7f57a28d150277db8b958f97f2233fa38db07`,
  `bf64646dd805cd3fcf07a1f9da2f23844dc9058e783289c76ac7bed9abcf1359`,
  and
  `df1a8c306e68d93a013bc0d5a45e6ef26296fe7dba7d635ca8327ce90b389866`;
- natural requested finish, lower `36.48853172876641`, upper
  `7896721.2542`;
- 11,766 discovered, 9,636 expanded, 2,130 frontier states, and 59,370 rows;
- 13 generated Fossil loadouts;
- ladder 63 goal subsets, three epochs, 9,633 candidates, 366 generated
  operators, 15,864 generated rows, two joint attempts, and zero successes;
- one missing-frontier discovery received one priority offer and one service
  completion, leaving zero open;
- exact evaluation matched;
- strategy 482,866 bytes, SHA-256
  `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e`;
  and
- report 1,606,742 bytes, SHA-256
  `de0c46591f8b880d382ea6aeab2095e3a797af31ae94f6d4ef8d9416ecb28c89`.

### Non-armour Spine Bow

The frozen 60-second solve / 90-second host-watchdog request reached a native
strategy but exhausted host finalization headroom. Its process was absent
after terminalization, no survivor remained, dispatcher ownership released,
and leases/reserved bytes returned to zero. This was not treated as a solver
failure or silently retried with a cap increase.

A CLI-derived immutable revision changed only requested bounded finish from 60
to 30 seconds: `case-rev-2bd429b8b6cf09ba1511f8b710728ae5`, content
SHA-256
`2bd429b8b6cf09ba1511f8b710728ae5e5dafea8907937e17972877cf954a02a`.
It ran as job `job-b2eaeb63-b7a1-4368-a3fb-268f03d29914`, attempt
`attempt-05da9085-ddaf-41af-a02d-98a499b349d8`.

- job/full/core identities
  `14039455bdff7521c9379587e07b889c00d131f3cf58e4e82b71db3f8888d111`,
  `986d33df2995f244ea6c04fdf75a711246784518f9215adca1ae3be5455f3cd4`,
  and
  `df6ec4f670accefdc7a515d57b32528690a37f5aa35d57544e5b9a2fe06bf798`;
- natural requested finish, lower `212.38564294509226`, upper
  `223349.0000393144`;
- 927 states, exact frontier zero, and 8,448 rows;
- 17 generated Fossil loadouts;
- ladder 16 goal subsets, one epoch, 922 candidates, 445 generated operators,
  and 2,527 generated rows;
- exact evaluation matched;
- strategy 399,287 bytes, SHA-256
  `b6b28648663b2207ff5ef56f275890ca5a007e9a51a7634912e7ee3f9ec3dcea`;
  and
- report 1,061,917 bytes, SHA-256
  `e3b5a3b105b67ffe0fe33f50e3230b195b3c69b1c9fe97fb0f4fc4406fd911d8`.

This proves finalization headroom, not base-specific setup or ladder behavior,
owned the first stop.

### PDR secondary resource control

- frozen case `conquest-lamellar-allflame-clean-4-pdr-product8` (no local
  revision), content SHA-256
  `bbd55011c734132ef9f211eff16e67a44cfc72988c65d23f2152acade5ec130f`;
- job `job-b237248f-557b-4d3f-bfa4-195a1ffd5636`, attempt
  `attempt-911c5dff-339b-4c6f-b0e1-524ecb397856`;
- job/full/core identities
  `c77f76cccf4dfb639c573f5de7b3949f3fd670ab76c51a6144425ba17920cc10`,
  `f63d30fa781189602c859576426731140cbe2ee1ab6b0e9ff563738fa360a4bf`,
  and
  `3ee17f0050ca5ca61fef2c578665dd657ca58df25797afae0f97fedc314e5c7f`;
- natural `refused_resource_cap`, lower `21.772459401271156`, upper
  `7866.432124027084`;
- 1,207 states, 61,476 rows, 4,508,232 reforge work, and 127,543 transitions;
- nine generated Fossil loadouts;
- ladder 65 goal subsets, five epochs, 7,212 candidates, 279 generated
  operators, 17,007 generated rows, six joint attempts, and one joint success;
- three missing-frontier discoveries received three priority offers and three
  service completions, leaving zero open;
- exact evaluation matched;
- strategy 411,156 bytes, SHA-256
  `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6`;
  and
- report 1,643,144 bytes, SHA-256
  `7f195b87f8e19343111b5467c6d6833e0b35d9817372e70f7be7e94ed4918dcc`.

PDR truthfully owns its resource stop; no cap increase is indicated.

Gate 4 conclusion: the current carrier ladder remains implemented and active
under the full primitive envelope. It creates epochs, candidates, generated
operators and rows, attempts joint assembly, services missing-frontier work,
publishes independently evaluated strategies, works on non-armour bases, and
retains exact same-side controls. No ordinary ladder lifecycle defect was
found, so no ladder behavior repair was retained.

## 2026-08-29 — Gate 5 Strategy Qualification And Owner Override

The structured CLI `evaluate-strategy` confirmed that the authoritative full
attempt owns a recorded native independent exact evaluation. A second public
native-binding compile/evaluation of the saved strategy SHA-256
`957d77256010ffaac583ae082b7c25cbeac2e51bf87daa77d6855cf008aa0db0`
also converged with success probability one, zero failure/stop/
action-not-applied/no-matching-edge/unresolved mass, complete pricing, expected
actions approximately `1192167.803886`, and expected cost approximately
`1568614.098859` chaos.

The Lab profile intentionally disables Simulator sampling and immutable case
localization pins `verification.runs=0`, so the established repository
supplemental native-verification path was used. Its evidence was kept distinct
from the persistent Lab authority:

1. The unchanged 60-second immutable case requested 10,000 trials but
   truthfully stopped at its case watchdog after 608 completed trials. It
   recorded 39 successes, 569 action-limit outcomes, zero off-policy failures,
   report SHA-256
   `e40bdf84cc488f045ed5a86a364f3df6b2d8424b9a0ee18636e835c10985bf97`,
   and a wall-bounded strategy SHA-256
   `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`.
   It is incomplete and is not counted as a pass.
2. CLI-derived immutable revision
   `case-rev-901bb3b34abb51737b04a8e5789e9976`, content SHA-256
   `901bb3b34abb51737b04a8e5789e997655c7e2b9c908ec9eb751adce0bab0ebe`,
   changed only watchdog headroom from 60 to 900 seconds. Its direct native
   rerun completed all 10,000 trials: 838 successes, 9,162 declared
   action-limit outcomes, zero stop/cost/step/no-edge/action-not-applied/
   missing-price/off-policy outcomes, and complete sampled action/material
   accounting. Report SHA-256 is
   `ccfc99375c638574342c67896e9415f199419a16dbb5c68fc49e49898d41f413`.
   However, wall-bounded solving published a different 339-node/929-edge
   strategy, SHA-256
   `4b0086ed7df3d7d9a20d6c038e6856b2f303f9ed2c4e7fe0bc882feb54c74054`.
   This is useful horizon/accounting evidence but is not attributed to the
   authoritative Lab strategy.
3. The authoritative saved 329-node/868-edge strategy was then compiled
   directly through the existing public native Python binding against the
   pinned economy plus the case's disclosed `base=1` override. Oliver stopped
   this run after the last observed progress sample at 7,040/10,000 and said
   that was enough sampling for now. Interrupting the process produced no
   terminal summary, so no success/failure/limit totals are invented for this
   partial run.

The high action-limit frequency is coherent with the independently evaluated
~1.19-million-action expectation under the unchanged 100,000-action-per-run
Simulator horizon. It is not evidence of off-policy routing, but it also is
not converted into a 10,000-run success claim.

Oliver explicitly changed the required Simulator count for this boundary and
waived further runs for now. Therefore Gate 5 has independent exact-evaluation
authority and substantial sampled runtime evidence, but no claim that the
authoritative saved strategy completed exactly 10,000 trials.

## 2026-08-29 — Gate 6 Deferred Acceptance And Current Stop

At the substantial milestone Oliver explicitly asked to skip the full
acceptance suite and defer it until later. No release WASM rebuild is required
by the retained test-only native change. The focused build/API/abstract/CLI
checks recorded at Gate 1 remain the only fresh test acceptance and are not
misrepresented as the complete repository pipeline.

No engine behavior defect was found and no runtime repair was retained. The
boundary remains active at an evidence-complete, acceptance-deferred stop
instead of being archived as fully accepted. Repository state at this stop:

- branch `main`;
- HEAD `1029801df505299d7528754277529350686674d3`;
- upstream `origin/main` at
  `e7b81a0143ed1b348ed9a80b28be44916cfd7a9d`;
- one local commit ahead, no push; and
- protected untracked `0` remains the sole worktree entry and was not read,
  modified, staged, or cleaned.

Final bounded cleanup inspection reported dispatcher ownership released, zero
reserved leases, zero reserved host bytes, an unpaused queue, and zero live
`poecraft_solver_benchmark` processes. `git diff --check` passed. No broader
test or documentation-link suite was run after Oliver deferred final
acceptance.

The evidence selects one useful successor question: the full five-goal run
builds 31 carrier goal subsets, two epochs, 343 candidates, 95 generated
operators, 2,251 generated rows, and one successful joint-policy candidate,
yet its published strategy still has approximately 1.19 million expected
actions and no occupied Fossil route in exact consumption. A future selected
boundary should first attribute this gap between qualified ladder construction
and practical selected-policy quality. It should retain a repair only if an
already-authorized executable ladder candidate is lost, under-serviced,
mispriced, or rejected by existing selection/publication authority. It should
not resume fragment composition, add another planner, weaken exactness, or
raise caps.

After the local documentation checkpoint, the `origin/main` tracking ref was
observed to have advanced externally from
`e7b81a0143ed1b348ed9a80b28be44916cfd7a9d` to the runtime/source checkpoint
`1029801df505299d7528754277529350686674d3`. Codex did not push. The branch is
therefore one documentation-only commit ahead of the newly observed upstream.
