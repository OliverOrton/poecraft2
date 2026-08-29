# Carrier-Ladder Exact Boundary Contract v1 Execution Log

Parent: [Plan](plan.md)

## Selection And Startup — 2026-08-29

- Oliver selected the proposed boundary by asking Codex to keep working after
  the preceding diagnosis-only closeout.
- Branch: `main`.
- Starting `HEAD`: `dc14aab6c9f3bfa933573451c672c52ba8df94a2`
  (`Close continuation boundary diagnosis-only`).
- Upstream relation: `main...origin/main` is ahead eight, behind zero. No push
  is authorized.
- Worktree exception: the sole dirty path is the preserved untracked file
  `0`. It was not read or altered and remains excluded from every content,
  patch, stage, and commit target. Any other unexpected dirty path is a hard
  stop.
- Governing documents, the change-impact map, and the prior diagnosis were
  reread before mutation.
- Primary immutable witness:
  `case-rev-cdfb71c9e2403db6b20d067ea8b42e91` / SHA-256
  `cdfb71c9e2403db6b20d067ea8b42e91839c5c9862c9fe4eebe213c06d3df07e`.

## Gate 0 — Activation And Current-Source Design Audit

Passed source audit before executable mutation.

- The failed joint candidate, temporary selected rows/values, deterministic
  observed-choice routing, named missing parent, and independently certified
  frontier coexist inside `try_install_reachable_incumbent()` immediately
  before its restore guard returns ordinary solve state to the caller.
- Capture will close a private copy of the selected prefix over every routed
  successor. States without a completed selected row become explicit typed
  stops: the first existing ladder-owned missing parent is the requested
  entry; already certified frontier operators are rejoin dispositions; every
  other missing parent is an explicit refusal. Capture does not call the
  renewal installer or mutate ordinary ownership.
- The retained prefix is a separate `SolveWork::Impl` observation object. It
  is not an `IncumbentPortfolio` member and is excluded from Bellman,
  scheduler, lower-bound, compilation, and publication inputs.
- `ProductionPolicyOracle` already reconstructs its strict context from the
  authored exact start, imports the filtered planner vocabulary, propagates
  observation requirements, maps collision-checked strict carriers back to
  coarse parents, and builds primitive/fixed-option rows through native
  kernels. The smallest safe extension is a selected-row-only cooperative
  traversal with named observation stops; its existing quotient method is not
  used unchanged because its fallback may materialize alternative actions.
- Boundary observation must distinguish `goal` from `terminal`: an exact
  carrier at a named stop is terminal for traversal but is not successful.
  Forced semantic identity must also take precedence over the oracle's normal
  goal-terminal collapse so reached entry members remain distinct.
- No mechanic ruling, representative `CalcContext::materialize()` from the
  coarse parent, second transition implementation, or publication shortcut is
  required. Gate 0 therefore passes.

## Gates 1–3 — Retained Prefix, Strict Replay, And Typed Exits

Implemented the benchmark-private `carrier_ladder_exact_boundary_v1` contract
with `off`, `record`, and `recover` modes. Product/default requests still omit
the setting. The setting is private to native benchmark/Lab execution and did
not add a C ABI, WASM binding, strategy operation, mechanic, or product
surface.

- Capture happens inside failed selected-policy finalization before ordinary
  incumbent restore. It copies selected rows and values, closes the complete
  routed prefix, serializes complete stable item keys, and owns a separate
  capped observation allocation. It is never inserted into the incumbent
  portfolio, graph scheduler, Bellman values, proof lower, compilation input,
  or publication pipeline.
- Typed stops are `goal`, same-identity independently executable frontier, or
  unresolved. Goal and non-goal terminal semantics remain separate. A
  frontier is usable only when its incumbent is independently evaluated,
  proper, executable, and exact-routing compatible.
- `ExactBoundaryRecoveryWork` reuses the production strict oracle and native
  primitive/fixed-option kernels, follows only the captured selected row, and
  never materializes alternatives or a representative coarse state. It is
  cooperative, destructible for cancellation, and independently capped by
  prefix states, exact states, rows, transitions, work, owned bytes, wall
  time, and samples.
- Recovery recomputes the captured graph prefix before traversal. Missing
  selected rows and unsupported semantics are explicit refusals; they are not
  absorbing success. Exact members remain distinct by collision-checked hard
  item key even when their coarse parent is equal.
- Ordinary result finalization is frozen before the private diagnostic. The
  private allocation and wall clock are excluded from ordinary resource and
  requested-finish accounting. For deterministic qualification only, an
  optional `ordinary_finish_state_action_rows` fence ends the ordinary solve
  at an exact completed-row count; zero preserves the existing wall-clock
  behavior.

