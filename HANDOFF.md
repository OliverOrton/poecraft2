# Session Handoff

**Status: B1 through B6 and the
[mechanical solver split](docs/archive/2026-07-22-mechanical-solver-split/README.md)
are complete and accepted. Oliver selected
[focused-round performance attribution and scheduling](docs/active/focused-round-performance.md);
Gate 0 boundary verification and fresh baseline capture are the exact next
steps, and no source work has started.**

Oliver selected the now-completed
[bounded policy results and benchmarking plan](docs/archive/2026-07-22-bounded-policy-and-benchmarking/plan.md)
on 2026-07-22. The branch is `codex/bounded-policy-contract`, based on clean
`main` commit `60500ef`. Boundary documents were committed as `0c18493`; the
B1 implementation, including retained constructive-witness reuse, is committed
as `58aa5ea`; B2 product presentation is committed as `86e337f`; the B3 native
feasibility/generator implementation is `4c99cd0`, and its pinned corpus is
`d06efff`. B4 orchestration, bound trace, action utility, and search-cost
telemetry are committed as `1d891a0`; B5 stratified reporting and the staged
iteration workflow are committed as `e40b73d`, with accepted B5 handoff
`099d6aa`. B6 final acceptance, the materialized-carrier permanent-bench fix,
stable documentation, evidence summary, and plan archive are committed as
`3d76198`.

The mechanical-split selection is `39b5613`; its unchanged source parent is
`edb7d8fc10eee2a8c34d86b08c5a471243bc6c8a`. The clean execution start was
`edc03a6`, whose only intervening content was the HANDOFF economy incident
record and which therefore did not change the baseline. Accepted source motion
is commit `042a281` (`Split solver solve phases (token-equivalent, no behavior
change)`). The economy pipeline was not run, published, or otherwise changed
in this chunk.

On 2026-07-23 Oliver selected the focused-round plan above. The read-only
[post-B6 reconnaissance](docs/archive/2026-07-23-post-b6-reconnaissance/README.md)
is archived as historical input. The selection is documentation-only: solver,
binding, build, fixture, artifact, and web source remain exactly at `042a281`.
Oliver also confirmed that the natural-T1 generator is intentionally T1-only;
tier-range support is not a missing or deferred generator change.

The preceding exact constructive-policy milestone remains review evidence only
on `codex/exact-search-design` at `273831f`. It was not merged, rebased,
cherry-picked, or used as a source donor. Candidate A/C search guidance remains
forbidden. The incumbent bundle and retained witness are for executable
upper-bound/output production only.

## Mechanical solver split accepted boundary

Gates 0 through 8 completed in order. The chunk only split
`engine/src/solver_solve.cpp`; it changed no mechanic, public C ABI, strategy
vocabulary, test, Calculator code, `solver_internal.hpp`, or engine-smoke test
source. The exact oracle, exact evaluator, simulator verification, compiled
strategy verification, profiling, tuning, and economy pipeline did not run.

### Boundary, tooling, and commands

The first commands were:

```text
git status --short --branch
git branch --show-current
git rev-parse HEAD
git rev-parse 39b5613^
git diff --name-only edb7d8f..edc03a6 -- engine bindings scripts engine/CMakeLists.txt
```

They established branch `codex/bounded-policy-contract`, clean tracked and
untracked state, HEAD `edc03a6`, selection parent
`edb7d8fc10eee2a8c34d86b08c5a471243bc6c8a`, and no source/build-path
difference across the intervening HANDOFF-only commit. The toolchain was
PowerShell `5.1.26100.8875`, Python `3.14`, and GCC
`C:\msys64\ucrt64\bin\g++.exe` version `14.2.0`. CMake was unavailable.

Every build and benchmark process ran in a detached
`CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW` process group with a 900-second
watchdog, tree termination on timeout, and a survivor poll. The exact build
invocations were:

```text
py -3 build/mechanical-split/watchdog.py --evidence build/mechanical-split/before/build-watchdog.json --log build/mechanical-split/before/build.log --watchdog-seconds 900 --cwd . -- powershell -NoProfile -File scripts/build.ps1
py -3 build/mechanical-split/watchdog.py --evidence build/mechanical-split/after/build-watchdog-3.json --log build/mechanical-split/after/build-3.log --watchdog-seconds 900 --cwd . -- powershell -NoProfile -File scripts/build.ps1
py -3 build/mechanical-split/watchdog.py --evidence build/mechanical-split/after/wasm-build-watchdog.json --log build/mechanical-split/after/wasm-build.log --watchdog-seconds 900 --cwd . -- powershell -NoProfile -File scripts/build-wasm.ps1
py -3 build/mechanical-split/watchdog.py --evidence build/mechanical-split/after/web-test-watchdog.json --log build/mechanical-split/after/web-test.log --watchdog-seconds 900 --cwd apps/web -- npm.cmd test
py -3 build/mechanical-split/watchdog.py --evidence build/mechanical-split/after/tsc-watchdog.json --log build/mechanical-split/after/tsc.log --watchdog-seconds 900 --cwd apps/web -- npx.cmd tsc --noEmit
```

The fresh native count gates ran before and after with:

```text
build\engine\poecraft_engine_tests.exe --solver-solve-only
build\engine\poecraft_engine_tests.exe --solver-api-only data\compiled\current
build\engine\poecraft_engine_tests.exe --solver-eval-only
```

The run-local deterministic evidence commands were:

```text
py -3 build/mechanical-split/derive_input.py
py -3 build/mechanical-split/body-audit/body_audit.py baseline
py -3 build/mechanical-split/run_benchmarks.py before
py -3 build/mechanical-split/body-audit/body_audit.py compare
py -3 build/mechanical-split/run_benchmarks.py after
py -3 build/mechanical-split/compare_benchmarks.py
```

For each benchmark repetition the runner invoked the fresh phase binary as:

```text
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus <exact-case-manifest> --case <exact-case-id> --output <distinct-report> --skip-verification
```

Each of the three performance cases used one discarded warmup and five
measured repetitions before and after. Each hash-only case used five measured
repetitions. Across 56 benchmark processes and seven build/check processes,
all 63 watchdog records report `timed_out=false` and `survivor=false`.

### Fresh baseline and input provenance

The baseline native build passed in `211455.00970003195` ms. Its watchdog log
SHA-256 is
`dfffcf13375765e8514ba4d666aa9190b8c51eac8d6215bcd84168c64e295fc2`.
The fresh baseline test executable SHA-256 was
`679213c7a19b453a27d4e6942a5e3f1f2a201193f4f48196c0f99ada7751d2e4`;
the benchmark executable was
`2e8356321a06b2e396d5f0f3d57a708cd86248d0e874aeecd67c87329c77c324`.
The compiled artifact manifest SHA-256 was
`c21886798080e98cfaf85e000851e7d9d3074147d5a6237f78cb51fa890d0d4d`.

