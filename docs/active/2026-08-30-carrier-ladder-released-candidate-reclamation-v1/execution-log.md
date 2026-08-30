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