Focused native controls prove two exact carriers at one coarse parent remain
distinct, goal differs from a non-goal boundary terminal, an improper loop is
rejected, and an invalid successor refuses. The final focused refinement
executable reported 375 checks and zero failures. The corpus-runner plus CLI
workflow selection reported 21 passed tests.

Implementation defects found and repaired before qualification were:

1. unresolved stops were initially treated as absorbing;
2. exact-state counts were initially cleared before serialization;
3. graph staleness checked a retained hash instead of recomputing the prefix;
4. observation wall time leaked into ordinary requested-finish timing;
5. a wall-seconds finish still permitted scheduling variance after clock
   isolation, requiring the benchmark-private row fence; and
6. a long derived Windows strategy path exposed a harness write failure. The
   cap case was re-derived directly from the short parent and completed with
   native exit zero.

No in-scope defect remains in the retained source.

## Revision Ledger

All revisions below were created through the repository JSON CLI and are
immutable. Frozen controls have no local revision ID.

| Role | Revision | Content SHA-256 |
| --- | --- | --- |
| initial off | `case-rev-040e924884aaf5121a6b8b328e040890` | `040e924884aaf5121a6b8b328e040890a66e6f22c227e75c73c9e2670378c707` |
| initial record | `case-rev-1339a8b44a3c857a163b5a9f1d06bff8` | `1339a8b44a3c857a163b5a9f1d06bff81a1aea71a88ebf67256dc7d59487b004` |
| initial recover | `case-rev-0b436d7de22c30f456ce6fc7137eeb01` | `0b436d7de22c30f456ce6fc7137eeb011db2c039d08839abacbded6d73edace5` |
| 360-second off | `case-rev-b96fc7a5681310ef56ce55634bc56733` | `b96fc7a5681310ef56ce55634bc5673346e62dde0de54aab26e3cac95035ffa1` |
| 360-second record | `case-rev-f2653d1ea67f959eeb0d1498ddab47fb` | `f2653d1ea67f959eeb0d1498ddab47fbd6ea09f5995263d6e453cd5e9a7c3b44` |
| 360-second recover | `case-rev-ec4df69bf0444a53d3b0a1d137320e15` | `ec4df69bf0444a53d3b0a1d137320e15b16285f7f333b8513e46e47aca0ecfbc` |
| final deterministic off | `case-rev-e1a5ac7485220794c23791874fd468b7` | `e1a5ac7485220794c23791874fd468b79524e84e8734cc9eb051fe4df1e111d5` |
| final deterministic record | `case-rev-181df5e9f9b7fa36ebb4f75dd6d85eb1` | `181df5e9f9b7fa36ebb4f75dd6d85eb14a138819439fa50244f715b1747f16dd` |
| final deterministic recover | `case-rev-03e9346553b8b7367f6397406fc733ae` | `03e9346553b8b7367f6397406fc733aea87f09a8274aa69b2c85c0289c944f73` |
| superseded long-path cap | `case-rev-36c1144f78e1ce435dc02acfe1e0c2d2` | `36c1144f78e1ce435dc02acfe1e0c2d2884e6bd74aa98989bd408817695e48f1` |
| final short-path cap | `case-rev-1277deae8d655a691c46ba49557b8f85` | `1277deae8d655a691c46ba49557b8f85f7fb43b92ab4fe770fa9468e8b3ecd32` |

The final derivation identities are off
`8c63fcf5f58055d32fb3d29a08b96a7a38ee8071bc881ca62f66391040df2a19`,
record
`f1319d126b87596ca935c6edfbda392b660bafed7c900cd88d4ccfe1045b2c79`,
recover
`8919495576ed955357450ea33419f5859e58e9ffe103b4ea09418cf1e6607e06`,
and short cap
`e03cabf05aa026634bd52fe846cfadb148b95f1564eac3c07e07ba97915fe8d4`.
All final deterministic cases use `ordinary_finish_state_action_rows=26119`,
360-second native/host watchdogs, and a 300-second private maximum.

## Exploratory Job And Attempt Ledger

These CLI jobs were necessary to find and remove the defects above. They are
superseded by the final `f8228a13...` matrix and confer no qualification.
Jobs with the same identity were deliberate fresh-replicate runs. The first
three jobs were canceled before dispatch and therefore have no revision,
attempt, executable, core, or full-request identity.