The exact derived 2k input came from
`fixtures/solver-scaling/v1/cases/dire-pelt-two-t1-product.json` and its
unchanged manifest. Provenance hashes:

| Input | SHA-256 |
| --- | --- |
| source case | `23def8caaba8e07cb38887ca43926f7658f0ea3699fbef95392be23a97682980` |
| source manifest | `94bdb48b7e6b72626dfd6c33b9e51c975aa196bf77a3cc94645b6f382218458b` |
| derived case | `5d03e15d0ad1eb3f56277717e8cc0d594eb542067bc8b8868b74ef91d29e1fa3` |
| derived manifest | `c2b00f15ea8cfb2bc69ffe2c23baea773f3361aafd696d383a655b32f2658afc` |

The case changed only `id`, `caps.max_expanded_states`, the added
`caps.max_absolute_optimality_gap`, `expected.solve_status`, and
`expected.verification_status`; the manifest changed only `cases`. The
derived case intentionally exited 2 as `refused_state_cap` in every
repetition and passed all plan-defined cap assertions.

### Native counts and source-body gate

Pre/post counts were gate-equal:

| Existing native gate | Before | After |
| --- | ---: | ---: |
| `--solver-solve-only` | 391 checks, 0 failures | 391 checks, 0 failures |
| `--solver-api-only data\compiled\current` | 45,762 checks, 0 failures | 45,762 checks, 0 failures |
| `--solver-eval-only` | 1,695 checks, 0 failures | 1,695 checks, 0 failures |

The body auditor tokenized C++ while ignoring whitespace, included lambdas in
their containing bodies, excluded signatures/outer braces/initializers,
normalized declaration defaults, paired overloads by unqualified
name/parameter-token signature/occurrence, and compared exact body and comment
payload multisets. Before and after both contained 193 bodies and 82 comments.
The comparable body multiset SHA-256 was
`16adb3b1dd2c5e5f329ccf183db6021508714cb9dc1b89a8b887af9fbc99fce1`
and the comment-payload multiset SHA-256 was
`d3b469c09e5517e49b2fdcaf908bd61921fceca7c28edc4cdb9a3ba0ca00d962`
on both sides. Baseline inventory report SHA-256 was
`2511d284fefbe833ab60eb7f0294e73ad075d83667e1d52d21a46fa6e687116d`;
post inventory was
`b14252d78fd8b9ff47681d26488db1d0409fece967eefcd512fe1c5037af9a0f`;
the passing comparison report was
`feb073dd6e817aa66b52bcde1893b665b52f37852a6026177c3f89238798eadb`.
`git diff --check`, a color-moved review, and a second structural review also
passed.

The accepted layout is exactly:

```text
engine/src/solver_solve.cpp
engine/src/solver_solve_bellman.cpp
engine/src/solver_solve_constructive.cpp
engine/src/solver_solve_expand.cpp
engine/src/solver_solve_finish.cpp
engine/src/solver_solve_focused.cpp
engine/src/solver_solve_heuristics.cpp
engine/src/solver_solve_quotient.cpp
engine/src/solver_solve_telemetry.cpp
engine/src/solver_solve_types.hpp
```

The private header owns shared declarations, private types, and
`SolveWork::Impl`. The retained entry file owns construction and solve entry.
The phase files own Bellman/step, constructive incumbent work, expansion,
finish, focused lifecycle, heuristics, quotient handling, and
progress/telemetry respectively. The locally scoped `exact_partition` template
remains token-identical inline in the private header because its definition
must be visible at its use sites.

### Deterministic hashes and performance

Every one of the five repetitions for every fixed case produced the same
pre/post pair:

| Case | transition bits hash | policy bits hash |
| --- | --- | --- |
| `mechanical-split-real-product-2k` | `b957f6e9d877b74a` | `e822e68756d7cfc6` |
| `solver-scaling-dire-pelt-two-t1-chaos-strict` | `2d2b5656301764c1` | `4e38cf37211140ba` |
| `solver-scaling-dire-pelt-two-t1-chaos-quotient` | `505cd3271f47ca66` | `1e4aef1dd50cee68` |
| `oracle-real-two-mod` | `845d39705f1a45da` | `763df0ca5ed817c5` |
| `refusal-state-cap` | `5ac77dee07447866` | `1cedae1f72f786c3` |

`oracle-real-two-mod` is the required millisecond hash-only fixture, not the
prohibited exact two-T1 oracle. The exact oracle was never started.

The exact primary five-run values, medians, one-sided post/pre ratios, and
1.10 decisions are:

| Case / primary field | Before values | Before median | After values | After median | Ratio | Gate |
| --- | --- | ---: | --- | ---: | ---: | --- |
| 2k / `phase_wall_ms.solve` ms | 22795.0888, 22604.3551, 24127.0446, 24306.6987, 23451.9368 | 23451.9368 | 22314.342, 22383.4564, 22263.0481, 22048.0779, 22200.9199 | 22263.0481 | 0.9493053085491856 | pass |
| strict / `solver_telemetry.timings_ns.extraction` ns | 22048282300, 22456028100, 21979256100, 20827179900, 20919294900 | 21979256100 | 19277929200, 19530154500, 19431776100, 19366448000, 19312349200 | 19366448000 | 0.8811239066457759 | pass |
| quotient / `phase_wall_ms.solve` ms | 35916.2306, 35730.2877, 35177.3749, 36267.043, 35616.7477 | 35730.2877 | 36452.6355, 36242.7895, 36391.2877, 36444.894, 38020.0525 | 36444.894 | 1.0200000152811532 | pass |

Secondary before/post medians were captured from the same reports:

| Case | `phase_wall_ms.solve` | `phase_wall_ms.total` | timer medians in ns, before → after |
| --- | --- | --- | --- |
| 2k | 23451.9368 → 22263.0481 | 24603.5917 → 23268.6322 | abstract layout 89300 → 78300; compilation 946577400 → 875171300; constructive 13018635800 → 12391342000; expansion 1224810100 → 1109859500; extraction 42330300 → 37861800; optimization 0 → 0; registry 165600 → 141000; setup 116500 → 113000; strict cover 1619499800 → 1562879900; transitions 767711000 → 710912000 |
| strict | 39617.2826 → 36624.8544 | 61615.6149 → 56017.2981 | abstract layout 31400 → 31500; constructive 12100599300 → 11747874300; expansion 556961200 → 535556700; extraction 21979256100 → 19366448000; optimization 72372900 → 73124400; registry 174000 → 175300; setup 109400 → 105600; strict cover 6026200 → 5843300; transitions 121509700 → 113812900 |
| quotient | 35730.2877 → 36444.894 | 35804.2851 → 36517.2934 | abstract layout 32100 → 31900; compilation 53968900 → 53273600; constructive 11832619300 → 11823448900; expansion 515529600 → 524961100; extraction 2799000 → 2898100; optimization 1192600 → 1149000; registry 180200 → 177300; setup 110000 → 105700; strict cover 5985800 → 5978900; transitions 112899700 → 113145800 |

The baseline benchmark summary SHA-256 is
`d9784ca01b3c83dc09a4f2d0493503afc5e2860a1bfcda64d72492e0fdabbd56`;
the post summary is
`984876f6ee4fb1dda38e7fa37ba35ec1d98a22e19d1897860589c8b0b4fc31a4`;
and the passing comparison report is
`af63391b50f6a45d53e8025071fc2972a2224468f012cde101f68b5d1580a3a3`.
The exact measured report SHA-256 values, in run-1 through run-5 order, are:

| Case | Before reports | After reports |
| --- | --- | --- |
| 2k | `9f8fe0046b1d3feaed55baa72c6acc8271a1d5f35cce4217592f7a11379eff6b`, `f393b57e1d9958076974d8218ab11e2ff89c2d37027f76ee8244f1e9141585c3`, `68fcd6cf589d21c38e46e60e222d3a429e6d01c68e4e0ec1abb182b9179243a0`, `16138c04795c2f9a8b7228f17d3aa4fb5234f7b5fc2e9317974deaa3a03e1b23`, `00ddaa7ea43e3e15f908ae7a795aea64adc9d0c0d5371db2ccd341fec42cf23b` | `d7b40cd01e3c3c77660036bf0b2a8dea9aadb355e4b62dde43f78f517dee2a13`, `f5ac345b802506ad142c818d0712753a3ac9a9c315207d956d1c08d39e5f5cba`, `5fdb7320680cecab8225754af31f3d8e35be9355ad41ea12436cb59fc3c1cd0f`, `68070b2c2a135dabb1ffb15646b31fab66efa7ffc5740b4535a79f40826133b9`, `8bd36d838cf80f42e724cd14ea9a84e38ef378dd7aaa94317672119866eef7fe` |
| strict | `89c3110d7cf024dbd3e0b1aea41fe3552d72a91d078c7c56d10380cee922b0af`, `9ea1b873f811a7f2bd1fa87b3c23aa0d8815adb13ac1b1f1f802a60cb5d34d2c`, `be944fe3173815b0c6025e675dd093b997c1e8463ccf6d41769422c94a18d61c`, `f65f3dbf4c70eb1cc9b70275ff05edaff2b7f30d54d6ab4013d6811de731f9af`, `f3c2e33ff7f1cf0e6c207f24b0b401fc85c0f8fded3d9788984be5fa260f4d32` | `a75c110e53474f0b371a9a2cf48fd0eb73b75453740c324e1c6114ca1c765965`, `35164769652c3e561cdbb3202d74baa0c47c2b89593bd2acefe312e94599e05f`, `af99d584c4a0502546e91a36a2db42c1baaa8efdff854d0c2f7d708f8212907e`, `b1f9975589d005b0e950680e0281f7864507976ccbb0c11d82f2a59e480c344b`, `f5933d16289c74ccdc7299c48c5ac19076abe78dbda07594e9ce980c063390a4` |
| quotient | `d63744bb1a16ce6e618e2adbd4603dfea69838137538b73816234d40fe1441fd`, `c30f7f5bb1d8dec2831bd3e6b560159ff0fe1b3169d1d41620ffc740f7a123ae`, `447968149e8fff69f94c321047d4ce73645e92f369b3dabab007692ba2f24c8c`, `3c1d1c03d617a70b67c096fda49c33d901735fcd65055d3440def7b032c44ade`, `ace6212f5d1e38d8a08c3a3d67152b175adc97520330b18d8deffe29618e725f` | `f8b9d27aa5f5e81de8c9d7a55c00b89bb39e3dee920f745a42d7d8a118b5f31e`, `b06ecf7fbe44d3faa5d7726f030300ef7c6a8a5e6410929697e2da6107555244`, `75997ce94c5358158a7f3d099757e86446825c3740bb30634748f1e27d6da1c8`, `7a395f3284b1e2e173f8f7e61b0539e0d9785237386c08c226ffa9fed72a6008`, `590ec11ad32ceb47674a793d730b419b380ad92da466ebbe7c6b15e13abe4a07` |
| `oracle-real-two-mod` | `32e186907c91db4f1d79287152ba1dd21ad69cdb3bfbff850f4c00b24a4ec9f5`, `1753849aba55ca44302793ed0b5c4fec094491c673b67cafbf0c9c45bc89c6e4`, `8561f6c8476a84e5cf057242653c2e7d1e19ff6d45b18a0831610937f15d1da7`, `7585172c229cd783c1632296f167dd1b47329a68978af574ab120df8c656cf0f`, `7eb0c30ad4461c214feeb0b0302a2a3d701d57a92c2d0e5fce480e41d0c662f6` | `8eaccf67fc7b1b53b240324e2fcc189e865b135508fc084f248acea38c782daf`, `cf8a14463492df48574d056a566de7f60de77a8727fb8ec0836820a378afed0b`, `063ec81a6de0f97025e87277f2ba324c0d137661962e540a1aa6ad10619ad63a`, `c970dd745cd39225e894da24c48b9043f93551984cfdfd18e5ca41ded3c64858`, `66346fda3050c7b71d960ebd16c3b2d5afdf37f3bac4dbf272b9b48c48b69cf6` |
| `refusal-state-cap` | `533d468afa5f12ca2930f62cb0bfba38e4fbebec35f55630b3d05fe10b915f80`, `b05574a328815ac54fb1921197f1a062fe5447816c2f9abd287d888d358dc094`, `366e5086161bbc4b178de10ef4ff7defc12340fb87f267e5095b2d690e700a50`, `24091ba8ec0bc93e627929b43623c3926c5b428912a91384eb77ce9d4309c6bb`, `fa67c46028ac9964ea005468ee980c0be71598e9dfe64ada7acf896776ba0088` | `a63111960c28b1e917d51905c7ca1b26c734a81757250d5a82b82950bd4e14ff`, `76039b5300e139b46eab78ae11faae0159beb69bfdd386791bbb30b969ae14bb`, `801ac357fff21c2ede28257db2837f8b7e76f2aafae5f010f1144bfcc759fa26`, `eae24d25556d2bacea0c5cf4bf76b50788be746d50317696014995592406d31f`, `b1ed0f65aa10427a1061543f7bf2cfe616a8dc0973ea71b9b2ebe860dca6f1bb` |

