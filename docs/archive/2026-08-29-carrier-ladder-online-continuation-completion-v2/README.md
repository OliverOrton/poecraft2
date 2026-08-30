# Carrier-Ladder Online Continuation Completion v2

**Status: completed diagnosis-only on 2026-08-29.** The existing exact
refinement path successfully services a named missing continuation, but
whole-policy reassembly after every newly serviced state is too expensive
under the fixed solve budget and materially worsens incumbent quality. The
speculative retry behavior was removed.

Parent: [Documentation archive](../README.md) · Final [result](result.md)

## Objective

Turn one live missing continuation from joint-policy assembly into ordinary
exact row service early enough for a later joint retry, while retaining the
existing ladder, scheduler, mechanics, compiler, evaluator, incumbent,
publication, caps, and product authority.

V2 never stored fragments, items, bases, or craft-specific advice and never
used the historical strategy as an incumbent or seed.

## Retained Observation

The benchmark-private joint-attempt lineage schema is now
`joint_anytime_attempt_lineage_v2`. For each first missing continuation it
records:

- whether requested bounded finish was already latched at discovery;
- whether exact refinement selected the state and at which expanded-state
  count; and
- whether the state was expanded, queued, a carrier, still open, and how many
  owned valid priced rows it had at terminal publication.

The record is available only through the existing disabled-by-default private
exact-boundary diagnostic. Ordinary solving and product behavior do not read
it.

## First Lifecycle Run

Case
`conquest-lamellar-allflame-clean-5-goal-product8-derived-d46072db7f95bbb4-derived-0a717785aa75a228`,
revision `case-rev-33ceac3cbb5a476ad183a28f47966566`, ran with idempotency
key `continuation-v2-lifecycle-20260829-v1` as job
`job-d4ce1f39-202c-4e2c-ac96-f47a74234418`, attempt
`attempt-1b05510f-bdb5-471e-aa2d-580cd41f7509`.

Identities were:

- core solve:
  `fad114b6a743e6e0932c3191579e98e0cf4565d7eba6edc34c50fa0037225428`;
- full request:
  `d8f149fc0b182e891a66fa6aeda023dc94d7f4267a879ad7c416cf26062e10bd`;
- job:
  `b99b2d51509d079466081898b3da8e00a1b836c233743ba27298e368be839f7c`;
- ordinary result:
  `3a647002c5bca10f4ba3ed3ba7d171197d008b043e1423449f872eec2c34ea07`;
  and
- ordinary report SHA-256:
  `d20acdcc4ec528bad87eb27a416374b5108b75773eace4f30a88b99efc220b8f`.

It naturally reproduced strategy SHA-256
`bee87369fee733fff3d2e093eae5fa914ddd6616badcbf6f324234e4d95d8892`,
24,578 rows, 9,816 states, 4,457 expansions, success probability one, zero
off-policy mass, and independently reconciled exact cost
`1550334.436668944`.

Checkpoint two discovered state 1780 before bounded finish. Existing
refinement selected it at expanded-state count 983. By terminal publication it
was expanded, queued, a carrier, and owned five valid priced rows. Terminal
assembly no longer failed there; it reached new unexpanded state 4489, first
discovered only after requested finish was latched. This proves successful
ordinary exact service and a continuation cascade. The aggregate open/service
counters were stale because their closure bookkeeping is carrier-automatic-
lane-specific, but the complete row and later selected-policy walk are the
authoritative lifecycle evidence.

## Rejected Benchmark-Private Retry

A temporary diagnostic-only change retried existing whole-policy assembly
when a named continuation gained ordinary rows. It used the same immutable
revision with idempotency key `continuation-v2-retry-20260829-v1`, job
`job-6f9999fe-f543-4d8b-bef5-b654d0ee9d21`, and attempt
`attempt-bad26ea1-2066-4bf0-a040-221a6a473d99`.

Identities were:

- core solve:
  `4e28dbc750fc8d006283d8a7adeafc6c113004c29dfa81c4890561d52b55e5ae`;
- full request:
  `a780dd80089ca428d1aea4ca59ebaedbb4e6600d7a0f2c2df8e0a76c5e19def5`;
- job:
  `a3f2c7e3f9a70584b679d38271c1fa210c132e2e5160c47e325709457144c6a3`;
- ordinary result:
  `3ffcbf989a7c7fb206200fbf30a8a57773b99cfe8d5fac142e5d4eba8e6ec0af`;
  and
- ordinary report SHA-256:
  `f32f639b25ee4ba295dc62acb30efc35323756d055891fa5202f9b73bbec44b9`.

The probe closed 25 successive named continuation states and retained two
open ones, proving that exact service itself works. It also triggered 25
extra whole-policy retries. Under the unchanged ten-second solve horizon that
reduced final rows from 24,578 to 17,233, reduced expansions from 4,457 to
3,071, and changed the independently evaluated strategy to SHA-256
`256b526679ea9bc46552ea3747bdcdb4623a3954c6dc7eca1b93257b5b116659`
at exact cost `8690805.04129252`. The approximately 5.6-times worse incumbent
fails the plan's usefulness criterion. All retry behavior and its trigger
were removed; no part of it is present in final source.

## Verification And Disposition

The final retained source contains only bounded observational lineage. Native
build passed after speculative removal; final diagnostic executable SHA-256
was `0b728eba3063f0de0b9273cadb5e29dce643b788a17d3591ab7c82ad170aad5f`
and DLL SHA-256 was
`82b84d799a9ad456c13902f6a159de03a08a8f802340d23c1e64328c54f400f5`.
`git diff --check` passed. No Simulator or full repository suite ran because
no compiled behavior-changing strategy or cross-layer production behavior was
retained.

V2 closes diagnosis-only. A future selected boundary would need to continue a
named selected-policy walk incrementally, or otherwise amortize continuation
closure inside one assembly attempt, without rerunning the entire global
candidate assembly after every exact state. That is a materially different
design boundary and is not inferred as active work.