| Executable SHA-256 | Revision | Job | Attempt | Terminal fact |
| --- | --- | --- | --- | --- |
| n/a | n/a | `job-f806e083-e4db-4a8a-9c8a-19a0d3261f79` | n/a | canceled before dispatch; identity `3d31f593360f2f4fa21493899e6abaa5e63d7aada0b0354ecdd5f449bb438ba9` |
| n/a | n/a | `job-8a0ee49c-da75-43bd-b4f3-7dd94dfc27f5` | n/a | canceled before dispatch; identity `8c4b4761ba183a452a415c75e2b20265824365cf18484f825258305dd74989d0` |
| n/a | n/a | `job-0f6db631-2096-4fa8-8110-76412ef8d005` | n/a | canceled before dispatch; identity `079dc62cfee2da2e155727e03e9ac74213603d5cb53fe07694a7c2f342312842` |
| `0aa70a9c67ea46c28d81c239d5f69be88d4f1dbefed71cb99a2605c64545d2d6` | off | `job-afa10f1a-475d-4bf0-9eac-2fca6b845780` | `attempt-73bdfe14-6b25-4913-a2d2-d573c1c7899c` | exit 0; core `0aeda096e73c6b4793c6a1024b850e01b5400d6e0c58a5e13f25de1604d66f88`; full `cf5500e827d1cfdeefaba7205b078e3f4e34997932efaa4b12526ea94c4a4665` |
| same | record | `job-d70a5031-7878-4710-8a39-332677f8679e` | `attempt-b2df643a-7431-440c-9b43-d69f980395cb` | exit 0; same core; full `f15df783778a3052c6b7f4cee836ed84f4575472535c6fc92c10d62143e8ea13` |
| same | recover | `job-40a13942-edbd-46a8-b295-fcfaacc85479` | `attempt-a93e79b8-88d0-48d7-85a2-04c33cb492f1` | exit 0; graph-identity refusal; full `2d04e2c6a57c8586055cd5fb62611d49561e01210218787e012901ee923d4775` |
| `17af7da4eaa85d8b1ac4ba84c7775565e563d724a385dd4a5dbf3a54f7c14980` | off | `job-856e69cc-d742-4072-8cb2-050bb131b247` | `attempt-79402da3-69aa-42e3-82ed-4796743deee7` | exit 0; core `4a43292badbdb2c0c776bde83eac4f1cf3fe56e6ed34998bdb6b30ed53d9be4a`; full `95384937c302371e309c4050d02afa1d59b8ef5233d20f3bb2fb32f2bd5e860d` |
| same | record | `job-9f80dd4f-d030-4321-b664-f8e4f02abebd` | `attempt-1b1c1985-5fed-4c10-a3c4-6c686eccc0a1` | exit 0; same core; full `730b1f6f9f34a6f2e439181d94e42fb6638232de8821ce1bdb8a1b0ec9b2b841` |
| same | recover | `job-4510820a-c06e-4137-8a6a-9eb86f408ec1` | `attempt-2f9ce53a-6ee8-444b-bfe1-29111cc33d75` | watchdog exit 2; same core; full `60491a168d02e70c69268070f32c68893c79cb250c0cc55570a15e97374b50f4` |
| same | off-360 | `job-8992c540-c3e0-43f1-86f5-4468fe112a2f` | `attempt-f9c0b4ef-7f3a-471e-8133-34734b86baf2` | exit 0; core `60ffca8e4d1dced34a0ec8950d00897a994a03620621bb99a2f44876ef381c89`; full `d5e0aa16e317b9fce170ecf5befe7c1fc3a9751f5bc2dd24e24c8dd7c0daad53` |
| same | record-360 | `job-0690cc00-ac5c-46fb-a447-b01ba81553f9` | `attempt-bd1019d1-4b81-41dd-b2a0-4a5b79bc1ae3` | exit 0; same core; full `3075629e3d4bc1706ca0fff8b6cd715dc8dda50ee0278beb5937f6f149b661b3` |
| same | recover-360 | `job-81e2a480-2bdf-4d59-96f7-6f835464d81b` | `attempt-958df8d3-0bbe-48fb-8b10-a5f99b705cdc` | exact-transition cap; exit 0; same core; full `486f6eb6720faed9b91ea12ebb4ae9a1d2909381d313c7d6b038a4e83fb9fe03` |
| `ed3655333e40da273f66026e009d36b5161bb80da844dac1a3ec394d68c1ee0b` | off-360 | `job-37b93c4d-ab5b-4e6c-8da0-88c35df95f3f` | `attempt-9c7db674-23be-41ed-a6fd-254bc21da758` | exit 0; core `23214073765c80d84cde0b04579a8fb09a7a19ebb99ffc89d8e5ae418cc1f1c6`; full `6a296539d7facd9ed8ced794d1c2d2563eb0a429272ba96c8c5afa069aecc38b` |
| same | record-360 | `job-c08ec8ce-1896-495d-b4ac-e7b2c9fdaae7` | `attempt-e7677be5-8ffb-42ad-abf4-a5d731b3cf52` | exit 0; same core; full `f67da6d19b1f85b367089454f762df8484d1e68b50eb0f7a4e76e7a864f2e557` |
| same | recover-360 | `job-3664e789-8dde-4b4b-8c03-acb88a2ae686` | `attempt-860de569-7727-48f5-936a-7c9bcfb06e04` | exact-transition cap; exit 0; same core; full `aa346303714da27e18a874dd717daf2f42c3c0bfee6e1d330e2bc249aefe88e9` |
| same | off repeat | `job-5db3c3e3-e5ea-465b-9fc4-67e9f2d61395` | `attempt-4ae1729b-260b-4e2a-825a-5130498fb0ae` | exit 0; identities equal prior off |
| same | record repeat | `job-b0d87754-bd20-49c1-84ca-504d50461890` | `attempt-5cced4a0-13be-4fe3-9063-94ad85b0e85c` | exit 0; identities equal prior record |
| same | off repeat 2 | `job-8f1021fa-95f1-4fed-b0e3-9877257977f9` | `attempt-af8e244f-8512-4cc3-bc97-37a8e4d0a510` | exit 0; identities equal prior off |
| `07143a33a22af781d0476706b32a4aaa8d0c00a7112c66b7af9ffa7c9caa5b42` | record | `job-b6e0545f-4913-497f-8836-c6cdb9670fc3` | `attempt-24176e36-bd6d-4718-ad0b-5218637256d4` | exit 0; core `31ea55d9cf56de318764b8fad98393b2415222b8d1c20ac878c3eb06f3362060`; full `daef0601a83f82a9522b33339f60e524821e3279d4564a7c93f6423131d32ce4` |
| same | recover | `job-dc7a1e76-e9c5-45ac-a6c4-063b6902c87d` | `attempt-450fd4f0-e802-4ecc-8d0a-85ac92af5d9d` | exit 0; same core; full `15f4bafb696c980293b79332d6654e01f3cb71acef7c6586bdde9e7015e626b0` |
| same | off | `job-a2cb11a6-87f2-4832-8792-fd19a0821c51` | `attempt-89110db3-51df-4ddf-a1ce-f80d76a7699b` | exit 0; same core; full `d28acc3602681d330f7ea7c8b511aa877ea3ec57606205e760f3ad5e40dc30dd` |
| same | record repeat | `job-676257c1-c596-477a-96f5-6dc3a10f222a` | `attempt-c2987480-6eb2-41d0-ad2f-7867e9abf638` | exit 0; identities equal prior record |
| same | off repeat | `job-98d84a06-7d8d-4e06-aaec-84f2e8b77c94` | `attempt-57955831-763f-4f26-9510-05d915674bb8` | exit 0; identities equal prior off |
| same | record repeat 2 | `job-b89a7021-9e49-4385-86ff-b4cbb716defd` | `attempt-3eebcc7e-403e-4dd2-8a51-2daa4afa5188` | exit 0; identities equal prior record |
| same | recover repeat | `job-aa20807a-8c6c-4bed-8dc7-e840a87927ca` | `attempt-ee231b3d-44b7-4e3c-a360-09601b47efe9` | exit 0; identities equal prior recover |
| `09e6a9c5d1c857caf356bc7151a33772ce6c5ef85cc9236f4991d33885d1b48f` | off | `job-9849e8b5-2452-44d2-bd14-e247d635d48b` | `attempt-22ad5f7f-7639-4252-a35f-5a34ab09580b` | Windows partial-file replace exit 2; core `4be20da38e9bc0a673802d3fd14c54809a9a6c1c33b19b17d23a5679e0a80342`; full `4f15221f0882558fefc8ad98ef7762f28ee689b8eaf1ab2ac17907e4e906e78b` |
| same | off retry | `job-8fdff65e-aa42-46b6-bc17-5086146ceb95` | `attempt-4bced746-8a1d-44d1-bf04-5b452a7881a3` | exit 0; same identities |
| same | record | `job-50845b30-9464-4cd2-85a8-220664c2a963` | `attempt-b142664b-1285-4080-aaf0-a4251fa70805` | exit 0; same core; full `62148ccd08677801a09e85c2439307cabd490f5ac532a1e69456179537333951` |
| same | recover | `job-ebda2487-e7ae-4022-8249-c091be72d152` | `attempt-d93a6976-146d-4646-b9f0-57901c273a87` | exit 0; same core; full `4f0c237b3345cb6e86140c0bd9fd2767bdb7c831c8dfeef7b57728d171a02821` |
| same | record repeat | `job-222fb88a-dfa4-4291-8c24-9e710f8dbe45` | `attempt-86dc358b-0196-4373-b7b4-53746548dba9` | exit 0; identities equal prior record |
| same | off repeat | `job-c68e756f-0a0a-4491-928f-90b626cdef01` | `attempt-ceaa392f-7baf-4e48-a8ca-9d71acb1fd53` | exit 0; identities equal prior off |
| same | record repeat 2 | `job-7d6f751f-ec41-4e7e-922a-3c73dc1eeddc` | `attempt-b9f7b345-2a0f-4521-b8d6-5f595d8622f2` | exit 0; identities equal prior record |

