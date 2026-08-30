# Carrier-Ladder Selected-Prefix Closure v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Activation

- Selected by Oliver after the diagnosis-only exact-boundary closeout.
- Starting branch: `main`.
- Starting HEAD: `e5c5ad6b0bfed7287a3f7892330755e77efc35ba`.
- Starting parent: `22c00f50d8d0dd6ef8269cc0dc73a0eab0887bab`.
- Upstream relation: `main...origin/main [ahead 11]`.
- Worktree: clean except protected untracked file `0`.
- Protected file policy: not read, altered, staged, cleaned, renamed, or
  committed.

### Current-source provenance

- `capture_failed_prefix` closes captured reachability over the selected
  ordinary cached sparse row and selected observed-choice routing.
- `capture_incumbent_policy` materializes actions only for states marked in
  that captured reachability set.
- `ExactBoundaryRecoveryWork` replays selected actions through
  `ProductionPolicyOracle::boundary_selected_compact_row_cooperatively`.
- Strict successor interning coarsens every native exact outcome, calls
  `ensure_coarse_policy_parent`, and requires the captured policy entry for
  that parent.
- Therefore the observed state-213 refusal is consistent with a strict exact
  outcome whose coarse parent was absent from ordinary sparse-row support. It
  is not yet evidence that state 213 lacks a completed row in the captured
  graph.

No source or test mutation preceded activation. Routine suites are deferred
until a substantial implementation milestone, per Oliver's instruction.

## Gates 0–2 — First Strict Support Boundary

The source and retained prior report resolve the original ambiguity:

- coarse state 213 was already one of the captured prefix's 485 typed
  `unresolved_missing` stops;
- the capture walk reached it from positive selected-row support, called the
  existing progress-aware `select_initial_row(213)`, and found no completed
  valid row;
- it therefore was not an already-completed row accidentally omitted by the
  `policy_reachable` filter; and
- recover mode incorrectly excluded unresolved stops from its observation
  boundary set, so coarse-policy discovery tried to execute state 213 and
  produced the misleading `no selected policy action` invalid-prefix error.

Implemented the narrow private correction:

- unresolved captured stops are now non-goal absorbing observations for
  selected-prefix replay, never successful requested entries;
- recovery retains one bounded first exact predecessor/action/outcome edge,
  plus a streaming reached-stop count and deterministic identity on every
  terminal/refusal path;
- the public projection binds collision-checked exact-key identities and word
  counts, complete coarse structural keys, selected coarse/strict operators,
  selected-action semantic identity, exact probability bits, stop kind,
  captured policy row, coarse reachability, and completed-row authority;
- full exact keys remain internal; public member samples likewise use bounded
  identities plus complete exported item state so telemetry cannot be
  dominated by large exact keys; and
- no ordinary row, value, policy, scheduler, bound, incumbent, compilation,
  or product authority reads the new result.

The first implementation accidentally retained every reached-stop event when
recovery ended at a cap, producing a 146,997,742-byte private diagnostic.
That defect was found with temporary stderr byte instrumentation, then fixed
by streaming the count/identity while retaining only the first edge. The
instrumentation was removed. Terminal/refusal finalization now owns the same
bounded path.

### Focused checkpoint

- Native build: passed.
- Focused `--solver-refinement-only`: 379 checks, zero failures.
- The new structural control proves a non-goal unresolved observation stop is
  complete absorbing support but does not become the requested entry.

### Exploratory CLI and direct witness ledger

The unchanged deterministic recover revision remains
`case-rev-03e9346553b8b7367f6397406fc733ae` / SHA-256
`03e9346553b8b7367f6397406fc733aea87f09a8274aa69b2c85c0289c944f73`.

