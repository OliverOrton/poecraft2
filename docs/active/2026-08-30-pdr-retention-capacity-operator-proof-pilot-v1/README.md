# PDR Retention-Capacity Operator-Proof Pilot v1

**Status: active implementation boundary.** Selected on 2026-08-30 after the
production resumable-candidate mechanism qualified. Work is local-only.

Parent: [Active work](../README.md)

## Objective

Test the smallest proof-only retention/capacity abstraction that can directly
strengthen an existing action-specific lower consumer and retire live strict
PDR obligations before they consume the current proof/quotient memory budget.

This is not a general RCASSP implementation, root-lower vanity experiment, new
planner, scheduler change, or executable-policy generator.

## Starting Evidence

- Production PDR now closes exact at `3758.1244272552067` Chaos with a stable
  executable upper and strategy SHA-256
  `c5ddf81a73eeec532a3efdbcbe661216942c32464ac51127401bd657b3aa1597`.
- The existing typed proof manager, strict obligation ledger, action-envelope
  retirement authority, terminal debt, and action-specific proof patterns are
  the only permitted consumers. No parallel pruning or lifecycle system is
  allowed.
- Prior strict-proof work identified proof/quotient memory as the live PDR
  boundary, but its coarse checkpoint omitted scheduler state. This pilot uses
  ordinary fresh execution and does not build replay first.
- The verified fragment lane remains isolated and parked. It is neither an
  input nor a consumer for this boundary.

## Shadow Contract

Begin with one or two measured live PDR carrier/action obligation shapes whose
current lower remains competitive against the verified upper.

The first pattern must:

- retain actual prefix/suffix occupancy and capacity;
- retain exact identity only for the smallest selected goal subset needed by
  that obligation;
- retain source-authoritative blocker, protection, and destructive-survival
  facts only for its selected action family;
- build exact local pushforward rows through existing mechanics authority;
- use an action-complete existing fallback for every unsupported operator;
- prove concrete-to-abstract and terminal inclusion, normalized complete mass,
  action-local pushforward parity, and a Bellman subsolution;
- own no more than 16 MiB retained bytes and report transient bytes separately;
- report stable pattern/source/scope identities, direct obligation-retirement
  count, and RC-attributable strict-state growth.

Shadow values have no pruning, retirement, scheduling, incumbent, executable
policy, mechanic, probability, action-vocabulary, or publication authority.

## Work Plan

1. Read bounded projections of the retained PDR reports and trace current
   strict obligation creation, action-specific lower lookup, proof-pattern
   selection, quotient growth, retirement, and byte accounting. Name one or
   two dominant live source/action shapes before writing an abstraction.
2. Add the smallest internal pattern/state/identity representation and a
   mechanics-independent focused fixture for abstraction, terminal inclusion,
   exact local rows, mass, subsolution, stable repeat identity, and retained /
   transient byte accounting.
3. Connect the pattern to fresh PDR telemetry in shadow only. Measure which
   existing live obligations would consume the value, how many would retire,
   and strict-state growth attributable to the pattern. Do not run a long PDR
   solve until a changed shadow pattern needs that evidence.
4. Stop diagnosis-only if the cheap action still pins the lower, no live
   obligation consumes the value, local rows reproduce strict state one-for-
   one, retained bytes exceed plausible proof bytes avoided, or required
   probability/signature coverage cannot be proved.
5. Consider production consumption only if shadow evidence directly retires
   at least one obligation, at least two distinct source/action shapes consume
   the bound, soundness/mass/terminal/identity checks pass, RC-attributable
   strict-state growth is below 5%, and retained ownership remains within
   16 MiB. Promotion must use the existing typed proof manager and retirement
   authority.

## Verification Discipline

Use focused abstraction/calc/proof tests while implementing. Do not run
Simulator for proof-only changes. If a materially changed compiled strategy is
eventually retained, the current repository rule is 1,000 trials once; an
identical already-qualified artifact needs none. Run broad acceptance only if
a retained production consumer crosses enough layers to require it.

## Work Log

- 2026-08-30: prerequisite Outcome A accepted at production commit
  `2f72aa8`. Activated this shadow-first pilot. Source/report attribution is the
  first live task; no abstraction or consumer is presumed yet.
- 2026-08-30: bounded production-report inspection found 213,532 typed ledger
  entries: 150,552 exact rows and 62,980 exact inapplicabilities, with zero
  `IndependentGlobalLowerVsVerifiedUpper` retirements. Every operator-lower
  family recorded zero evaluations; the concrete skip owner is
  `no_finite_incumbent` (including 72,170 currency, 14,434 harvest, 7,217
  fracture, 11,675 temporary-Bench, and 25,900 Eldritch-side observations).
  The proof manager selected `carrier_mdp` 372,980,457 times and `clean_mdp`
  34,250,919 times. The retained strict-compatibility sample is concentrated
  in Fracture, Exalt, and Annul, but it contains no verified-upper margins.
- Source tracing explains the gap: pre-materialization retirement consumes
  only `incremental_certified_upper_values`, populated from an early stable
  restricted policy. The new independently evaluated resumable PDR incumbent
  arrives through `commit_output_incumbent` after the ledger has already
  materialized its rows; that commit does not revisit the operator-proof
  consumer even though the incumbent owns aligned per-state values.
- Decision: do not choose or build an RC pattern yet. First add a bounded,
  no-authority audit at verified-incumbent installation. It will evaluate the
  existing operator lower against retained ledger state/action pairs and the
  incumbent's certified per-state values, report would-retire counts and the
  closest still-competitive shapes by family/goal subset/side capacity, and
  leave every ledger lifecycle unchanged. This is the minimum evidence needed
  to select one live RC consumer rather than optimizing a root display value.