## Gate 4 — Behavior-Neutral Qualification

Final native benchmark executable SHA-256:
`f8228a13f25f2d95ca976ad331ce576d4fb79225e8d7a23172883ed70f0a044d`.

| Mode | Job | Attempt | Job identity | Full request identity |
| --- | --- | --- | --- | --- |
| off | `job-ff8b4a82-97eb-417e-ad85-1317b5c30ce3` | `attempt-2029dc76-d98b-408f-8962-c81f30070668` | `50f74627b7ae44a9a1e27fc55c2e89ef8738fb3de5b9920cc86b2715619f6065` | `12ea66971436219f63ff0fd60b461864c55a846df93b48f7eda5469b314580e5` |
| record | `job-85c4544b-7365-4d08-8417-8165c4bcba35` | `attempt-eae7fe01-32e3-4b64-97ea-9cd6d8998b14` | `55732aa34659d3f44bfafd2d5f8888dd64bdf540330de3d92593ce51b3065ff9` | `7f0489d61b512d97762b5fd06f8de9c65f225a9e4b85a0bf4e25b60e1d6389dd` |
| recover | `job-fc989089-b2a1-40a4-939f-abc1d261adb6` | `attempt-0b9e4f8f-76de-446f-8cea-41c77f7d8a18` | `552503f8865e213f4c7eb833b120d087e43e3acac6470795d17802b43ef45c0c` | `c1bb266946df8d0bb1d65402ab98016f30818ed3fffebed4d12102b973722976` |
| recover replay | `job-6d9a2155-f68c-4ea5-9379-6997f8c4082a` | `attempt-b66dd540-18e5-4b92-927f-9b31221856e2` | `552503f8865e213f4c7eb833b120d087e43e3acac6470795d17802b43ef45c0c` | `c1bb266946df8d0bb1d65402ab98016f30818ed3fffebed4d12102b973722976` |