| Role | Job | Attempt | Result |
| --- | --- | --- | --- |
| pre-bounded projection v1 | `job-13aec74e-6213-477f-85c0-d2bf0ae098cf` | `attempt-e59361bc-a5f3-44fb-aae9-f13d0c992480` | ordinary result unchanged; telemetry exceeded 2 MiB |
| one-sample projection v2 | `job-98775b38-abdd-4350-9d04-a3a38861f78b` | `attempt-522ae46c-4d65-4a01-a3e5-5b4f29115808` | ordinary result unchanged; telemetry still exceeded 2 MiB |
| hashed exact-key projection v3 | `job-cda47a97-3b1f-4ace-ac88-c6680ae6c2a8` | `attempt-95a4759d-437f-4595-b910-051e2eb3e05d` | ordinary result unchanged; unfinalized event vector still exceeded 2 MiB |
| hashed member projection v4 | `job-3efe8d07-3b37-4c83-8cb9-2bebfe37ba1d` | `attempt-36fc0413-51c8-43ff-91b6-1f7d878dde73` | ordinary result unchanged; unfinalized event vector still exceeded 2 MiB |

Diagnostic-only telemetry-cap revisions were
`case-rev-359f92e25e091577ef10576017963be4` (8 MiB) and
`case-rev-3d6570053fe4f40194993062da01a811` (64 MiB). They were used only to
prove the retained-vector defect; one transient Windows partial-file replace
failure and long generated strategy path are non-solver harness evidence.

After bounded streaming was implemented, direct execution through the same
native corpus case completed with telemetry under the original 2 MiB cap.
Ignored evidence path:
`build/solver-lab/debug-selected-prefix-report.json`.

The exact result is:

- recovery: `resource_cap` / `max_transitions`;
- exact states / rows / transitions: 26,927 / 892 / 9,995,835;
- private work / peak owned bytes / wall: 3,933 / 176,454,520 / 226,189 ms;
- requested members: zero;
- reached unresolved-stop edges: 4,710,858;
- reached-stop identity: `e7070f4026fa7ca4`;
- first predecessor exact identity / words: `8d56e91d0520b518` / 81;
- predecessor coarse state: 0;
- selected coarse/strict operator: 5 / 5 (`chaos` in the pinned product action
  list);
- selected semantic identity / words: `e13ae2cea3ca477b` / 28;
- first stopped exact identity / words: `833a6a9002e3c045` / 85;
- stopped coarse state: 213;
- exact probability bits: `3ee95f1f576b7265`; and
- captured policy row: none; captured coarse reachable: false; completed
  selected row: false; disposition:
  `unresolved_no_completed_selected_row`.

This closes Gate 2 diagnosis-only. No selected-prefix support extension is
authorized, no requested exact boundary was recovered, and no policy/cost
improvement was found. The first owner is incomplete completed-row/service
coverage after the selected root Chaos row, not a missing action-catalogue
entry and not fragment composition. The private transition cap has been
observed; Gate 3 still requires deterministic replay and genuine cancellation
controls on the retained final executable.

## Gate 3 — Cooperative And Identity Controls

All retained qualification below is pinned to native executable SHA-256
`c5f30c685805c1f940eb9c0bf7f0d777d81e90be8e36623928bfeb52a3436410`
and source checkpoint `3e82e24fe8082f239f5506e0f0b927f59d5baa38`.

The genuine cancellation control used immutable recover revision
`case-rev-03e9346553b8b7367f6397406fc733ae` / SHA-256
`03e9346553b8b7367f6397406fc733aea87f09a8274aa69b2c85c0289c944f73`,
job `job-24b5ddc5-7d91-4136-bd2f-af43395396e3`, and attempt
`attempt-aaa09421-eef5-4d4a-a03f-dbc3bf4fc8a1`. Its core/full/job identities
were respectively
`fa602288abda01405087f6ef5903273ffa2788d5aad3599bf40edade448cf3db`,
`a3b6dc96700216ef8f8fe188f87c66e3ceeef6d7c4dc7c934e098a71a26cabd3`,
and `782893a252f1f1ab298a26a618bce481e66288aa2e1a0716d5fb6fb04ea9a36c`.
It was observed running, then canceled through the structured CLI. The
supervisor acknowledged in 373.7076 ms using
`graceful_then_process_tree_termination`; process-group termination followed
by parent polling found no surviving process. The dispatcher returned to
released ownership with zero reserved host bytes and no reserved leases after
holding the requested 1,610,612,736-byte reservation.

