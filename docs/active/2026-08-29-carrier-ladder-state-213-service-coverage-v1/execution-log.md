# Carrier-Ladder State-213 Service Coverage v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Activation

- Selected by Oliver with “keep working” after the prior closeout recommended
  this exact next ladder owner.
- Starting branch: `main`.
- Starting HEAD: `c51e6e3f8e30eeb683a964ddd6f971119d509368`.
- Starting parent: `3e82e24fe8082f239f5506e0f0b927f59d5baa38`.
- Upstream relation: `main...origin/main [ahead 14]`.
- Worktree: clean except protected untracked file `0`.
- Protected file policy: not read, altered, staged, cleaned, renamed, or
  committed.

The first read-only audit confirms that capture calls the ordinary
`select_initial_row(state)` path for state 213 and finds no completed valid
row. The retained witness does not yet distinguish row-span absence,
unfinished row lifecycle, selector rejection, scheduler ownership, or a
truthful cap. No source or test mutation preceded activation. Broad suites are
deferred until a substantial milestone, per Oliver's instruction.

Activation was committed as
`248430c7ee35ab08e1c8f90c7cbe7640db991ad1` (`Activate state 213 service
coverage boundary`).

## 2026-08-29 — Gates 0-3: Provenance And Diagnosis

Current-source tracing found that the selected-prefix capture occurs before
ordinary publication is frozen. State 213 may therefore be observed before
the ordinary scheduler reaches it even though the same solve later completes
and publishes its rows. The old capture retained only that early observation,
which was insufficient to distinguish a persistent service hole from ordinary
queue latency.

Implemented one benchmark-private, fixed-size, versioned
`carrier_ladder_row_service_witness_v1` projection for the first
deterministically unresolved captured stop. It records initial and terminal
snapshots of:

- state/goal/progress and graph-generation identities and counts;
- broad, ordinary, cache, focused, queue, active, and alternative expansion;
- bounded candidate-scope counts;
- row span, ownership, admission, completion, pricing, validity,
  selectability, variant, transition, and choice counts;
- carrier/frontier/queue/order/cursor ownership;
- bounded typed action-envelope lifecycle, lane, authority, and stop-owner
  counts; and
- requested-finish/resource-cap facts plus deterministic identities.

The source identity is `carrier_ladder_exact_boundary_source_v5`. Terminal
refresh happens after ordinary publication accounting is frozen and before
private record/recovery work. No public C ABI, strategy vocabulary, ordinary
row, value, policy, scheduler, cap, incumbent, bound, compilation, evaluator,
mechanic, action-catalogue, or fragment behavior changed.

Focused classifier controls cover not observed, undiscovered/late-interned,
no row span, rows not scheduled, scheduled incomplete, completed invalid,
selectable rows, certified frontier, requested bounded finish, resource cap,
and goal precedence. The focused native solve suite passed 86,235 checks with
zero failures. The final native benchmark executable SHA-256 is
`e79b2c9318f05acb2a75185ace09fd85b7796071f12a65fd220d848c832652c4`.

### Diagnostic selection development

These pre-final-executable attempts were used only to make witness selection
deterministic and truthful:

| Purpose | Job / attempt | Executable / report SHA-256 | Result |
| --- | --- | --- | --- |
| first natural diagnostic | `job-f53dea32-6dab-4592-addc-49e129600b3d` / `attempt-dc068ced-0f50-41d6-a33a-b72c35f16147` | `61ba83f3a18ce6d0c01cc56a5c833eb00d08ab63f0a1340b3a0e532bb1d2e383` / `00e7ad5d1d769bfd19c4436c445c74cdc5f6cd103ff0bd28508b6fc2e1bdca34` | selected state 1780; proved selection needed the captured stop class |
| record selection control | `job-a5b82906-c7df-415a-8240-36bb4ca213f0` / `attempt-ec9f6588-c462-48fd-bd42-ad6f48aadc08` | `bcb79e6c1e479e38eac2e6bd4ccd68a7e1d275bca13a5d962648da15250de5ab` / `266a1a2da334e344f01fc952e655dea7be62b99cbef39c55a82d8b91ef654ff9` | selected state 1782 because an early certified-frontier fact was later downgraded |
| state-213 initial snapshot | `job-43737252-f050-420a-8f8b-a441b4b3440b` / `attempt-12d79d8d-da32-4d7b-a082-aa535a1857dc` | `32032435de1f21d08ef9f020c0261d943fc492874a492cf676bf13cf277214fb` / `ded97ea362f8ae30c365fa4311c1b7c4140b7f8ae2345b452da767093550b6a6` | selected state 213; exposed the need to apply final frontier validity conditions |
| decisive two-stage witness | `job-6e103d95-b22a-4b22-9831-bb0f57e8e3a2` / `attempt-751943f9-8625-4efb-846c-36ad66d1c239` | `488d1079d04127b233500a052a8d96f7077d398b4b33bed658e69755cadfaca1` / `97f538017712607066414417c667ebd92761b26792326734ef123948360c0d4c` | proved initial zero rows/queue ownership becomes six selectable terminal rows |

### State-213 ownership result

The immutable record revision is
`case-rev-181df5e9f9b7fa36ebb4f75dd6d85eb1` / SHA-256
`181df5e9f9b7fa36ebb4f75dd6d85eb14a138819439fa50244f715b1747f16dd`.
Its final two-stage witness is:

| Fact | Initial capture | Terminal ordinary publication |
| --- | --- | --- |
| state / goal mask / progress | 213 / 16 / 1 | same |
| disposition | `no_row_span` | `selectable_row_available` |
| graph states / rows / successors | 4,396 / 5,093 / 24,149 | 10,065 / 26,119 / 84,647 |
| expansion | broad false; ordinary false | broad true; ordinary true |
| queue/carrier | queued, position 0; not yet carrier | no longer queued; carrier position 3,047 |
| row lifecycle | zero declared through selectable rows | six declared, owned, admitted, completed, priced, valid, finite, and selectable rows |
| variants / transitions | 0 / 0 | 7 / 878 |
| action envelope | zero entries | 15 entries; 7 exact-row complete, 6 exact-inapplicable, 2 unresolved named stops; 13 scheduling-complete |
| terminal owner | no requested finish or resource cap | requested bounded finish true; resource cap false |
| identity / facts identity | `0fc3485cf61f346a` / `1319f535b48e3270` | `2b299dfba3cc1573` / `fcafd2ec3fd9efdf` |
| graph / prefix identity | `f9b9362c26a7f92f` / `9fec2e08127e8e08` | `fc9fdfac72e6f0a8` / `d9e8126d800ae476` |

State 213 was captured early while first in the ordinary queue and was then
serviced normally. Gate 2 therefore passes diagnosis-only: there is no action-
catalogue bug, no persistent row-service or scheduling omission, and no Gate
3 repair permitted. A future boundary may separately improve the recency of
the private captured prefix, but that is not an ordinary ladder correctness
defect and is outside this boundary.

## 2026-08-29 — Gate 4: Behavior And Identity Qualification

The final immutable cumulative-10 revisions are:

- off: `case-rev-e1a5ac7485220794c23791874fd468b7` / SHA-256
  `e1a5ac7485220794c23791874fd468b79524e84e8734cc9eb051fe4df1e111d5`;
- record: `case-rev-181df5e9f9b7fa36ebb4f75dd6d85eb1` / SHA-256
  `181df5e9f9b7fa36ebb4f75dd6d85eb14a138819439fa50244f715b1747f16dd`;
  and
- recover: `case-rev-03e9346553b8b7367f6397406fc733ae` / SHA-256
  `03e9346553b8b7367f6397406fc733aea87f09a8274aa69b2c85c0289c944f73`.