All four core solve identities equal
`a80c6c1ebf3b63adc820b2ccbb32c6281d8ccccdbbeb4f11a3c92afa17ee87b9`.
All 20 enumerated core component identities compare equal. All nine ordinary
components compare equal: action-envelope ledger, cap/resource
classification, compiled strategy, core graph/scheduler, exact evaluation,
incumbent upper, ordinary inputs, lower provenance, and status/termination.
The ordinary result identity is
`fba6fb26e5fa194b1e12f711082ab290f8e0c3d3d18667ea079011ecc5ed7d6b`.
Full request identities are unequal between off/record/recover as required.

The bit-identical ordinary result discovered 10,065 states, expanded 4,841,
left 5,224 frontier states, and completed 26,119 rows. It retained lower
`36.48853172876641`, independently evaluated upper
`1550334.436668944`, strategy SHA-256
`bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`,
exact-evaluation status `matched`, success probability one, and zero
off-policy mass.

The record/recover capture is stable:

- selected-prefix boundary state: `1780`;
- selection identity: `817a9a5c93d6b30b`;
- prefix identity: `94dcb0cfc5162f29`;
- exit-contract identity: `c621919aa67c36dc`;
- goal/economy/scope/vocabulary/artifact identities:
  `746d448eabb2df71` / `ad2373d65bde3138` /
  `47c8a8f4e3ff4198` / `b5627b715ac1ba4e` /
  `f936a0dc37501788`;
- graph/graph-prefix identities: `f9b9362c26a7f92f` /
  `9fec2e08127e8e08`;
- private executable/source identities: `72b853b22e575c7e` /
  `a15a48a9fa86dc86`; and
- 469 selected stops, one goal stop, zero certified-frontier stops, and 485
  unresolved stops.

Recovery is deterministically `invalid_prefix` with first refusal
`non-goal coarse state 213 has no selected policy action`. It consumed 214
work items, peaked at 1,688,392 private owned bytes, and reported 17 ms then
19 ms private wall time. Because unsupported support is reached before the
named boundary, absorption and complete support are false, exact
states/rows/transitions and member count are zero, and member identity is the
zero identity. This is the truthful diagnosis-only outcome; no exact entry or
usable continuation claim is retained.

### Real Cap And Cancellation

The short-path private cap control used revision
`case-rev-1277deae8d655a691c46ba49557b8f85`, job
`job-56034e2c-1c5b-4e80-8751-bad0bbe3b411`, and attempt
`attempt-e63576d1-75ee-4a8f-9318-4aa11b5c6c86`. Job identity is
`48b1796e646f794aeca8c85032672791ee6d02e389eefd59684d6daeee9ed2a3`;
full request is
`89ceb28ce5c184ca7243459d6937c3c375a4fe426c16b6bfae31906106076ab5`;
core remains `a80c6c1e...`. Native exit zero and no survivor were verified. The
private diagnostic is `refused_resource_cap` / `max_prefix_states` at the
configured cap of one, before retaining a partial prefix.