Two separate natural recoveries used the same revision and complete request
identity:

| Run | Job / attempt | Result |
| --- | --- | --- |
| recover A | `job-85625d85-3786-4f13-bd59-f91012123ba9` / `attempt-509e37d4-22a8-401e-baf9-0a7c20b2dc49` | `resource_cap` / `max_transitions`; strategy SHA-256 `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`; telemetry 663,678 bytes |
| recover B | `job-a8ee1fc2-f925-411e-a624-39561d9461e0` / `attempt-6d42bd3f-5f51-48ae-b77c-e7d8008ce49e` | same refusal and strategy SHA-256; telemetry 663,681 bytes |

Their exact states/rows/transitions/work, member identity/count, selected-
prefix-support identity, reached-stop count, and complete first support-edge
sample are equal. Both report 26,927 exact states, 892 exact rows, 9,995,835
exact transitions, 3,933 private work items, zero members, 4,710,858 reached
stop edges, and support identity `e7070f4026fa7ca4`. Wall time and serialized
runtime telemetry differ only as non-semantic measurements.

## Gate 4 — Behavior-Neutral Qualification

The immutable matched revisions were:

| Mode | Revision / SHA-256 | Job / attempt | Core / full / job identity |
| --- | --- | --- | --- |
| off | `case-rev-e1a5ac7485220794c23791874fd468b7` / `e1a5ac7485220794c23791874fd468b79524e84e8734cc9eb051fe4df1e111d5` | `job-2962888c-31cb-4aec-8f28-53ab1079ccc5` / `attempt-d3ad6e0f-8a3e-4026-a7e3-b99416b20378` | `fa602288abda01405087f6ef5903273ffa2788d5aad3599bf40edade448cf3db` / `ca4757babbc3b62f2b3f149d534a10649f662a6962dc722d7825515b7fe4bc43` / `4c9f52c62cd24e10eca3eff27e3972643ac1dfa5e0c4e0b00ac6f1c47f642184` |
| record | `case-rev-181df5e9f9b7fa36ebb4f75dd6d85eb1` / `181df5e9f9b7fa36ebb4f75dd6d85eb14a138819439fa50244f715b1747f16dd` | `job-2dced85e-08c2-4b8f-bc92-ad4241ded917` / `attempt-03b3fd56-2286-4fae-914b-62d1b817685b` | `fa602288abda01405087f6ef5903273ffa2788d5aad3599bf40edade448cf3db` / `95ef47a48fd7ba543be8d22f2e21b44717f62338bb8bd47c65a3d76409d177ee` / `4161c677facf7604da51cbf021419555cf928f0c94e062841f49587315594404` |
| recover A | `case-rev-03e9346553b8b7367f6397406fc733ae` / `03e9346553b8b7367f6397406fc733aea87f09a8274aa69b2c85c0289c944f73` | `job-85625d85-3786-4f13-bd59-f91012123ba9` / `attempt-509e37d4-22a8-401e-baf9-0a7c20b2dc49` | `fa602288abda01405087f6ef5903273ffa2788d5aad3599bf40edade448cf3db` / `a3b6dc96700216ef8f8fe188f87c66e3ceeef6d7c4dc7c934e098a71a26cabd3` / `782893a252f1f1ab298a26a618bce481e66288aa2e1a0716d5fb6fb04ea9a36c` |
| recover B | same immutable revision | `job-a8ee1fc2-f925-411e-a624-39561d9461e0` / `attempt-6d42bd3f-5f51-48ae-b77c-e7d8008ce49e` | same as recover A |

The CLI comparison reports `all_core_solve_components_equal: true`,
`all_ordinary_components_equal: true`, equal core-solve and ordinary-result
identities, and intentionally unequal full-request identities. All 20 named
core components and all nine ordinary result components compare equal.