### Build-path and downstream acceptance

`engine/CMakeLists.txt` explicitly lists all nine translation units exactly
once. Both `scripts/build.ps1` and `scripts/build-wasm.ps1` use the
`engine/src/*.cpp` glob and each executed all nine exactly once. The native
fallback and WASM glob paths therefore have local build coverage. CMake's
explicit list was inspected and updated but could not be build-tested locally
because CMake was unavailable.

The first two post-motion native builds failed on declaration/qualification
plumbing only (nested return-type/template visibility, then missing
semicolon-only defaulted special-member declarations). Their watchdogs
reported no timeout/survivor; log SHA-256 values were
`c62f1e51dc2fab5913bd85820ee0c654d5d371d38d7d2e55cc689c1610a68440`
and
`7c62f1cc070a84d688906dc8664b930de88313655474e87471aa96334e7777e3`.
The fixes stayed within permitted declarations/qualification plumbing and the
body/comment gate remained exact. The final native build passed in
`230843.85639999527` ms; its log SHA-256 is
`5e77a56f93e001eb6e1a12b7226640a5286e17de05e659c05a4f2466c192614c`.
Its test executable SHA-256 is
`408d889ac759ef6712877bc1b8ef2c6c07364a960c44f2a1f4ad2a875600dfc7`;
its benchmark executable is
`a38fbe997ed6f53f3ede4a60f4417f821a81fdaff8ae1dd1b93e660551b3981b`.

The rebuilt WASM path passed in `70752.57799995597` ms with log SHA-256
`d959a6612cdf7e081bf8065d8561dbf6f5d7457d19c2d72ea840a1f4d1165096`.
The release module SHA-256 values are
`730f643fb284517dc0d46fcb3c04c915a355fd6d7af0204507a50f5313710f86`
for `poecraft_engine.mjs` and
`c178603af94faff6960144192682f60e557b521cf7866411a1cf6b8fc19e50a3`
for `poecraft_engine.wasm`. Existing `npm test` passed in
`12328.627999988385` ms, including engine smoke 27/27, solver corpus 4/4,
and the remaining existing web suites; its log SHA-256 is
`d54ec71473a8d47b1f6e205e248823a45bc940639135e262a19b3ae87283f1f6`.
`npx tsc --noEmit` passed in `2504.879100015387` ms with the empty-log SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
No rendered, screenshot, or browser visual review was performed.

The initial scoped read-only `doc-drift` audit at `042a281` found only the
expected stale sole-owner and pre-motion execution-state claims in the solver,
engine, active, direction, documentation map, and HANDOFF pages. It confirmed
that `docs/foundation/change-impact.md`, `docs/product/*.md`, and
`docs/mechanics/*.md` required no change. Those findings were corrected before
the final audit. `py -3 build/mechanical-split/link_audit.py` then scanned 136
owned Markdown files and checked 746 relative links: zero targets were missing
and zero non-template docs were unreachable from `docs/README.md`. It
explicitly excluded the concurrent untracked `docs/active/post-b6-recon.md`;
the passing report SHA-256 is
`42b5b3943231dc46607687069e47a504d5a97562e23c4b1597f5833c6c40b907`.
The final scoped read-only `doc-drift` rerun reported `CLEAN`: no stale live
sole-owner or pre-motion claim remained, and the ownership maps, lifecycle
links, archive wrapper, evidence, stopping point, and blocker were coherent.

## B1 accepted boundary

B1 implements ABI v2 bounded-result vocabulary, exact/bounded policy status and
termination separation, numerical-versus-product gap separation, atomic
same-round incumbents, complete bounded-policy stitching, target-gap stopping,
exact native/compiled policy evaluation and occupancy foundations, downstream
Python/WASM/TypeScript propagation, benchmark reporting, stable documentation,
and rebuilt WASM artifacts.

Targeted evidence on the committed implementation:

- `scripts/build.ps1` passed;
- `poecraft_engine_tests.exe --solver-solve-only` passed 391 checks with zero
  failures;
- the earlier ABI/API narrow run passed 45,717 checks with zero failures;
- all six changed benchmark manifests passed `--validate-only`;
- `npx tsc --noEmit` passed;
- `scripts/build-wasm.ps1` completed after the ABI and final native retention
  changes; and
- `git diff --check` passed before the implementation commit.

Oliver authorized exactly one superseding 1,800-second watchdog for the final
B1 oracle after the earlier 900-second deadline was shown to be miscalibrated.
The detached run completed with no survivor in 1,092,227.105 ms and wrote
`build/diagnostics/b1-two-t1-oracle-1800.json`. It passed every required fact:

- exact value `230.26738656962243`;
- 57,182 expanded states, 738,139 state-action rows, and 1,165,840 transitions;
- transition hash `ad4fc4865f2872e9` and policy hash `f797e61b00a127a7`;
- compiled graph 6,391 nodes / 9,607 edges and 2,668,693 strategy bytes; and
- constructive witness 3 syntheses / 323 reuses / 0 refreshes.

This is the re-pinned `60500ef`-lineage structural baseline. The tracked
`fixtures/solver-scaling/v1/evidence/q5-two-t1-product.json` remains the
historical `7b11b34`-era timing/evaluation report and must not be overwritten;
it is the comparison source below. Full exact-evaluation and 10,000-run evidence
is deferred to the single B6 acceptance cadence.

The retained witness refresh rule is: synthesize when absent; reuse while goal,
economy, the complete action prefix present at synthesis, referenced
rows/operators, executable dependencies, and properness remain valid.
Monotonic graph and lazy vocabulary extension do not invalidate the witness.
Refresh on an existing dependency change or validation failure, not on a mere
focused round, graph/vocabulary extension, or lower-bound update. Direct or
partial executable upper policies may still replace it atomically when cheaper.
Concrete policy references and Unveil preferences are materialized once from
captured same-round source data only when a bounded incumbent is returned.

## Open performance note and future profiling chunk

The new oracle is still roughly twenty times slower than the approximately
51-second `7b11b34` solve: `phase_wall_ms.solve` is 1,083.087 s versus 50.712 s,
a measured 21.36x ratio. This confirms a second, non-constructive per-round
regression source. It was not investigated during B2-B6. Do not investigate or
fix it unless Oliver selects the profiling chunk separately.