The first derived cap revision/job/attempt were
`case-rev-36c1144f78e1ce435dc02acfe1e0c2d2` /
`job-6ceac1ca-8e95-4fae-897b-f720fb2b54b1` /
`attempt-bf3457f0-63b2-470d-96d9-973ac9fcef9a`. It produced the same truthful
private refusal, but native exit 2 occurred afterward because the generated
strategy filename exceeded the Windows path limit. It is retained only as a
harness-path diagnostic and was replaced by the short-path control.

The genuine long-running cancellation witness used executable
`ed3655333e40da273f66026e009d36b5161bb80da844dac1a3ec394d68c1ee0b`,
revision `case-rev-ec4df69bf0444a53d3b0a1d137320e15`, job
`job-17b0c5ca-1e4d-4a58-b27d-76354332a7e8`, and attempt
`attempt-54290776-53dd-4c12-bc42-26c0743fdd0e`. It was genuinely monitored
through 19,474 ms into native exact evaluation, then canceled through the CLI.
Terminal status was `canceled`, exit 1, mode
`graceful_then_process_tree_termination`, no process-tree survivor, released
lease/reservation, and zero retained host memory. Measured acknowledgement was
413.8582 ms versus the configured 250 ms; the evidence is accepted as real
tree-removal/release qualification without falsely claiming the target was
met.

## Gate 5 — Fresh CLI Controls And Artifacts

All final controls ran separately through the repository CLI on executable
`f8228a13...`:

| Case | Job / attempt | Core / full / job identity | Result |
| --- | --- | --- | --- |
| partial four-to-five `conquest-lamellar-allflame-partial-4-to-5-product8` | `job-446f3870-3b6a-4d0c-a598-058b7ae8211a` / `attempt-d359d9d9-de15-46eb-b18a-994c4ab43347` | `3c9ca8ecffe3ebd1ef97d1c3957d893283372c2caed2f98aadd65f87acc0326f` / `0043e5f6886047a728348e18681530dcc2e496eb8c028daa1fa9fae6c3b8f7c0` / `fa40d9c12f01dbe2506a96421c24e7c0ce4d0b5ab5018ef0b8bd48794eaa6895` | bounded feasible/requested finish; lower `36.48853172876641`; evaluated upper `7896721.254200992`; exact matched; strategy `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e` |
| PDR `conquest-lamellar-allflame-clean-4-pdr-product8` | `job-9fb85839-5075-4d9f-8a29-069960e314a5` / `attempt-2dee4a23-d743-443a-ac45-1457494ecee7` | `87c8d2b1afbfec046049747039113135f1ea751b8876f42ee264164555cf9137` / `48577603dc5438d2a43ac409312d59179e1ea3a0d484fcfe9976aedc9a51b012` / `35ac86b63e73f6ee1f52b7794c05bd6b73b06f8570eabab4170184a4c588d309` | bounded feasible, truthful `refused_resource_cap`; lower `21.772459401271156`; evaluated upper `7866.432124027084`; exact matched; strategy `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6` |
| same-side prefix `conquest-lamellar-allflame-clean-3-prefix-extended-product8` | `job-d18d9fec-0480-4ef5-a64a-cefff646e3b3` / `attempt-264b8a08-5f4a-4f9d-b622-36acd4f5bb77` | `f6dcd5eaeb40d5dfe8743e5980779707a211c8835d2c30525366e71bc5ece5dc` / `74f1cf273cac1ca4af1c91f3ca1bc524df733c21e2d57554082c08e6bf5eed1c` / `dd0c5b6ea510da361081feddeee4f35a7e548f32be5f8eeab4706936574ec7d7` | `exact_closed`; value `1618.2138946963837`; strategy `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff` |
| same-side suffix `conquest-lamellar-allflame-clean-3-suffix-product8` | `job-e7663461-5cbd-43de-ac32-7a5d0e4d66f7` / `attempt-7027a3fa-46d9-4d82-8f9e-31698db82fad` | `1c135b04364259f00f149186a160ff5d176f4374be806ad07680edf15640294d` / `a79f348deaa31488431380a97d4fd8698b999b92508b3fe7fe3489d39ceaebfb` / `f30af88b22c2a1da11c59678fb66591b32d5907f891e73ed1355f8d81b8ce3a6` | `exact_closed`; value `1101.15648683309`; strategy `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a` |

### Final Artifact Ledger