| Mode | Complete key | Job / attempt | Core / full / job identity | Report SHA-256 |
| --- | --- | --- | --- | --- |
| off | `state-213-service-witness-off-v1` | `job-27d7c2ea-8c2f-443c-932a-4b2250ab8504` / `attempt-a02c8727-cc95-440d-99be-42acdc87026c` | `48b7c7ed58fa1827c1ef5bfdb69cf9259a49a57edbea524565900d3a0a8bf64d` / `cc83185a33a4e47ee2cc91ee4d3df728669389184685acf1994fbc8e6fb208b4` / `b307187a0be17a1760032b87213c1b8cc10923ce359a1e7ecde8a6f5a4e01323` | `66c1c8625257b51b6216b364ec044aa182a1d1528ec2f239dc43355ae7724ec4` |
| record A | `state-213-service-witness-record-v4` | `job-33f2852f-f0f7-4243-86ab-8ce778adbd45` / `attempt-aa41823b-90ff-46e2-ae5d-0e855c425870` | `48b7c7ed...` / `14901e41ce57d5a95b8a1e68c8c4e7b79f7438e523e3fcf7c05c4cfb8288c26a` / `3dc9910d9292b5b0494bdf7445893f5366550f74dea48f2c202edbb9bde07ff2` | `7ef0d4bdf76a087f09644899e6e8d803d48fa514231136432b8033c3f48acea0` |
| record B | `state-213-service-witness-record-v5` | `job-c68a32f0-81d0-4474-bf44-a9a63568ac18` / `attempt-494a253f-3f54-4150-8212-e649e155b021` | same as record A | `1d093eaef7ad02564070b2c001ae0fe4ff464d807028ae7d75fe80429dc99fe1` |

Structured CLI comparison of off and record requires and reports full request
identity unequal, core solve identity equal, ordinary result identity equal,
all 20 enumerated core solve components equal, and all nine ordinary result
components equal. Record A/B also have identical selection
`68069fabcc16021f`, prefix `94dcb0cfc5162f29`, exit
`c621919aa67c36dc`, source `a14c78a9fa7af0ba`, initial/terminal witness,
ordinary work, strategy, and exact evaluation identities.

All three modes remain `bounded_feasible` / `requested_bounded_finish` with
lower `36.48853172876641`, evaluated upper `1550334.436668944`, 10,065
discovered states, 4,841 expanded states, 26,119 rows, strategy SHA-256
`bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`,
311 nodes, 875 edges, exact evaluation matched, success probability 1, and
zero off-policy mass. No 10,000-run simulator qualification is required
because compiled strategy behavior did not change.

## 2026-08-29 — Gate 5: Fresh Structured CLI Evidence

No MCP, GUI, direct SQLite, catalog edit, alternate runner, or changed limit
was used.

### Deterministic natural recovery

| Run | Complete key | Job / attempt | Full / job identity | Report SHA-256 |
| --- | --- | --- | --- | --- |
| recover A | `state-213-service-witness-recover-v1` | `job-63dbb9e3-6e29-4333-9a5d-fed5572893d0` / `attempt-f50438d3-a946-4965-9b6d-cfbbe218db12` | `54d6ac18808bc4cd97508c8ab4542f8b3be0dee0ce1cb2a82395c2b12d98f037` / `c369d04f2acc053f03fb1e5ab87e0ea87e14c5215802b5d6d98f3b0cc63e004f` | `f777d23d00525b7e76f80a5fe3170e328043b674dc3ef391172cbdbaadda5ce9` |
| recover B | `state-213-service-witness-recover-v2` | `job-e75002f8-0227-4571-96dc-d3d7c9271efb` / `attempt-3bd4634e-1ce0-467d-8972-59808790f8c1` | same | `ad3423bf4c60f3f7371014899646cdd5e2169a5d5a0e5f14e58d12290e2b471e` |

Both naturally reach private `resource_cap` / `max_transitions` with 26,927
exact states, 892 exact rows, 9,995,835 exact transitions, 3,933 work items,
176,454,520 peak owned bytes, zero members, member identity zero, selected-
prefix support identity `e7070f4026fa7ca4`, and 4,710,858 reached stops.
Their complete retained first support-edge sample is identical: predecessor
exact identity `8d56e91d0520b518`, predecessor coarse state 0, coarse/strict
operator 5/5, action identity `e13ae2cea3ca477b`, stopped exact identity
`833a6a9002e3c045`, stopped coarse state 213, probability bits
`3ee95f1f576b7265`, and disposition
`unresolved_no_completed_selected_row`. Both preserve the same final ordinary
strategy and the same two-stage row-service witness identities above.