The comparison below is field-for-field in milliseconds. `—` means the timer
was absent or deliberately skipped, not zero. The old report's 50.712-second
solver phase is the like-for-like baseline; its 96.166-second report total also
contains 31.354 seconds of exact strategy evaluation, 6.882 seconds of
verification, and other report overhead. The new B1 run deliberately skipped
those post-solve gates under the intermediate testing cadence, so report totals
are not directly comparable even though their arithmetic delta is shown.

| Phase/time field | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `phase_wall_ms.registry_layout` | 17.976 | 17.922 | +0.055 |
| `phase_wall_ms.solve` | 1,083,087.349 | 50,712.495 | +1,032,374.855 |
| `phase_wall_ms.compile` | 788.223 | 1,163.060 | -374.837 |
| `phase_wall_ms.verification` | — | 6,881.602 | skipped |
| `phase_wall_ms.total` | 1,092,227.105 | 96,165.783 | +996,061.323 |
| `exact_strategy_evaluation.wall_ms` | — | 31,353.698 | skipped |
| `focused_expansion.duration_ns` | 841,222.378 | 28,777.684 | +812,444.694 |

`focused_expansion.duration_ns` is the implementation's cumulative focused
lower/upper optimization timer across rounds, despite the generic field name;
it is not a second independent wall clock.

| `solver_telemetry.timings_ns` field | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `registry_generation` | 0.136 | 0.140 | -0.004 |
| `abstract_layout` | 0.079 | 0.082 | -0.004 |
| `solve_setup` | 0.119 | 0.117 | +0.001 |
| `expansion` | 19,837.635 | 21,378.521 | -1,540.886 |
| `transition_calculation` | 7,136.128 | 8,517.387 | -1,381.259 |
| `optimization` | 4,842.530 | 148.657 | +4,693.873 |
| `constructive_policy` | 121,995.350 | — | new phase |
| `strict_clean_goal_cover` | 13,221.741 | — | new phase |
| `extraction` | 7,620.506 | 5,648.039 | +1,972.467 |
| `compilation` | 755.155 | 1,124.907 | -369.752 |
| `verification` | — | — | both null |

| Expansion phase/detail field | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `prepare` | 9,288.257 | 13,241.009 | -3,952.751 |
| `prepare.byte_audit` | 172.693 | 6.589 | +166.104 |
| `prepare.automatic_admission` | 8,733.396 | 13,003.213 | -4,269.817 |
| `prepare.diagnostics` | 249.690 | 205.003 | +44.687 |
| `prepare.operator_pricing` | 118.398 | 18.467 | +99.931 |
| `kernel` | 2,928.156 | 3,711.301 | -783.146 |
| `sparse_row` | 1,802.919 | 2,126.367 | -323.448 |
| `row_byte_audit` | 49.419 | 51.976 | -2.558 |
| `diagnostics` | 31.957 | 57.020 | -25.063 |
| `cache_release` | 116.388 | 84.383 | +32.005 |
| `periodic_cap_byte_audit` | 4,835.254 | 1,617.043 | +3,218.211 |
| `finalize` | 479.168 | 178.835 | +300.333 |
| `finalize_byte_audit` | 240.038 | 85.687 | +154.351 |
| `attributed` | 19,531.517 | 21,067.934 | -1,536.417 |
| `unattributed` | 306.118 | 310.587 | -4.468 |

Remaining non-zero telemetry timer leaves are also compared below. Timer leaves
not listed were exactly zero in both reports: all detail timers for
`fracture_prepare`, `multimod_finish`, `permanent_bench`, `protected_side`, and
`renewal`; the unused detail timers under `imprint`, `primitive_fracture`, and
`temporary_bench`; primitive-family `essence`, `bestiary`, and `other`; and
`bench.row_time_ns`.

| Other telemetry timer | New ms | `7b11b34` ms | Delta ms |
|---|---:|---:|---:|
| `automatic_candidates.admission_phases.local_context_ns` | 129.181 | 3,954.730 | -3,825.549 |
| `automatic_candidates.admission_phases.local_context_other_ns` | 88.737 | 1,221.376 | -1,132.639 |
| `automatic_candidates.admission_phases.local_layout_build_ns` | 14.046 | 930.162 | -916.116 |
| `automatic_candidates.admission_phases.local_ledger_init_ns` | 1.207 | 82.687 | -81.480 |
| `automatic_candidates.admission_phases.local_planner_build_ns` | 25.190 | 1,720.506 | -1,695.316 |
| `automatic_candidates.admission_phases.synthesis_ns` | 680.485 | 508.329 | +172.156 |
| `automatic_candidates.by_kind.imprint.time_ns` | 68.440 | 59.173 | +9.267 |
| `automatic_candidates.by_kind.primitive_fracture.row_time_ns` | 0.001 | 269.462 | -269.461 |
| `automatic_candidates.by_kind.temporary_bench.precompile_time_ns` | 2.422 | 2.475 | -0.053 |
| `automatic_candidates.by_kind.temporary_bench.enumeration_time_ns` | 457.271 | 146.425 | +310.846 |
| `automatic_candidates.by_kind.temporary_bench.row_time_ns` | 496.259 | 458.347 | +37.912 |
| `automatic_candidates.by_kind.temporary_bench.time_ns` | 6,763.699 | 6,825.972 | -62.273 |
| `cache.distribution.build_ns` | 7,136.128 | 8,517.387 | -1,381.259 |
| `cache.reforge.build_ns` | 354.748 | 316.299 | +38.449 |
| `cache.primitive_families.currency.time_ns` | 1,707.937 | 1,538.761 | +169.176 |
| `cache.primitive_families.currency.row_time_ns` | 2,532.422 | 2,355.020 | +177.402 |
| `cache.primitive_families.fossil.time_ns` | 649.577 | 761.499 | -111.923 |
| `cache.primitive_families.fossil.row_time_ns` | 1,430.655 | 1,589.652 | -158.997 |
| `cache.primitive_families.harvest.time_ns` | 209.453 | 940.185 | -730.733 |
| `cache.primitive_families.harvest.row_time_ns` | 445.470 | 1,352.565 | -907.095 |
| `cache.primitive_families.bench.time_ns` | 4.410 | 6.018 | -1.608 |
| `cache.primitive_families.fracture.time_ns` | 37.398 | 133.733 | -96.335 |
| `cache.primitive_families.fracture.row_time_ns` | 0.001 | 269.462 | -269.461 |
| `diagnostic_cost.calc_owned_byte_audit_ns` | 4,934.798 | 1,485.834 | +3,448.964 |
| `diagnostic_cost.calc_owned_byte_ledger_ns` | 8,503.782 | 9.087 | +8,494.695 |