Each tuple is `kind: artifact ID / content SHA-256`. Sizes and paths remain in
the immutable Lab catalog and investigation bundle.

- off `attempt-2029...`: ordinary `artifact-2c0e9c80de72b235f57c21e1a797b099` /
  `7657c9317d8fecc1433bae2b3745e577e61ce86825e736082b157df04fb66503`;
  partial `artifact-37b26791164a7eeb5c32a24aadc68e39` /
  `99f76ebbbeb4bf3833f6828c2a4ed0b252d6edb6e6040ada58804dfc619a042a`;
  report `artifact-31caae95d19cd5f61c1a5083c748fe21` /
  `93ea61b7f004bc7af240b061c0edebfc63ca2d0002c97ec6bb94d8b682a1740f`;
  strategy `artifact-7797334043cb0cb4152a83d6e800550c` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  log `artifact-53dbce50a0923abf0de33634346c0e0b` /
  `7a34c38a78b434635bb434d2f0371d9cde1c66ebb413ccd294a233def98c1480`.
- record `attempt-eae7...`: ordinary `artifact-ca5ac34aef5c3b0a456ae4fb41e8aaed` /
  `eddba883caafb0dfa4d015872cc78ff9d668f9d563a080e8915bbad3a7593ebe`;
  partial `artifact-d6320ab849c4991c5dbec352841e0f26` /
  `fab0f2acdddf02ac763e5c078e3a41c8a8bd369d0ba7805335fb73a0c9065f05`;
  report `artifact-e7ff78bc041eb7aaf2e833131a9dff00` /
  `2e9142f822e082f6bca91ee7f9e23fd55f0e74c36347706d2fc40c0ecc04ddee`;
  strategy `artifact-5c0724aacf5a5c39e3ca638344d36db2` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  log `artifact-7ae63b2ee51a272b605d28fd55abf149` /
  `37300b7278a47eb6362cedc68a28694acde4de178daf8adb544ccc0b573c3fdf`.
- recover `attempt-0b9e...`: ordinary `artifact-a145b71415db265907bf1dab9da25817` /
  `2252b2b8ea7c62d23a6dd2a8390786c08876925ca7f5f6a2e49e208bea713fc5`;
  partial `artifact-5fcff9bbaaa5a064e91240001ac74e61` /
  `a7225ba2112267039bc096de58f3eca82d0821ba13bf0d833880ae7dae2cacfb`;
  report `artifact-1a20d7ef393067b4d05deb4477a3f4ae` /
  `ef7d7a30b25fc08e516eb0d42e0615f00c0086792d6dc0e9c9687307dfa07d73`;
  strategy `artifact-67750254e3085f58878e4d454d631a58` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  log `artifact-209142855774f7f4ff73740d0b26db4b` /
  `bc717eb7f457b2c999c9159050b8f0fdf6f2b4323b94ab66a91b46edb6f6e671`.
- recovery replay `attempt-b66d...`: ordinary
  `artifact-b7219d6b7d135e9c31a8ac476777573d` /
  `4786e1dcff347125dfebfada8d7bd6017c5915298fc033096f209cc2a2dda0aa`;
  partial `artifact-adbcd5edc98f4fe77a959b6bfa618ca3` /
  `b0a4491ef3a6c99696dfde028a7f7d0dac1363a2940e50e4fb657c67e74480b9`;
  report `artifact-d76b707de70c097aae64956a787cd28f` /
  `d36821238cf7198cd44d586f56211da3286c052504dc3ac8045a520c7f5be3af`;
  strategy `artifact-1b3d940bb3c2c00575a4a43fc1e9e960` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  log `artifact-449f8b900743d5c6b5a141a6e8925c11` /
  `6d9b275017e58f9812e23f264e2167b4291b388f2897a480745c330c6556dc16`.
- partial control: ordinary `artifact-5294b39b03bd3208e122263a1fc98391` /
  `a8e4f99fc4784bf368dfd844ea5ba25908a6836bd11a6511ceacd52c81758014`;
  partial `artifact-eb838bd14dd89ac7a9b02714f4e6a831` /
  `258c70e00ac3f82fea9cf62114d20219478f13eb011845924696c621808f87a4`;
  report `artifact-86d3f481d09e2504d82e06e4ad11aed2` /
  `15ae092529ec4b068e18a0c42f17b63d02488e72bf69b5fe6099b5004267e2ac`;
  strategy `artifact-39a4d5f64ae8e20ed5dbc9dc1feefa82` /
  `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e`;
  log `artifact-148c2c1c22f6af066d9a3a0f1bd7ee93` /
  `2b38b912a535d5818d85cc7d019f1ce0332fba78313fb003c4b872b5c343e201`.