The equal core components are `action_scope`, `allowed_mechanic_families`,
`case_without_id_or_fragment_shadow_v1`, `compiled_artifact`, `corpus`,
`economy`, `effective_disabled_action_families`, `executable`,
`explicit_imprint_scope`, `goal`, `goal_action_list`, `measurement`,
`product_action_envelope`, `profile`, `requested_bounded_finish`, `scheduler`,
`solver_caps`, `source`, `start`, and `watchdog_seconds`. The equal ordinary
components are `action_envelope_ledger`, `cap_resource_classification`,
`compiled_ordinary_strategy`, `core_graph_scheduler`, `exact_evaluation`,
`incumbent_public_upper`, `ordinary_inputs`, `proof_lower_provenance`, and
`status_termination`.

Every mode preserves the ordinary result identity
`4681a1968a133a046afcc477f0eacde02f6353052d8625eab5ed17f78be1e0cb`,
lower `36.48853172876641`, evaluated upper `1550334.436668944`, 10,065
discovered / 4,841 expanded / 5,224 frontier states, 26,119 rows, 84,647
transitions, 1,345,828 reforge work, 311 strategy nodes, 875 strategy edges,
the common strategy SHA-256 above, and exact matched evaluation with success
probability one and zero off-policy mass. Only the requested private mode and
its isolated diagnostic differ.

The diagnosis-only recover sample is deterministic:

- predecessor exact identity / words: `8d56e91d0520b518` / 81;
- predecessor coarse state: 0;
- selected coarse/strict operator: 5 / 5 (`chaos`);
- selected semantic identity / words: `e13ae2cea3ca477b` / 28;
- stopped exact identity / words: `833a6a9002e3c045` / 85;
- stopped coarse state / probability bits: 213 / `3ee95f1f576b7265`;
- captured row: none; captured coarse reachability: false; completed selected
  row: false; and
- disposition: `unresolved_no_completed_selected_row`.

## Gate 5 — Fresh CLI Evidence

The real private cap used final short-path revision
`case-rev-1277deae8d655a691c46ba49557b8f85` / SHA-256
`1277deae8d655a691c46ba49557b8f85f7fb43b92ab4fe770fa9468e8b3ecd32`,
job `job-64a4ad67-86d6-4ff4-b564-0d485279c393`, and attempt
`attempt-a9d3c685-f4af-44fb-9e82-c2169b9fca58`. Core/full/job identities are
`fa602288abda01405087f6ef5903273ffa2788d5aad3599bf40edade448cf3db`,
`aefef5c9e3934d48b2a8039ae8562b2d26bc7a8086bbf1c7152d4d452a7b7fe1`,
and `bcf75f9f06078dc739f28aaa62491483481465b17198e9fc4a23a2a0abea98d8`.
Its ordinary result remains the matched result above while the private seam
truthfully reports `refused_resource_cap` / `max_prefix_states` at cap one,
with no recovery work or members retained.

Fresh independent controls on the same executable were:

| Case / content SHA-256 | Job / attempt | Core / full / job identity | Result |
| --- | --- | --- | --- |
| `conquest-lamellar-allflame-partial-4-to-5-product8` / `02b3b2fa5b23b7d3f902ccdf1232969c8c590fae01960329d073df91aefe7b3d` | `job-1f5c5195-557d-417e-8408-3c99dc811094` / `attempt-b8eb9788-80d4-4009-991f-e9b23c34eb34` | `ae0df6cc42551f0e756c52c2c8784eb1abf1c61fa9e7a6bcdbb952f331b403de` / `2f1af488885fdeb4a7f38b6a2bc504e7cda25958de594b22d53ad49a291b983b` / `dec8c1b13e270162c21c64c7118d1c5e5fd7f9c37c2fda2b3fd0cc6a8bb87944` | bounded feasible/requested finish; lower `36.48853172876641`; evaluated upper `7896721.254200992`; exact matched; strategy `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e` |
| `conquest-lamellar-allflame-clean-4-pdr-product8` / `bbd55011c734132ef9f211eff16e67a44cfc72988c65d23f2152acade5ec130f` | `job-d6c3dcbb-1cf4-4f63-afc7-16d85a87f9a9` / `attempt-a0c2c9c1-8eef-4f6a-8c82-d6a3151b28f9` | `d9df7124b720c452232c0dadc8c2a5ca55a294db5b3c198d32f826131de3ae4a` / `0d2fde2aa16dd0b59ba59a5a695d596676cc99cb729961f8cea84adb87948076` / `8a2152ed2b3352883587675ad8ee4178adaa0ff12cf76dc12c0b70eb38d8613e` | bounded feasible / `refused_resource_cap`; lower `21.772459401271156`; evaluated upper `7866.432124027084`; exact matched; strategy `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6` |
| `conquest-lamellar-allflame-clean-3-prefix-extended-product8` / `4de8e1100eb2f541cf0654e3891577a725823ed418db1a1c2d9dfbfad160f758` | `job-7a7d8b5e-d081-482c-8653-d3556c443c76` / `attempt-7869b407-0649-468a-b5cb-f460ace5c4d1` | `191b05def836ed4b7d39637fd62380d014654e9cbc691382a40cef899a2b8a8c` / `b03aaed26e94ea23035c279d3ab469c926c6ceb89316da88eb47728a28c4900a` / `133c2721b371194875bf3e0825d72aba448625b954d775826ecff58d5418c8ea` | `exact_closed`; value `1618.2138946963837`; strategy `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff` |
| `conquest-lamellar-allflame-clean-3-suffix-product8` / `714588ed7f2325a2204a7d6adf4d8a7935c271b8e9261a4bc77ebed11b168883` | `job-1532ff81-3d1f-4a93-bf20-1ca15d180d38` / `attempt-91c9fa79-3cef-4c84-ac9c-db1210af3bb0` | `c8dd8765cfe7ab71da11dd7d50e4423900e304e2877e12d9c083dc204bd6fb10` / `12268a91b3c992946d914f22b76cdf3d03b6c3d94414aca8d1a08ecd0ade5983` / `3f2beb2362fb0188711232c1af70d8994c824bdacabaade6c8655f41c31a2312` | `exact_closed`; value `1101.15648683309`; strategy `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a` |

### Final Artifact Ledger

Each compact entry is `kind: artifact ID / content SHA-256`.

- cancellation `attempt-aaa09421...`: partial report
  `artifact-d468e691280613dac0a6e6d48b00549e` /
  `70d53bc0cfaae272276448c65830595e46b0d9dcb3155b158c5ce8c376f6ebee`;
  supervisor error `artifact-da3158517605a7ef3a1a12f061dadf97` /
  `1339cb23e249282e878ab878389cd49dba688ec2218ebaff4f0636f84863b9de`;
  worker log `artifact-8efe6468d9ba5152c4310feb8d8a841f` /
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
- recover A `attempt-509e37d4...`: ordinary finalization
  `artifact-1fb951c17e60202686ff3ed5b73532c2` /
  `a68b2f1b835aaaebac2b866380ff74e333b44ee9a1476f308a97f7f677359c0a`;
  partial `artifact-3f376577ba9de5fef3c0a7463e96ada9` /
  `07e3804b222b14920b66c365031ba3477a9093219029dea1b5266310e79a00ce`;
  report `artifact-2366597e85c3adc4a9714046fa3bb219` /
  `787d7cc4fbf748f795e9dea06d05cc01e170fecc4f75641c8237f018a5f6de8c`;
  strategy `artifact-1df9a78db12e3d3e41ffc9d9eeaf64b2` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  worker log `artifact-b251cbffba65fa420ac084c5c315258d` /
  `2ca8d4989b5e0fe6c4c60aa025dc1b857ee8e7a89062326c650d80169208eb00`.
- recover B `attempt-6d42bd3f...`: ordinary finalization
  `artifact-4ec5bf3d31902520abdef20320660c63` /
  `f98095288405d8285dcd9fc6e4ef71c56240d8bb175cb7eeeb8f7fc927b75240`;
  partial `artifact-56c32022274bef7d599fc0e9c5cdebe6` /
  `3bd0745340b5b9b787010c8f2cba28bc81e4dca8c27a3bbd23d782ddf6d6fb7b`;
  report `artifact-1ff963d820e04f86980d1f8e348ac489` /
  `e57320227b0784c7fa7eb80204e36d5931de6d804ab144be66cdbaa01883f04e`;
  strategy `artifact-ac0c4ccfdc284b2afc3c39241101674a` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  worker log `artifact-918af17881a8dd9dd86f2f9d8e824246` /
  `f0fbc941682ed22eccc613077f728fb1ba54f55ce1e3923b9501d3e5129f5fc2`.
