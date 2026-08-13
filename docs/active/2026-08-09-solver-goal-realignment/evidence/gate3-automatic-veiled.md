# Gate 3 Automatic Veiled Evidence

**Status: complete on 2026-08-13.**

Parent: [Solver Goal Realignment plan](../plan.md)

## Owner ruling and proof boundary

Oliver selected acquisition-time offer generation and the best legal exact
cleanup/retry continuation when an offer set contains no goal modifier. The
automatic grammar is consequently limited to:

`optional relevant pre-cleanup -> Veiled Chaos or Veiled Exalt -> immediate observed Unveil`

The Simulator persists the three offers at acquisition. The exact solver
enumerates the offer distribution at observation, but no intervening action can
change the carrier, side, groups, or weights in this grammar, so the two paths
are distribution-equivalent. Post-acquisition blockers are excluded. Every
legal offered modifier remains in the observed choice group, and Bellman values
select both the best direct goal offer and the best non-goal continuation.

## Product-path coverage

- Veiled Chaos and Veiled Exalt are dependency-only carrier-local candidates;
  neither becomes a broad standalone product action.
- Candidate generation requires a rare carrier without a placeholder and at
  least one unveiled modifier capable of satisfying an unmet goal.
- Relevant removal of a non-goal crafted modifier is admitted before
  acquisition only.
- Both acquisition prices are checked independently. Missing prices remain
  explicit non-admissions.
- The automatic family has distinct `veiled` candidate, telemetry, and
  preservation-witness identities with the `acquisition_time_offer` mechanism.
- A forced Veiled Exalt winner compiles through `has_unveil_option`, exact
  evaluation reconciles to the solver value, and sampled execution consumes
  the persisted offers.

## Focused verification

All commands ran from the repository root against the native Release build.

| Command | Result |
| --- | --- |
| `powershell -File scripts/build.ps1` | passed |
| `poecraft_engine_tests.exe --solver-automatic-veiled-only` | 59 checks, 0 failures; exact cost `1.25`; 10,000/10,000 sampled successes |
| `poecraft_engine_tests.exe --solver-s8-3-only` | 525 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-solve-only` | 6,672 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-compile-only data/compiled/current` | 804 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-eval-only` | 1,174 checks, 0 failures |
| `poecraft_engine_tests.exe --solver-family-contract-only data/compiled/current` | 182,666 checks, 0 failures |
| `npx tsc --noEmit` in `apps/web` | passed |
| `git diff --check` | passed |

The deliberately tiny automatic-admission owned-byte regression accepts either
of the API's existing exact limit witnesses: an internally returned deferred
batch or the cooperative outer continuation's `SolverResourceLimit`. Adding a
telemetry slot changed only which suspension observes the cap; rollback and
retry behavior remain covered by the Solve continuation tests.

Gate 8 owns the final release-WASM build, full repository acceptance, selected
cross-runtime verification, semantic comparison, and portfolio run.