- PDR control: ordinary `artifact-a4ec494aefb1cba17cd06e0dc4b01a35` /
  `5cb32b052a08fa32882b8a030e857d0e7319d38ef743d9712124b8a6b67714db`;
  partial `artifact-551b1784064d5b3f58a3ddc13d1834d1` /
  `e3930b7bbcf3f83043bb2665a33576c4f7d119cd180154d2d95c351c58d71e6f`;
  report `artifact-b631f61a5303c184dcd5946ca11be8cf` /
  `7a0434962e88e5c7b6cba17d138a9cb6cf87461b5cd97daecbff09f77234dd87`;
  strategy `artifact-4e334520d50c6c22cf3d50359883d825` /
  `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6`;
  log `artifact-0d74f3224feb5df232b7e951323ecf8d` /
  `3c174c5acf80b0e3647c191a1ac52ab92534dff21b6bba9fe6b6f25291c3b60c`.
- prefix control: ordinary `artifact-e0ed3cd394ecde13dc2bc51a4720e166` /
  `d8a02d4ce3fc6c92c6e3bf1e03af9e6a78bb87f12dfadc589bba30c987336dd7`;
  partial `artifact-c57187dc023f0ae64a493523e082ea6f` /
  `1f1013fcff8360ad080e73258855e88ccbf1e42a8295e81ea920f7c2f5115f0c`;
  report `artifact-b98e697c8203d005df94b5944ad5f19e` /
  `4386471149f8247a54701a07329d62fc34c4f9e14e339cd9b30b1a6f3c3b7521`;
  strategy `artifact-d78c6a7f525a763d3ade0037d5e505c4` /
  `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff`;
  log `artifact-4bd4309856064e5eb708e862f44c05f8` /
  `bddd99283e6faa597ba4cd52921b9abfff4d2cbc309483f0e7ea30d1eb55a618`.
- suffix control: ordinary `artifact-71f96e4bb1aa30fc1484baa9759c3c6e` /
  `b2bb2a89deb13a709afcefb1d8909c1ba8ca1ab46f69574e4977ac19d8775fab`;
  partial `artifact-0942ccee6868121fc572adaceeff1b32` /
  `6095a5b1fcefaff0e4070a4a8a7b64ed0db1ff6fcb973659265cf1f35d8cfd8e`;
  report `artifact-3c1c8d5ec5beccfc7b671c16403c4c2d` /
  `66b1ae38506ecf9b7fe8f82843dc4da6921ba2575ad5672355f9170672010d02`;
  strategy `artifact-ef38b44d7d49d7b85f9e8cdcebf4283e` /
  `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a`;
  log `artifact-90ee25abd6c18feb15ff2e500681da4f` /
  `07305d0a0caded1cdefacc11d1f3dc79f075ab9e0df79d6ad261e51d1ebd2e0b`.
- short cap: ordinary `artifact-c74cf1b4b84e5805f36227f1575c797f` /
  `bd7686fde8a792e5357f78ba3a55f9c8eb0aec044a90523458f571089e2b3a37`;
  partial `artifact-7183b67c3212abd68376fe8d6e6ea408` /
  `3c2cf1e1ff62f3f91c6a61f47a7bf0ccbc99c8711a270de1bd78d67f37d76fa9`;
  report `artifact-b6b5442a5f9e4d73dd330119c36d9db4` /
  `596e501a1d0727214ae81e3843dae86e87a5a1027971504cfb9a4c21f5b60b10`;
  strategy `artifact-006ae7118c583f092c04cbb943ad02e4` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  log `artifact-196257e187625b4d21b364f4805d903f` /
  `93e2fdb5414b4fca804d7604eb9d7830e9740c1dcac6e4723fbfbbcb85b4c4a9`.
- cancellation: partial `artifact-d1b327b0efc2c27eabbaba454d6d1ac5` /
  `5af837b7cb715cfcf9cf7da96589e2403e7ce61b1f929c23cf9cf99a56b4639f`;
  supervisor error `artifact-93f3322df2207738cddc4a1350f7b085` /
  `628da23585e20c8a082e90938506b4bf2101da6052e75e0adb19d8f2111dc97e`;
  log `artifact-93a8f767491684a024b2fa9e9b1b0d94` /
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

### Investigation Bundle Replay

`export-bundle` for recover attempt `attempt-0b9e...`, with complete key
`carrier-boundary-final-bundle-f8228a13-v1`, created
`bundle-465824d1da2e39466dcb8468`. An exact same-key replay returned the same
bundle ID, path, 2,891,613-byte size, artifact identities, and content SHA-256
`b9fffc77d7b8bd27c852671ead1e5d053e8b7ae4233c6acc09b465950960deb5`.
Its artifacts are the recover ordinary/partial/report/strategy/log IDs and
hashes listed above. Gate 5 passes without MCP, GUI, direct SQLite, catalog
editing, or an alternate runner.