### Genuine cancellation and real cap

Cancellation used the recover revision, complete submit key
`state-213-service-witness-cancel-v1`, cancellation key
`state-213-service-witness-cancel-request-v1`, job
`job-e4b4bb35-676e-47d8-964d-fbcf175246d9`, and attempt
`attempt-b315a696-c133-4042-b860-c6a71cae014c`. It was observed running at
native process 57,416 with active lease
`lease-21826b45-73b4-4db0-83f0-9ae82d883530` and a 1,610,612,736-byte
reservation, then canceled through the structured CLI. Acknowledgement took
394.5334 ms using `graceful_then_process_tree_termination`; the terminal
attempt reports `survivor=false` and
`process_group_kill_then_parent_poll`. The dispatcher released ownership,
reserved host memory returned to zero, and no reserved leases remained.

The real private cap used immutable revision
`case-rev-1277deae8d655a691c46ba49557b8f85` / SHA-256
`1277deae8d655a691c46ba49557b8f85f7fb43b92ab4fe770fa9468e8b3ecd32`,
key `state-213-service-witness-real-cap-v1`, job
`job-e1c329f9-3c7d-4cba-9931-826dc5f3cb45`, and attempt
`attempt-6199c1d3-347e-4c38-bc38-d8a99ba6690e`. Its core/full/job identities
are `48b7c7ed58fa1827c1ef5bfdb69cf9259a49a57edbea524565900d3a0a8bf64d`,
`a7c12a7e38e4fac23e61f650034ed428b6edb0fd105fcfb4249d5665b40534c7`,
and `5a9986e8e420030794625265ba0bc623fd4400af03231641d714cc88ce8617ff`;
report SHA-256 is
`8dba5518bab0ddad5f9462987c3b19daacf20c343e2a2ad78f7fa8dc17d45c39`.
The ordinary result remains matched while the private seam reports
`refused_resource_cap` / `max_prefix_states` at cap one with zero recovery
work and zero retained members.

### Fresh ladder controls

All use frozen case content through the repository structured JSON CLI.

| Case / content SHA-256 | Complete key | Job / attempt | Core / full / job identity | Result |
| --- | --- | --- | --- | --- |
| `conquest-lamellar-allflame-partial-4-to-5-product8` / `02b3b2fa5b23b7d3f902ccdf1232969c8c590fae01960329d073df91aefe7b3d` | `state-213-service-control-partial-4-to-5-v1` | `job-5d56ff2a-2612-43be-b295-1d49b9679932` / `attempt-e74a3077-e5b3-4e31-83a0-817cd5c61be4` | `f626f0ed61ad03607e075f0576e0116c81039cd5b2fa07a1ff946df09dd3cceb` / `d7b5ff21eafb704d292154452d58fbcc7b20972d8ceec66c94d4d37dc3378d3b` / `3537b877ba3ca9cf42a6a5f9120f36a92e68c9c1f6330daa9c014a2c62292c38` | bounded feasible/requested finish; lower `36.48853172876641`; evaluated upper `7896721.254200992`; 12,075 states/61,767 rows; exact matched; strategy `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e` |
| `conquest-lamellar-allflame-clean-4-pdr-product8` / `bbd55011c734132ef9f211eff16e67a44cfc72988c65d23f2152acade5ec130f` | `state-213-service-control-pdr-v1` | `job-6bcaa7df-7220-4546-85b4-d55e3181f005` / `attempt-af2d7636-f619-49d8-998c-214369b675c6` | `21b9e9b2818c11641c9ca69902dac964857abb61a7854232178678c0cd33451c` / `cbb33fc115e843bfccbe0dd29b8157194fd2c116d220427f6b826f60c8e495f8` / `7abff59e78484300e8203f5ebc335b97ef739e9d49d98885c58046cd20c8afa6` | bounded feasible/resource cap; lower `21.772459401271156`; evaluated upper `7866.432124027084`; 61,476 rows; exact matched; strategy `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6` |
| `conquest-lamellar-allflame-clean-3-prefix-extended-product8` / `4de8e1100eb2f541cf0654e3891577a725823ed418db1a1c2d9dfbfad160f758` | `state-213-service-control-prefix-v1` | `job-95b58b08-d832-48fe-b60c-4e8dab2e4131` / `attempt-7d31c578-6ae3-4c0c-9489-8ae3d1f6795a` | `4dc66d6cb3cc4c61a0798f989bc72ac3fcfff036c24d8bd2b6f19cfc09061b5c` / `fc148be96981da2639448efbbe8b7a17191dd732feab9ee140193f328a684cac` / `2b21654903ef0f77f3d504837254eb883149db6eeac92de0103f699ba802fd1a` | exact closed at `1618.2138946963837`; strategy `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff` |
| `conquest-lamellar-allflame-clean-3-suffix-product8` / `714588ed7f2325a2204a7d6adf4d8a7935c271b8e9261a4bc77ebed11b168883` | `state-213-service-control-suffix-v1` | `job-5bac2c08-2938-4d77-97fd-853fb045e473` / `attempt-1a50b3ec-07b3-427c-a91c-0dcf0b234a3a` | `0a07e5782a1081b9ad76eae715246c43340b2098031e1e8df36ef4bba04e29d5` / `68a03a2d6a526ff550e455219306f517057aabc4fd8cd1f57419fb0d0e26dcf6` / `d4e2e47c94e19ee694e01f3a945646d3bd7352137c66821a8d7d3ff7baf6eb0a` | exact closed at `1101.15648683309`; strategy `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a` |

