# Released-Candidate Reclamation v1 — Execution Log

Parent: [Plan](README.md)

## 2026-08-30 — boundary selection and causal preflight

- Preflight: branch `main`, HEAD
  `f85e026db160d28e5f300d21c34d2e4dbe4e0bf4`; worktree clean except protected
  untracked `0`, which was not read or modified.
- Rebuilt retained native sources with `powershell -File scripts/build.ps1`.
- Ran immutable clean-five revision
  `case-rev-54343c2296afe4f624f622c16779498c` once under the disabled
  benchmark-private resumable diagnostic. Output:
  `build/qualification/resumable-joint-policy-nonexclusive-service-v1/clean5-diagnostic-before/report.json`.
- Result: bounded feasible at the natural 120-second finish but only the
  primitive fallback (`470485191.442781` cost/actions; 5 nodes, 6 edges).
  Candidate `798391a94c01cd51` captured once, resumed twice, yielded three
  times on states `f14cffc5f1fa892f`, `47ddc51f96e5b172`, and
  `7012dd19217f7da9`, then refused `resource_interrupted`. Its retained work was
  103,987 and its still-live payload was 304,701,928 bytes, above the explicit
  268,435,456-byte candidate cap. It retained 1,210 fixed decisions at cursor
  103,984; source/target generations grew from 3,773/4,140 to 58,040/14,054.
- The same run recorded only 4 ordinary joint-policy attempts and 2 ordinary
  missing-frontier services. Source inspection confirmed that
  `try_install_reachable_incumbent` returns immediately after either candidate
  yield or refusal, while `release()` retains every owned vector. Direction:
  repair reclamation and non-exclusive service before designing new admission
  logic.

## 2026-08-30 — scope correction from PDR

- The first repair made both active yield and terminal refusal fall through to
  an ordinary joint-policy attempt. Matched clean-five recovered the exact
  qualified baseline: `85408.64362148782` cost, `8259.468210528557` expected
  actions, success 1, off-policy mass 0, 775 nodes/1,634 edges, 24 ordinary
  attempts, and 301,454,033 peak native-owned bytes. The refused candidate's
  live payload fell from 304,701,928 bytes to 2,312 bytes while retaining its
  304,702,272-byte peak diagnostic.
- PDR rejected the broader fallthrough rule. Candidate
  `51d67b3219b70c43` was over-serviced to completion after four resumes, and
  repeated same-checkpoint ordinary attempts prevented exact closure before
  the 300-second watchdog (`65,076` rows, `7,133` states, `6,175` expanded).
  A follow-up that deferred complete-candidate evaluation reproduced the same
  expensive plateau and was stopped early rather than allowed to duplicate the
  watchdog result.
- Scope is therefore narrowed to the observed defect: active `Yielded` keeps
  its original one-attempt preference; `Released` reclaims payload and falls
  through to ordinary work. The temporary complete-candidate deferral was
  removed.

## 2026-08-30 — narrowed private primary

- Focused fixture after the narrowed implementation: 64 checks, zero failures.
- The final matched clean-five private run is
  `build/qualification/resumable-joint-policy-nonexclusive-service-v1/clean5-diagnostic-release-only/report.json`.
  It reached the natural requested bounded finish with the exact qualified
  ordinary result: cost `85408.64362148782`, expected actions
  `8259.468210528557`, success 1, off-policy mass 0, and a 775-node/1,634-edge
  compiled graph. It made 23 ordinary joint-policy attempts and completed 22
  missing-frontier services.
- Candidate `798391a94c01cd51` captured once, resumed twice, and yielded three
  times before the existing 256 MiB cap refused it as
  `resource_interrupted`. Compact diagnostic storage retained the 1,210 fixed
  decisions, cursor 103,984, and 304,701,944-byte peak while live candidate
  storage fell to 1,984 bytes. Normal native live/peak ownership returned to
  212,026,024/301,437,188 bytes, matching the ordinary control class.
- The final code does not alter the active PDR path: `Yielded` still records
  its named obligation and returns exactly as in the retained passing witness;
  only `Refused` reclaims and falls through. The next evidence is therefore
  production accounting rather than a duplicate private PDR run.

## 2026-08-30 — production activation and primaries

- Checkpoint `fdfbd7c831b837ea6b6d7946aba26c3166f70416` retained the narrow
  release repair. Production activation reuses the existing high-impact
  executable-upper profile; candidate bytes are cached on mutation, included
  in fast/full ownership, and reconciled against an audited recomputation.