- off `attempt-d3ad6e0f...`: ordinary finalization
  `artifact-b36693403c45b3e32abc9c58e3a27dbc` /
  `871677591dd9151cd5993aa5784788dfac26735087ba77ea44badcfd021fc897`;
  partial `artifact-f9a75f318c8dc783f57e81b29258491a` /
  `615a248c34778fa616c13deaf1e98e5fc9a642ab1868a34968e5d33da2bc8bc3`;
  report `artifact-9d53707c34c78ca72a580e2ffe78ab42` /
  `f49e4d58dc877e72dee824718dcfb3ef2c1f8fecfb6254ac7b7ec9194db2f592`;
  strategy `artifact-6ae59468b5a41d2cbb91c9a464d29d91` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  worker log `artifact-dc55cb3696c03fe77d26755e6498b019` /
  `8080a133a3b8d2fffb5496c53a990a4cdce3234943822247463f0234fb030158`.
- record `attempt-03b3fd56...`: ordinary finalization
  `artifact-83b2e229913534172ffafe869678acc8` /
  `d114e36cdd03ed47027fdc17d4109c9b5a1c545726259121e9ec1051a9c0b5a6`;
  partial `artifact-da1b8c348a435bb9828135ea91f28b8e` /
  `aa07c4a613eeebcdf82dbaa955183c5c7ece7f5a4e49ddca13326fa4972e46be`;
  report `artifact-692c65ea9368523b275e4eaf390b7e11` /
  `d344cc1ac5ea7496d81c4d83a15ed7fb4bdc344be4af4d38c12c1ce46c54d60e`;
  strategy `artifact-0e3f1c8268faaf4367fd67d6ea1ca072` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  worker log `artifact-293a1c6810f407e277e620d050212d00` /
  `c47d60349556afe17e35f431e2866348e70f7341f30b3123e9f7bfef442e8b34`.
- real cap `attempt-a9d3c685...`: ordinary finalization
  `artifact-88517525918d2b50f835877d0dd92d3e` /
  `13916184c042cfad5afef643cf2b6346e5ecb4f87b2f85c4a469bd023b693253`;
  partial `artifact-08a220abf98e2e835a1b7c729c2d579d` /
  `7ed45a24de0911de7f67e8bc5b64c5acc2bf453537ac18ccfda2f8323a2557f7`;
  report `artifact-498b8686f0268d93ee1a76f068880681` /
  `2469854359585cc2090c589a6aae4fd193049387e55b1ed80d2968609b4a1ec6`;
  strategy `artifact-33de5ddbf95d1d540f499b44ee082c27` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  worker log `artifact-cd17ee7ca2635adc62f1bdbe01df4172` /
  `c64aa36fa787a95a08db03271edbc40ae9ee990a856771614a823a765ceef1b2`.
- partial control `attempt-b8eb9788...`: ordinary finalization
  `artifact-7d29183d5bed4c528b95d05cd9d4cd37` /
  `8cc82096dfcda6e2b255ec71f1b199a8424e8a60f43a9b3de5e95d0db2e2bf42`;
  partial `artifact-3eee2403482a19c3fd2ffa77ebe627c2` /
  `f1132b78c064ff21efb33f473a8682edaadcfffeb879977de7eadc8623716090`;
  report `artifact-3c3368aedc64722ee92c9cb7ece73984` /
  `49c2990adb074b01271bc7003a6e792b4ca17de9cabe809fe2d7820c68781e8a`;
  strategy `artifact-80cecc9c32387041ae3c8b7665f089ac` /
  `0762b21dcd1b91a78bac45ce9dac66552b9a7f7ed7f623c783fd342a4b59090e`;
  worker log `artifact-bf19b58f8f90ea51bb8e7cef901d33a0` /
  `fd8492bfa512232aeca5cd13b416c888fdf4ee9fbc767a1722e55ae097908a08`.