All four strategy hashes and evaluated values equal the prior qualified
boundary. Their report SHA-256 values are respectively
`9ed84924f959bdfc1f22acafc924f0639148cb940f0a70eaca57318f9fa65df8`,
`d5a50eed46b799cbd0e93bfbef4f42ec8dee18cf5b3ffcd9d2fbf5e643ff3bac`,
`04b6e2ebc2cbc113b50f5e3e344bc0ed8b0fd61d63f785b68c330966057f243c`,
and `fbaf42c4e913fa9e81a12a778a66272a10f5b03f10a4a428b4b3b60f889b9535`.

### Final artifact ledger

Each entry is `kind: artifact ID / content SHA-256`.

- off: ordinary `artifact-a88d9c0684a6d5b4338921aa22c04bb0` / `bf3a93a32b8b97c5974fd1d78a58b68f0b8d5e05256620b0100d005f8ec15f48`; partial `artifact-5026109e3c881f020f46d90681144168` / `dd2287623e202191e97271999e0fedeb8129e19906f4fcbc98958eb77e0db5d9`; report `artifact-5329d79e90e8932338ff6c168d0b91e0` / `66c1c8625257b51b6216b364ec044aa182a1d1528ec2f239dc43355ae7724ec4`; strategy `artifact-684b07fbcaa81121cf15ca1a71251863` / `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; log `artifact-663125837e410472a87898e6c3e01e72` / `9138bcf7b682516910776c83cea722998182cfd1c1f9d3ac58cc1cec1347ab86`.
- record A: ordinary `artifact-08ecdda0a16b0e5bfa7ae8494c9fc00c` / `0c23b4958bdf7d8b382d11532c246e740f2a37b0cf757ee201bf269a675c4113`; partial `artifact-992884dad53d102257741088c66f5fe6` / `21272ff7bc1af1391bb63798f9f1cf71d7703be4e198088d51ea96a0d387a001`; report `artifact-5c59cb2f6f4d6a6202bd38111bdd7947` / `7ef0d4bdf76a087f09644899e6e8d803d48fa514231136432b8033c3f48acea0`; strategy `artifact-30534a66498d29f5a2991d12796d3595` / `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; log `artifact-b43014b80bf796db94a9f9bb78f5c344` / `a8252cca65ed32342f17fddaeb4e79af87d5757488be6afe86a7177756569888`.
- record B: ordinary `artifact-5cc63d203f29ca1d07cca698deab34c3` / `61114a09aecb2af4cc10c96c4553dc98d36aa7d35a84f4f22571723f40a21a3d`; partial `artifact-f2b7c230cb3c25bf88e0fcb738963cfd` / `67439fa1cc342d6f89342cf496c563586020c304ab7bc716f4d6d49e4107e6eb`; report `artifact-3e0dd1d9620751d77d87b606d99d401e` / `1d093eaef7ad02564070b2c001ae0fe4ff464d807028ae7d75fe80429dc99fe1`; strategy `artifact-689816ea18a1f0cd6e0325fb78a1479c` / `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; log `artifact-2a0dd62f828dad7b3aec0ab2e7478ed2` / `d718ec01fa9d6e64bbe8f3859f634ea93338031e0a32dc3467a2ed499f6ce0bc`.
- recover A: ordinary `artifact-04f49a02edefb3176f2a5939c53b6567` / `d6ab4b8fa783c5d2da0082b9f1c5415761f89a48e45bf6e4bcac4362288264a3`; partial `artifact-51dd2af5f3c48dede33e653276f080e6` / `bc4949f9fe1e80e1c4b92b9823e1e3bd05ddad0cb49307a133b4009396ca8b10`; report `artifact-1b7039528f00a5f2f959a49ec8ecfd5b` / `f777d23d00525b7e76f80a5fe3170e328043b674dc3ef391172cbdbaadda5ce9`; strategy `artifact-6c1ed73cee9327a5dd25745049ebdeef` / `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; log `artifact-471ee78ca2d72e0bde4685947672e7f3` / `717609288fde84a7f5e816f2fcf44cd6d53522043870aa3ad21c3488e55e3b6c`.
- recover B: ordinary `artifact-9e5153adddb523c72a3a77660b6b5c90` / `a6f58d84efae972f1ad6e3882d425f2f5b645ff211be5afd9c5c15393305a51d`; partial `artifact-b6fd85aa149c89ecd8cc0b8b7d8caa38` / `6068f14ab804c09226caafe91c90cfb50266e775754dec6bb8047ffc5d606e30`; report `artifact-e5639abb7aacb36a075ed97be1cc87a9` / `ad3423bf4c60f3f7371014899646cdd5e2169a5d5a0e5f14e58d12290e2b471e`; strategy `artifact-2af7ec5014ec2f0f4f85d76cd7008539` / `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; log `artifact-b55c44d65d0cfb5a0266a430fbc45e5f` / `b0d182da4adde5f4e1f70ef2f4fea32e03c3a61cbc6c2792484de98439e89de1`.
- cancellation: partial `artifact-6eba361d988e8dfae4b995fc52981f2a` / `123b9f54a3fda0738c41b9c6e8a3d3ac42c865497696a8c830eb04ca3b2e5259`; supervisor record `artifact-cbd5be6c91b20e2febb95440500253a3` / `21ebe881b024e46c7cb7421e91ba7bcb3ef8fffd8b2f7367b3e89ea115b9d231`; log `artifact-da4b80f6e0a773db3f235adb519cb304` / `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
- real cap: ordinary `artifact-f038e7c418635993cd63d7f095841e9e` / `8e2c0a073a8c23bf1cbf75fd0f6978d0930d56000d2bdbffd5547ec793733eda`; partial `artifact-4d82bfcf4c9d26a5340613dc138bdc93` / `0ac6b01439c517ac9dfc7c492c73cade63e2f280f5af3a8b4ec6a6a510023dc1`; report `artifact-d451800ccbe18f268792ac7cfee58c6e` / `8dba5518bab0ddad5f9462987c3b19daacf20c343e2a2ad78f7fa8dc17d45c39`; strategy `artifact-ba4f45cf38ae958d618287c73e2e5283` / `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; log `artifact-ecb262c24b56ff7480b696f4d739034f` / `f3fd037a8a4f0db6bfcbd111eb1ee20b1380af745eefd12f551116fc94ef78c7`.
- partial 4-to-5: ordinary `artifact-8c6d0ee814f779001bca902e227a27d6` / `85509e4cb3404bfe794b2fc33c0cdf75eb828f30bf3ed1c347877c31a9f64745`; partial `artifact-b6ba81c46c866bb9b302efff1c205246` / `f07ec63ebaf653587e133f75590994e439721b320e19bfef1381529b2fbd1682`; report `artifact-c8d8983f1fbe939f0eae58cefddd8464` / `9ed84924f959bdfc1f22acafc924f0639148cb940f0a70eaca57318f9fa65df8`; strategy `artifact-491e452d102b90f7de1088975c0b3c7b` / `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e`; log `artifact-3226630e6a02e8df8d97d648a95cd90a` / `c71c555d7797e547606f791685cd388f1f2d93be98db0d69947a63bb6c64d4ce`.
- PDR: ordinary `artifact-737bab3832a275df37c905b190e0a699` / `461df4a40b43cc479cb6acd2a239f217738526e84e46c79a73c616e46d9bfb4a`; partial `artifact-61f9dfade190f1d87221044f5f28c6f9` / `93ca06030cda9d0ab6005dda021ac557fb77fce0e96d5a92acd9c424a3ce2930`; report `artifact-e98ecc6c0d1e898a235d4f9bc8310dfa` / `d5a50eed46b799cbd0e93bfbef4f42ec8dee18cf5b3ffcd9d2fbf5e643ff3bac`; strategy `artifact-9d84aa753fb4b26995efaac471a9b0e2` / `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6`; log `artifact-ac73c4bd6d82ec9925d423e9a057184b` / `9ca46665028cf628f9c89fe822483f40d802e72b446792c293c01995b8edffa5`.
- prefix: ordinary `artifact-719f74adef1a9a1cf6684c921ccceea3` / `b6e925c719707609c33e38e99905939b626c40e7e914508d4658bf90c4acf763`; partial `artifact-4c98a7d3d2917a3f5faa3de904a393d3` / `5b722bfa4a32e7caa3b926355275ed11913c2731f437639a1cf4d324f2cb20f2`; report `artifact-ca35118134da6b2ef4a66d14355388d7` / `04b6e2ebc2cbc113b50f5e3e344bc0ed8b0fd61d63f785b68c330966057f243c`; strategy `artifact-c6cbf987e6a000aadc4f255805c6ec5a` / `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff`; log `artifact-6c0c4ccdec56a5e79dba3bf885e4bacf` / `b066021323ee6ca852f8b73ee73e615c537531ff1634f3787bee4942f6c6eccb`.
- suffix: ordinary `artifact-e3f203a840f61926a1802584ce60de5f` / `3a40e21300394f8b13f59c1d8b418ef90dc9ddf2d2c9f64374c892a9f21131d2`; partial `artifact-bc497c33e88ab898469b7a5ea606e75d` / `7168454d3e58e02a0a594ea651e34f606129f5c4f5b5a559007f07991fd77b53`; report `artifact-d176b0db5889a912b30dec34210ea089` / `fbaf42c4e913fa9e81a12a778a66272a10f5b03f10a4a428b4b3b60f889b9535`; strategy `artifact-dba1d22efc53946df5477ae671d2ab21` / `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a`; log `artifact-76b18536dbd887b57b84db644c98c19e` / `1d469fc2f782ae3e23b353cba6ece59c8d629dfabb2ba02b04478f348b70386a`.

### Investigation bundle replay

`export-bundle` for recover A with complete key
`state-213-service-witness-bundle-recover-a-v1` created
`bundle-3cf5126c5c41fe72f6a30451`. Exact same-key replay returned the same bundle
ID and path, 2,900,507-byte size, content SHA-256
`b8e1dc2f456bd9c7b2d9db830743c2189689dbe38d2e64039b737470f85270da`,
and the same five recover-A artifact IDs and hashes listed above. Gate 5
passes.

## Gate 6 — Pending

Run final affected-layer acceptance, release WASM/web parity, the full
repository pipeline, then perform documentation/scope audits, archive, and
local-only closeout.