The top three phase deltas by direct reported wall contribution, and therefore
the exact future profiling order, are:

1. cumulative focused lower/upper optimization:
   `focused_expansion.duration_ns`, +812,444.694 ms;
2. retained constructive-policy acquisition/validation:
   `timings_ns.constructive_policy`, 121,995.350 ms (the phase did not exist in
   the `7b11b34` code, so this is a new wall contribution rather than a
   same-field regression); and
3. strict clean-goal-cover preparation:
   `timings_ns.strict_clean_goal_cover`, 13,221.741 ms (also new since the old
   report).

The first item is the confirmed non-constructive per-round regression source.
The second and third overlap the same overall solve but are separately timed;
do not add phase totals as though they were independent report wall clocks.
Byte-ledger and audit growth are supporting profiling leads, not additions to
the owner-selected top-three scope. No performance fix was attempted after the
B1 gate passed.

## B2 accepted boundary

B2 compiles exact or bounded native policies solely when `policy_available`.
Calculator now offers optional absolute-chaos and relative-percent targets and
passes only positive values to the engine. Live progress shows lower/upper
bounds, absolute gap, and factor. Final result markup separately identifies:

- exact evaluated returned-policy cost;
- optimal-cost lower bound and certified policy upper bound;
- absolute gap and conservatively rounded multiplicative factor;
- exact, bounded-near-optimal, bounded-feasible, or no-policy quality;
- exact close, target gap, cap (including named cap hits), or no-policy
  termination;
- requested and fired target criteria;
- pinned economy; and
- the complete admitted priced action ID list plus exclusions/skips.

The DOM contract uses the approved “Certified within 1.10x of optimal” and “At
most 10% more expensive than optimal” forms, states that a weak lower bound can
make the certificate pessimistic, and never calls a bounded policy exact or
calls its upper bound the optimum. Bounded policies can open in Strategy Board
and use the existing evaluator/simulator surface when compilation succeeds.
The product verification button now requests 10,000 runs and labels simulation
as corroboration rather than the authoritative evaluation.

B2 evidence:

- `npx tsx test/solver-result-presentation.test.ts` passed all five focused
  DOM/predicate/target cases;
- `npx tsc --noEmit` passed; and
- `git diff --check` passed before commit `86e337f`.

No rendered or visual review was performed, per Oliver's ownership boundary.

## B3 accepted boundary

B3 adds the dedicated native `pc_solver_goal_feasibility` three-way query and
Python binding. A `feasible` result requires an engine-resolved, item-level and
positive-base-weight natural T1 assignment that satisfies rare-item side
capacity and every exclusion group, plus an admitted legal ordinary reforge
witness from the supplied empty normal/rare start. Structural lack of natural
T1 weight, side capacity, or compatible groups returns `infeasible` with a
reason. Unsupported starts or missing admitted witnesses return `unknown`.
The query never uses capped solving as evidence.

The seeded generator supports explicit base paths, base-list files, item
classes, and named base pools; item level and start item; one-to-four natural
T1 goals with exact prefix/suffix composition; family/tag filters; cases per
stratum; seed; solver resource caps; and generator/case watchdogs. Python only
proposes base-reach T1 families for the initial uninfluenced corpus. Native
feasibility remains the acceptance authority. Emitted goals use acquisition-
aware family keys, so bench/crafted members cannot satisfy them; priced bench
actions remain in the goal-relevant envelope.

Pinned corpus evidence in `fixtures/solver-natural-t1/v1`:

- 14 smoke, 120 full short-budget, and 12 deep cases (146 total) across six
  bases/classes: Body Armour, Helmet, Bow, Thrusting One Hand Sword, Amulet,
  and Ring;
- natural goal counts 1-4, varied prefix/suffix mixes, and deep cases with at
  least 100 natural pool modifiers and minimum single-draw goal probability
  no greater than 0.01;
- the smoke corpus includes the bounded three-natural-T1 Dire Pelt with
  `ItemFoundRarityIncreasePrefix3`, `IncreasedLife9`, and `ChaosResist6`;
- all 146 cases are native `feasible/natural_reforge_witness`, all resolved
  goal mods are base-reach, family tier 1, positive weight, and none is a
  bench/crafted goal;
- the generation funnel records 153 attempts, 146 accepts, 2 duplicates, 1
  deliberate ineligible-base probe, 3 deliberate infeasible probes, 1
  deliberate unknown probe, 1 zero-weight family, 2 group/capacity conflicts,
  and 0 exhausted strata; and
- full regeneration was byte-for-byte deterministic.

Generation provenance pins clean engine commit
`4c99cd093ed85b9aa2ecb86c3c9117b0f356068b`, GCC 14.2.0, ABI 2, native binary
SHA-256 `ee8d3d97955e864b1018dd41b1f87891b1b4952aa8fa076c331dbf589539c89d`,
artifact manifest SHA-256
`6b8bbfe77abf87373ec1782fb364500e064eb7539f1d3f0fdb210a807c927875`,
config SHA-256
`6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`,
and raw economy snapshot SHA-256
`9cae91c13f2c8a6bb06fe0d22487cfc77ca44983817a221de131e5fc3e72cb0e`.
B4 run records must separately pin the solver build so A/B runs consume these
identical explicit case files.

B3 targeted evidence:

- `scripts/build.ps1` completed after the native C ABI and test changes;
- `poecraft_engine_tests.exe --solver-feasibility-only
  data/compiled/current` passed 37 checks with zero failures;
- `py -3 -m unittest tools.ingest.tests.test_natural_t1_corpus -v` passed all
  three tests, including byte determinism and native natural/bench/group
  semantics;
- the configured 146-case dry run and committed regeneration both completed
  under the generator watchdog; the final run took about 11 seconds;
- `scripts/build-wasm.ps1` rebuilt the tracked WASM after the C ABI addition;
- a whole-corpus validation checked unique IDs, schemas, strata counts,
  goal-count consistency, T1/base-reach/positive-weight provenance, native
  feasibility, and all six intended classes; and
- `git diff --check` passed before the B3 commits.

No solver benchmark, exact evaluation, or 10,000-run simulation was executed
in B3; those remain staged for B4-B6 under the selected cadence.

## B4 accepted boundary

