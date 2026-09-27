# Finder consumer and sustained candidate search

## Baseline to preserve

The current Finder already has a separate work owner, live frontier, seen/history records, conditional controls, complete graph checking and retained-side native validation. Do not pitch those as absent. Its eight-attempt limit, single retention proposal and shallow hand-selected expansion families still limit coverage. Current measured A5 work stops at eight checks after about 35 seconds, not because the entire four-minute solve budget was exhausted. This is a recorded limitation of that configuration, not proof that more checks will find something better. [S2, S10]

## 1. Separate generation, ordering, checking and retention

A search configuration records grammar, ranker, attempt ceiling, live-frontier/seen limits, native work limits and checking policy. New field names and CLI flags must be registered explicitly. Default attempt count remains eight.

- The live frontier contains complete candidates awaiting checking or partial control choices awaiting expansion.
- History records why a candidate was generated and what happened; it does not permanently occupy a live beam slot.
- Exact dedup records distinguish identical controllers from related siblings.
- The best checked artifact remains owned through cap, refusal, candidate failure and Finish.
- A checked parent can produce a child. Already-expanded choices are not regenerated without a meaningful new binding/decision.

Preserve the working separation already present; repair only a concrete issue identified at these boundaries. Do not turn this into a new search implementation.

## 2. First eight-attempt treatment

Use one predeclared deterministic schedule, not a tuned score that quietly changes every other variable. Preserve service for a complete seed and the K retained-side parent. Admit at most four complete selective-family candidates in the initial eight attempts, including the parent when it actually consumes an attempt. Count failures and capacity refusals against the same attempt ceiling.

Record the full candidate set and checking order. The baseline retains its old generation. This is a **grammar and generation** comparison. For a ranking-only experiment, freeze the identical candidate set separately and permute order; do not describe different generated candidates as order-only.

Shared-family candidate construction can consume substantial budget before graph checking begins. Its time, native rows and reforge work count. Do not report only the checker's clock or hide construction in a bootstrap step.

## 3. Feedback and the 24-attempt diagnostic

After the first family qualifies structurally, allow one separate `max_candidate_attempts=24` diagnostic through the existing configuration owner. Exact flag spelling is selected during implementation; no option is assumed to exist. Keep one active checker, frontier 16 and the existing 256 seen-record ceiling initially unless a measured limitation requires a separately labelled change.

A feedback step may use complete, already owned evidence to propose a different target-side decision rule, a valid alternative held-side binding, or one compatible acquisition change. It cannot call an unavailable continuation zero, infer an exact value from a coarse feature, or convert a cap into an expensive-policy label.

The 24-attempt baseline has the same ceiling; it may exhaust its smaller grammar early. Both receive the same original 240-second Finish, one-GiB live aggregate and logical-work budget. Report actual attempts and work, not just configured ceilings. A 24-attempt success is not an eight-attempt improvement. A productive extended-native 600-second control, if separately selected within the global run allowance, remains a distinct treatment and is never called the ordinary product result.

Do not idle after exhaustion. Do not regenerate equivalent graphs to satisfy a quota. The objective is best checked original-root policy available over time.

## 4. Generalization and role features

Held masks, missing masks, actual side occupancy, blockers, tier/member structure, costs and persistent context can guide proposals. They are not a state quotient. A role swap does not imply equal probabilities or identical optimal actions. Exact equality/certificates stay bound to physical/native/control identity.

No neural training is included. Keep candidate records sufficient for later learning: immutable source identity, grammar/binding, preconstruction features, observed cost/work, acceptance or censoring/refusal, parent and graph identity. Features computed by an expensive native row cannot be mislabeled cheap preconstruction inputs. Do not export huge unneeded graphs or sampled trajectories for a hypothetical dataset.

## 5. Integration and completion criteria

P5 is complete when the shared producer is actually invoked by Finder, a complete child and follow-through variant reach the independent checker, appropriate losing outcomes return through legal paid control, and the search retains the best checked result under Finish/cap. The comparison can still be economically negative.

Do not declare success from a lower heuristic score, more candidates, a dormant preservation node or the easier R goal. Report clean and coverage outcomes independently and retain old stronger Current references.