- PDR report `build/qualification/resumable-joint-policy-nonexclusive-service-v1/pdr-production/report.json`,
  SHA-256 `d857aac1fc5ade274b0db6cfbc386f91ea80e17cc7678baa18b525c8a220cd8f`:
  exact closed at cost `3758.1244272552067`, expected actions
  `8608.877131574318`, success 1, off-policy mass 0, complete reconciled
  pricing, 7,213 expanded states, three joint-policy attempts/services, and a
  312-node/881-edge graph. Strategy SHA-256 is
  `c5ddf81a73eeec532a3efdbcbe661216942c32464ac51127401bd657b3aa1597`.
- Clean-five report `build/qualification/resumable-joint-policy-nonexclusive-service-v1/clean5-production/report.json`,
  SHA-256 `616465078b9a444c5dc967997ddce92ef32bca2be0d17f1068a7d944cc60eaf8`:
  natural bounded finish at exact cost `85408.64362148782`, expected actions
  `8259.468210528557`, success 1, off-policy mass 0, complete reconciled
  pricing, 6,329 expanded states, 23 attempts, 22 services, and a
  775-node/1,634-edge graph. Strategy SHA-256 is
  `9e8687ac1f1de705cd1bef59e5269395190e6b8d2134d26d0cf1aac2468717b1`.

## 2026-08-30 — secondary controls and rejected strict closure

- Partial four-to-five report
  `build/qualification/resumable-joint-policy-nonexclusive-service-v1/controls/partial-four-to-five-production-report.json`,
  SHA-256 `b823d94cbba3ab1ba9a61f0fe203062684a87e5c80b5ad49c35c8ac6efb5218c`,
  strategy SHA-256
  `b5bebe6edcea2fd1badee8f83f57b287c06f7c3df9200fa6aa439b2c9f53f432`:
  exact fallback cost/actions `470485192.50357205`, success 1, off-policy
  mass 0, complete reconciled pricing, and all caps passed. This exactly
  matches the selected boundary's `b690cad` current-main control and therefore
  has zero regression. The much better older 7.896M result is separate
  pre-existing quality debt; this boundary neither caused nor repaired it.
- Non-armour report
  `build/qualification/resumable-joint-policy-nonexclusive-service-v1/controls/non-armour-four-goal-production-report.json`,
  SHA-256 `78fb130390e73ef803c85351f2b061d2577082fd2f35a79c0f5d0148af5d1a59`,
  strategy SHA-256
  `029c1379ef661edb687c993274cea8427e90fca30bbb1a78aa040aca4a7edbf5`:
  the state cap was the named stop while the exact proper incumbent remained
  available at cost `223349.0000393144`, expected actions
  `1404492.0069683318`, success 1, off-policy mass 0, complete reconciled
  pricing, and all caps passed. It exactly matches current main.
- A strict-publication continuation experiment was run only after the partial
  result was initially compared to the wrong historical baseline. After an
  experimental pending-walk dedup repair, it serviced 233 missing states but
  still retained a 1,100-entry cursor and 1,860-entry walk before naming the
  234th missing state. Evidence is
  `controls/partial-four-to-five-strict-continuation-r6-report.json` under the
  same qualification directory. It proved strict closure is a large competing
  drain. All experimental strict-publication source was removed.

## 2026-08-30 — final acceptance

- Rebuilt WASM passed. Only existing Emscripten tautological-range warnings
  were emitted.
- Native CTest passed 17/17 in 117.33 seconds. Web tests passed 11/11 and
  `npx tsc --noEmit` passed. Logs are `final-native-ctest.log`,
  `final-web-tests.log`, and `final-web-typecheck.log` in the qualification
  directory. No visual review was performed.
- The immutable PDR case's 300-second whole-case watchdog cannot currently
  contain its exact solve plus a 10,000-run override. Two incomplete attempts
  ended at 9,344 and 9,984 successful trials with zero failures/off-policy
  events and the exact retained strategy SHA-256 `c5ddf81a...1597`. Neither is
  claimed as a pass.
- The existing Python native binding compiled that saved hash-identical
  strategy under the frozen session/economy/caps and completed 10,000/10,000
  successes with no terminal, limit, missing-edge, unapplied-action, missing-
  price, or cost-accounting failure. Structured evidence:
  `pdr-production-verification-10000-r2/saved-strategy-simulation-10000.json`,
  SHA-256 `4a8aebb973aec2a30e1d31baa691c786bb624b797ffedd1550dbdeff671f0479`.
  This run was unnecessary because the identical artifact was already
  qualified under the same semantics; it is recorded for honesty, not used to
  justify extra confidence.
- Oliver changed future compiled-strategy simulation to 1,000 trials when
  genuinely required, and none for an identical already-qualified artifact.
- `git diff --check` passed. The complete repository pipeline was not run; the
  affected native and rebuilt-WASM web layers were run instead.