B4 adds a native natural-corpus benchmark contract and
`tools/ingest/benchmark_solver_corpus.py`. The runner accepts arbitrary corpus,
artifact, executable, and output paths; orders cases deterministically; runs
each case in an isolated process group; applies the case watchdog under the
default 900-second ceiling; kills the process tree on expiry; checks for a
surviving parent; writes an atomic resumable ledger; skips completed reports;
and supports opt-in worker concurrency constrained by each case's declared
solver-owned-byte cap. Default hard-case concurrency is one. Run provenance
separately records source commit/dirty paths and native executable SHA-256, so
it is independent of the B3 generation-engine pin.

The stepped C/WASM progress surface now reports focused round and incumbent
kind, discovered/expanded/frontier states, sparse rows/transitions/reforge
work, and live/peak selected owned bytes in addition to `L`, `U`, and both
gaps. The native report samples this at every round/incumbent change and
bounded wall intervals, calculates per-cap proximity, flags whether each
sample raised `L` or lowered `U`, and derives time to first incumbent plus
standard absolute/relative gap thresholds.

Exact evaluator action utility is sourced from retained exact state/action
occupancy. Per action it reports exact visits/applies, priced expected spend
and known-cost share, distinct reachable states, and regions stratified by
goal progress, rarity, blocker count, crafted count, fractured goal subset,
and fractured count. Probability of any use is emitted only where the retained
evidence proves it (`exact_zero`); cyclic occupancy is explicitly reported as
insufficient rather than mislabeled as a hitting probability. Simulator action
distributions remain separate empirical corroboration.

Solver telemetry attributes row attempts, raw outcomes, retained transitions,
reforge work, cache requests/hits, row-construction wall time, and retained
selected-allocation growth by action ID. A lower-versus-executable-upper policy
section compares selected abstract-state counts by action. The telemetry marks
all search-cost data observational and records
`non_use_is_pruning_certificate:false`; benchmark action non-use never changes
admission or pruning.

B4 targeted evidence:

- `scripts/build.ps1` completed after native/header/test changes;
- `py -3 -m pytest tools/ingest/tests/test_solver_corpus_runner.py -q`
  passed four watchdog, resume, deterministic-order, and memory-budget tests;
- the historical 12-case benchmark manifest and all 146 generated natural-T1
  cases passed native `--validate-only`;
- the clean committed smoke run at
  `build/reports/b4-boundary-smoke/ledger.json` pins commit
  `1d891a01ae3cfce84fbefb08ba17e3a0e6df9f1f`, a clean tree, and executable
  SHA-256 `50add1869715e45276bcad914fbff60df76f5e9e144a6513fb15134629f56141`;
- that isolated one-natural-T1 case completed in 4,471.660 ms as
  `bounded_near_optimal` / `target_gap`, compiled, completed exact priced
  evaluation, produced 16 bound samples with first incumbent at 820.862 ms,
  reported utility for six policy actions and search cost for 176 considered
  action IDs, and left zero survivors;
- repeating the same run directory skipped the completed case through the
  ledger resume path;
- `npx tsc --noEmit` passed after the additive progress contract; and
- `scripts/build-wasm.ps1` rebuilt the tracked release module, followed by a
  clean `git diff --check` before commit `1d891a0`.

No full native/web suite and no 10,000-run simulation was executed in B4; both
remain reserved for B6.

## B5 accepted boundary

B5 adds pure-Python stratified aggregation and paired comparison over the raw
per-case B4 reports. Reports group by base, class, base/class, natural-T1
count, prefix/suffix side mix, natural-pool density, minimum goal probability,
incumbent kind, policy status, and termination. They report exclusive
exact/near-optimal/feasible/no-policy rates, refusal and target-reach rates,
median/p90/p99 time and selected-owned memory, time to first incumbent and
target, lower/upper progress, work and compiled-graph distributions, exact
policy action utility, observational action search cost, outliers, and paired
deltas/regressions.

Paired comparison accepts a case only when its ID and complete
session/start/goal/caps inputs match. Search-cost output repeats that action
non-use is not a pruning certificate. Analytics remain separate from native
and WASM correctness; they do not change solver admission, pruning, Bellman
comparisons, tie-breaking, epsilon, or gap stopping.

`fixtures/solver-natural-t1/v1/benchmark-stages.json` and
`tools/ingest/benchmark_bounded_policy_stage.py` encode the ordered funnel:
smoke, full short-budget candidate, deep representative/hard cases, explicit
compiled-policy exact evaluation, and the B6-only acceptance-verification
subset. Each non-smoke stage requires a green predecessor ledger under the
same label. The acceptance stage is refused by the B5 command so the required
10,000-run cadence cannot be spent early.

Native benchmark exit code 2 is treated as a completed measurement when the
process ended under its watchdog, left no survivor, and wrote its report. Its
case-level expectation miss remains explicit in the ledger and report so
no-policy and refusal outcomes are not censored. Exit code 1, timeout,
survivor, missing report, runner error, or memory-budget launch refusal remains
an incomplete run.

B5 targeted evidence:

- the native benchmark rebuilt successfully after adding generated corpus,
  feasibility, and generation dimensions to each raw case input;
- `py -3 -m pytest tools/ingest/tests/test_solver_reports.py
  tools/ingest/tests/test_solver_corpus_runner.py -q` passed all 10 focused
  analytics, paired-input, stage-gate, watchdog, resume, native-exit, ordering,
  and memory-budget tests;
- the clean committed two-case smoke at
  `build/reports/bounded-policy/b5-boundary/smoke/ledger.json` pins source
  commit `e40b73db00716edf7ad3ed53ec36eec51678cc49`, a clean tree, and executable
  SHA-256 `e09c2f0d5bef91430773bba949a0645963b3d8f81124a72511dee5f2937f46e3`;
- its one-natural-T1 case completed as `bounded_near_optimal` / `target_gap`
  in 4,721.542 ms, while the three-natural-T1 Dire Pelt completed as an honest
  `refused_state_cap` / no-policy measurement in 506.763 ms; the aggregate
  therefore retained 50% near-optimal, no-policy, refusal, and target-reach
  rates and recorded zero survivors; and
- a paired self-comparison covered both identical-input cases with zero
  exclusions and zero regressions.

No full-short or deep corpus, exact-evaluation stage, complete native/web
suite, or 10,000-run simulation was executed in B5. Those runs remain staged
for owner-selected iteration and the single B6 acceptance cadence.

## B6 accepted boundary