- PDR control `attempt-a0c2c9c1...`: ordinary finalization
  `artifact-3436d4a32c4cf50cc2fdf78898f3f301` /
  `16c8af22a23bca8c7be5dc13d433e9b1eaa2abfd4127fe59b2fb771cd2c2d1d7`;
  partial `artifact-9e02c802fa49179a906dde859c830e85` /
  `799cfff71c1132208fa0a5ac663b6dce96a94959f7eb39c8c32d83bdb3911d1e`;
  report `artifact-0a60ebe16c252ab2d8ce9fe2f3a8ec81` /
  `8d97e03b659585e262d0e1d99052cac859eb2acaf6e853a7e3e14fab88f529ca`;
  strategy `artifact-ee3276bed408b0c00db0b8ab59946f37` /
  `f4c1ccb6adf0c8135f2b1cb72a5952bd6875b2ecb5221943f83bc136f686a9c6`;
  worker log `artifact-13e32f533aa80500b40e5b8f08d0814f` /
  `4d66d2a39b0655140dbce30d71b8b9b79a4578a618682c745371bf5133495b5a`.
- prefix control `attempt-7869b407...`: ordinary finalization
  `artifact-d7edca7b58670a2a251a4d86e8629165` /
  `b677b06c9878f54ed67e4e248fef3b0027442449225c40bdbe8394330cbe09c0`;
  partial `artifact-fde0c43126175728e9d93fb6c67a24c1` /
  `36f5065ccb632c531c1b0d5f905b80a79052a067cac5d81a9a73cfaf94b9e47f`;
  report `artifact-ad115fab249d5d7b7f486085a6079b9f` /
  `d2c15a5f8cc4263fc6d71b2f46a8237531b6ae882d3c8f37eee73476548f3aa1`;
  strategy `artifact-41fcca6e8110c5e63010596b30c557e2` /
  `9cca84a7546d3ceefb36c04e04ffc2d4e84462565e556505c2a1fa00b940daff`;
  worker log `artifact-5335187d7f758ccc7d8a93a7437d75c3` /
  `75d1d5ef2746cc6fb3d490fc7cef703f0cf28419af883b1cf5445b7206c4c57e`.
- suffix control `attempt-91c9fa79...`: ordinary finalization
  `artifact-c0cb9a7284dbe2fb1cc02088d03514ad` /
  `e82f3c5765fc11e7fce97a143e4b1e7c6f8ec1fb7346140fb46e464eb0935c2a`;
  partial `artifact-119a4f2f53ea0ef071016df361ed81f6` /
  `480f769b531bf397a2905b6a83c4e5222e5b858ea0f040b000a205cbdbaabe6a`;
  report `artifact-898d65b8568e154a4e574d4c9211e9f8` /
  `21826c2726c92bbe1a9e113c1e8865f97b9151f5091a5772004a7beba08719ed`;
  strategy `artifact-d5a067339cd47df0257bdf24f197ecd3` /
  `dee8548fce8b2e92150c9243fa47abf2863a0312b9a5f67fb14ac3511db8872a`;
  worker log `artifact-1b88ec038037e32ec28797636bc00e49` /
  `276e7ce62c5d9e4da6ede2849d7c8645889ea0b0ab84fb753436328fa5b020fe`.

`export-bundle` for recover A, with complete key
`selected-prefix-closure-final-bundle-c5f30c68-v1`, created
`bundle-62fe0ec9b91f6318e9907c63`. Exact same-key replay returned the same bundle
ID and path, 2,887,972-byte size, the recover-A artifact identities above, and
content SHA-256
`b6fa90471fafb1d2be089930eebf397e464c0f8605f0917139dce59ba106fe8a`.

Gates 3–5 pass without MCP, GUI, direct SQLite, catalog edits, or an alternate
runner. Gate 6 owns the single complete acceptance pass and closeout.

## Gate 6 — Final Acceptance And Closeout

Implementation checkpoint:
`3e82e24fe8082f239f5506e0f0b927f59d5baa38` (`Attribute carrier selected-prefix
support boundary`).

