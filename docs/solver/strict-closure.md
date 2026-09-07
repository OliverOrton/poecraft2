# Strict Closure

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

Strict closure connects the selected policy and competitive alternatives to exact carriers. It is not synonymous with exact evaluation of one fixed strategy. The conditional correctness argument is in [numerical closure](mathematics/numerical-closure.md#exactness); this page describes the retained mechanism.

## Purpose And Inputs

Inputs are the coarse solve result, concrete start, prices, declared action/program scope, refinement limits, and any independently verified rollback upper. The strict path must keep those identities separate from local state, cell, and compiled-node indices.

Primary owners are `solver_policy_refinement.cpp`, `solver_policy_oracle_*.inc`, `solver_refinement_*.cpp`, `solver_quotient_partition.cpp`, `solver_quotient_bellman.cpp`, and `solver_quotient_proof.cpp`.

## Persistent Session

One `PersistentQuotientSession` retains the strict calculator, selected closure, split-only partition, Bellman graph, ProofStore, published rows, alternative obligations, reverse dependencies, and verified incumbent. Newly discovered carriers extend this owner instead of rebuilding an unrelated proof store.

A source or target split invalidates affected evidence through its generation contract. A row proved for an old class cannot be reused merely because one representative remains unchanged. The proof must still cover every member of the current source class and probability into the current successor classes.

Selected policy rows are materialized first. Other admitted semantic actions at reachable cells become carrier-wide rows or explicit lower-only obligations. An immediate-price lower is valid but may be weak. Partial alternative evaluation is not a completed proof row.

When an alternative reaches outside the current closed partition, the frontier returns to the grow/repartition owner before unrelated old-generation obligations are replayed. That ordering avoids treating a known missing dependency as if it were already part of the closed proof.

## Observation and row obligations

Refinement retains the exact observations required by the admitted actions and the selected policy. Offered choices retain their pre-choice carrier identity; an equal offered modifier from a different observation carrier cannot satisfy the branch.

Partitioning may share rows only under the implemented action/cost/probability and observation contract. A selected-policy equality establishes less than equality for every alternative. See [CLM-0005](claims.md#clm-0005).

Destruction can remove source distinctions needed by downstream routing, permitting the specific documented collapse back to a coarse parent. Preserving actions retain the side, lock, fracture, and other observations their contract requires. Neither behavior is a generic permission to discard exact identity.

## Closure Conditions

The retained strict contract requires:

- a proper selected policy for the requested start and relevant reachable domain;
- complete action accounting under the actual request;
- every competitive alternative certified or carrier-wide dominated at the current Q generation;
- closed required frontier and action envelopes;
- compilation and independent evaluation of the actual returned strategy, with the declared cost reconciliation.

A lower-only certificate may contribute to alternative proof but cannot issue an executable row. A verified policy may supply an upper while strict closure remains incomplete. Those directions meet only through the compatible final proof/classification contract.

The mathematical sandwich in [CLM-0024](claims.md#clm-0024) does not authorize changing native tolerance flags into exact real-number claims. The companion reference names the remaining numerical correspondence obligation explicitly.

## Resource Stops And Bounded Results

A cap may stop strict proof while a cheaper compatible executable incumbent remains available. Publication may then retain that bounded artifact under its existing contract. The cap does not convert unresolved alternatives into dominated ones or make a frontier terminal.

A large count of partially evaluated alternatives is an observation of work, not proof closure. Likewise, installing a class says nothing about unrelated open families. Evidence must identify selected versus alternative work, source/target generations, and the actual first stop owner.

## Failure And Telemetry

Inspect strict states/cells, kernels/transitions, frontier growth, splits, obligation lifecycle, proof-memory ownership, policy improvements, and final envelope closure. Distinguish an unsupported observation contract from expensive but supported work.

Use the current source path and a matched witness when investigating a regression. Historical failures of reconstruct-then-merge or early quotient attempts remain evidence about those implementations and scopes, not declarations that the persistent current mechanism is absent.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