B6 completed the final downstream acceptance and documentation gate. During
the pass, the historical WASM smoke fixture was corrected to request its
crafted bench family instead of letting a broad natural group stand in for a
bench goal, and both stale ABI assertions were updated to ABI v2. That exposed
a real state-local admission defect: permanent-bench discovery could reject a
legal finish when a temporary abstract context could not rematerialize the
already-existing dense exact carrier. Permanent deterministic benches now
evaluate directly on that materialized carrier, intern their exact successor
in the owning context, and cache the exact distribution. This restores legal
output behavior without changing mechanic authority, admission criteria,
Bellman comparisons, search guidance, or pruning.

The single final B1 two-natural-T1 oracle was not rerun. Oliver's exactly-once
directive and the standing 15-minute ceiling make the retained report the B6
oracle evidence. B6 revalidated report SHA-256
`9501b055aa1808bc17aa3213f001daf134c2bfc2f621ab2867c90d81a0c061da`
and all acceptance facts: exact value `230.26738656962243`, 57,182 expanded
states, 738,139 rows, 1,165,840 transitions, `exact` / `exact_closed`, graph
6,391 nodes / 9,607 edges / 2,668,693 bytes, and constructive-witness counters
3 syntheses / 323 reuses / 0 refreshes. Its measured wall time remains
1,092,227.105 ms; the open comparison and owner-selected profiling order are
recorded above.

The explicit bounded acceptance case was
`natural-t1-smoke-one-armour-09767fd6f824`. It completed under the detached
900-second watchdog as `bounded_near_optimal` / `target_gap`, with target met,
`L = 3.0923164022485841`, `U = 8.0902955687919231`, absolute gap
`4.9979791665433391`, and relative gap `1.6162573671015839`. Its compiled graph
had 947 nodes / 2,831 edges / 1,154,938 bytes. Exact compiled-policy evaluation
converged to `8.0902955687899443`, differing from certified `U` by
`-1.9788e-12`. The required simulator run then completed exactly 10,000 trials:
10,000 successes, zero failures, zero off-policy failures, mean cost
`8.0376181789999475`, and passed corroboration. This is a bounded certificate,
not an exact-optimality claim.

B6 gate evidence:

- the post-fix native build completed with exit 0 in 218,771.894 ms under the
  detached watchdog;
- the complete native suite covered capped `bounded_feasible` / resource-cap
  termination, early `bounded_near_optimal` / target-gap termination, and
  native-versus-compiled exact-value parity as part of 513,211 checks with zero
  failures;
- the final `scripts/test.ps1` cross-layer pass completed with exit 0 and no
  watchdog survivor in 46,196.463 ms, validated all 12 historical solver
  specifications, and passed ingest, binding, native, and complete web/WASM
  tests;
- `test_natural_t1_corpus.py`, `test_solver_corpus_runner.py`, and
  `test_solver_reports.py` passed all 13 determinism, feasibility,
  watchdog/resume/survivor, aggregation, and paired-comparison tests;
- `npx tsc --noEmit` passed;
- `scripts/build-wasm.ps1` completed with exit 0 in 74,349.823 ms after the
  engine fix, producing tracked release `.wasm` SHA-256
  `b70d3cf05c0ae9a2a2429bffbde9c279b24b74cad3d670a48090ec6a5b5b77c6`;
  and
- the checked-in
  [B6 acceptance summary](fixtures/solver-natural-t1/v1/evidence/b6-acceptance-summary.json)
  validates as JSON, `git diff --check` passed, and the documentation
  relative-link/reachability audit passed after the plan archive move.

Initial acceptance attempts exposed the two stale web assumptions and the
materialized-carrier defect above; those were debugged before the final green
pipeline. No performance profiling or mechanic change was attempted. No
rendered or screenshot review was performed, per Oliver's ownership boundary.

## Exact stopping point

Chunks completed with evidence: B1, B2, B3, B4, B5, B6, and the mechanical
solver split. Oliver selected the focused-round performance plan on
2026-07-23.

Exact stopping point: before Gate 0 boundary verification, the fresh
uninstrumented native build, input derivation, fast native counts, or any
benchmark repetition. No attribution instrumentation, copied-source variant,
candidate default, test, fixture, evidence, binding, WASM, or web change has
started. The selected source baseline is `042a281`; the preceding
documentation boundary is `5eb66d4`.

The accepted source has exactly nine `solver_solve*.cpp` files and the private
`solver_solve_types.hpp` header. Calculator, `solver_internal.hpp`, and
engine-smoke test splitting remain deferred. The selected plan is limited to
fast-case attribution and at most one predeclared focused scheduling-default
tuple. The exact natural two-T1 oracle must not run. The archived
reconnaissance is context only and cannot replace fresh evidence.

There are no blockers at selection. The next session must first verify a clean
selection commit and prove there is no source change after `042a281`, then
execute Gates 0 through 6 in order. Every build, benchmark, evaluation,
simulation, and test-pipeline process retains the detached 900-second
watchdog. The documentation-only selection passed `git diff --check` and a
Markdown audit covering 139 files and 823 relative links with zero missing
targets or unreachable non-template docs; the audit report SHA-256 was
`c8b30049c073ab4429dae421d298f891b1324c3558dc1f99a98ef8becd66d51d`.

## Economy pipeline state (2026-07-22, outside chunk scope)

Known product gap: the Calculator cannot price `harvest_reforge:defences`.
The engine's action id is plural (from `fixtures/economy/harvest-recipes-v1.json`
and the game's `defences` tag), but every published snapshot carries the stale
singular key `harvest_reforge:defence`, seeded before the S7.2R fixture
correction and never re-seeded. `defences` is the only harvest tag whose
spelling differs between the two vocabularies, and no pinned case admits the
defence reforge, so no test observes this gap in either direction. A
from-scratch recipe seed was verified to produce the correct plural
`derived_recipe` keys with zero code changes.

Incident: on 2026-07-22, during diagnosis, `poecraft-economy ingest-fixture
--force` rebuilt the untracked local `data/economy/poecraft-economy.db` from
frozen test fixtures. The 2026-07-15 live ingest history and quote rows were
lost from the database. The content-addressed live raw payloads in
`data/economy/raw/poe-ninja/` survive, and every committed snapshot artifact
and `apps/web/public/economy/league-index.json` is unaffected.

Do not run `poecraft-economy publish` in this state: the database's compiled
snapshots (including mirage `c33c84de…`) are derived from test-fixture prices,
not market data, and publishing them would put fake prices in the product
league index. Publish only after the database is rebuilt from live data — a
fresh `refresh`, or a reconstruction of the July 15 fixture from the surviving
raws — under a separate owner-selected step. That step changes tracked files
(a new snapshot JSON plus the league index) and must run between chunks, not
during one.