The clean native rebuild passed and relinked the benchmark executable to
SHA-256
`b07be0ba216563a601270ef93fcbc1278be0cb41196fa2ddfb70d7d541d470bb`.
As with the earlier qualification, the build path regenerates common objects
and the Windows PE byte hash is not a semantic cross-build identity.

One post-relink natural recovery used job
`job-98913cfa-5bc4-4a30-be20-13140a1221b0` and attempt
`attempt-71dc3328-a65c-4b5c-aefe-f53d471c0787`. Its core/full/job identities
were respectively
`a2e8429275f1ca071e024de9f7a54e7a85070639a92cd3be8ed3c61535818018`,
`c6d19f993d37cff442e32d8dcfa51d50889b22d243dadcd6f00b26351f66d9d8`,
and `e9ab139d64b5f26eede31a4b0ee3aaa557e970042b280fea4e139cef17157fab`.
Executable binding intentionally changed request/core identity. The recovery
status/refusal, capture selection/prefix/exit identities, exact state/row/
transition/work counts, private peak, member identity/count, support identity,
reached-stop count, complete first support-edge sample, ordinary states/rows/
bounds, strategy SHA-256, and exact evaluation are exactly equal to Gate 5.

The post-relink artifacts are:

- ordinary finalization `artifact-39ad272bf7181b6daf677cf7379ca8d6` /
  `d615d54909626c356ad448c4d6ce7dcc3c357926f6b0993f4724b55b971dbcd8`;
- partial report `artifact-ae7dbf8a24c9305a92f65bfeac799628` /
  `5bdfe378280d3e945f970ceb09bfbe96ed9e2d3d8441cdb71d2557883d7e3165`;
- report `artifact-5a830edb1727343f3358c83479dc89cf` /
  `feea8d797e920d99531c1bf1fa9b005c8816f5e4fdc2f36b6406463155932150`;
- strategy `artifact-56bb1c00806bb66aba7781c39930fc27` /
  `bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`;
  and
- worker log `artifact-325ad9c8e42f9205259e769229ef8b64` /
  `5bd6f5e774d23371c99ee90747d1eac6c30482cef618ff741730f51a14a00228`.

Acceptance results:

1. `powershell -File scripts/build.ps1` passed.
2. Complete affected native suites passed: refinement 379/0, policy
   refinement 2,083/0, solve 86,224/0, compile 583/0, evaluator 15,761/0,
   API 2,816/0, and fragment 220/0.
3. All 13 native benchmark specifications validated. The complete Solver Lab
   and CLI pytest batch passed 73 tests. A preceding `unittest` discovery
   invocation collected zero because these are pytest tests; it was not used
   as qualification evidence.
4. The release WASM rebuild completed. The artifact is 6,480,040 bytes with
   SHA-256
   `769fa8fcf0e2e596b66a4ad36647cf89b79a85877a1b1c987cdd8fc3d1511c9d`.
   `npm test` and `npx tsc --noEmit` both passed.
5. The single final `powershell -File scripts/test.ps1` pipeline passed:
   ingest/data/fixture/artifact validation, 3,417,672 native checks with zero
   failures, 13 benchmark specifications, and the complete web suite.

An attempted native `--help` invocation before the affected slices was
interpreted by this positional test binary as an artifact path and produced
expected missing-artifact failures. It mutated no repository state and was
not used as test evidence; every correctly invoked affected slice and the
full pipeline passed afterward.

No executable strategy behavior changed, so no additional compiled-strategy
10,000-run qualification was required. Existing native acceptance still ran
its normal 10,000-simulation controls. Gate 6 passes with the valid completion
outcome **diagnosis-only**.

Final `git diff --check` passed. The scope audit contains only the retained
benchmark-private native diagnostic from the implementation checkpoint, its
focused native test, the regenerated release WASM, stable solver internals,
and governing boundary/handoff/index documentation. No public C ABI, strategy
vocabulary, mechanic, canonical data, GUI, action catalogue, fragment
integration, product default, or protected untracked `0` change is present.
