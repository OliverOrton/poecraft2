#!/usr/bin/env python3
"""Exact rational illustrations for the be553608 whole-solver audit.

These are synthetic SSPs, not PoE mechanics, native tests, or performance
measurements. Uses only Python's standard library. Run:
    python solver_audit_synthetic_checks.py --output synthetic_results.json
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from fractions import Fraction as F
from itertools import product
from pathlib import Path
from typing import Mapping, Sequence

GOAL = "g"

@dataclass(frozen=True)
class Row:
    name: str
    cost: F
    transitions: Mapping[str, F]

    def validate(self) -> None:
        if self.cost < 0 or any(p < 0 for p in self.transitions.values()):
            raise ValueError("Costs and probabilities must be nonnegative")
        if sum(self.transitions.values(), F(0)) != 1:
            raise ValueError("A row must retain all probability mass")


def linear_solve(a: Sequence[Sequence[F]], b: Sequence[F]) -> list[F] | None:
    """Exact Gaussian elimination; None means singular, not a policy value."""
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError("The linear system must be square")
    m = [list(row) + [rhs] for row, rhs in zip(a, b)]
    for col in range(n):
        pivot = next((r for r in range(col, n) if m[r][col]), None)
        if pivot is None:
            return None
        m[col], m[pivot] = m[pivot], m[col]
        p = m[col][col]
        m[col] = [x / p for x in m[col]]
        for r in range(n):
            if r != col and m[r][col]:
                scale = m[r][col]
                m[r] = [x - scale * y for x, y in zip(m[r], m[col])]
    return [m[r][-1] for r in range(n)]


def evaluate_policy(policy: Mapping[str, Row]) -> dict[str, F] | None:
    """Certify goal absorption from EVERY listed state and evaluate cost.

    This intentionally does not claim that an unreachable improper component
    invalidates an otherwise proper entry-scoped policy.
    """
    names = list(policy)
    index = {name: i for i, name in enumerate(names)}
    a = [[F(i == j) for j in range(len(names))] for i in range(len(names))]
    absorption_rhs: list[F] = []
    cost_rhs: list[F] = []
    for i, name in enumerate(names):
        row = policy[name]
        row.validate()
        for target, probability in row.transitions.items():
            if target != GOAL:
                if target not in index:
                    raise ValueError(f"Missing continuation state: {target}")
                a[i][index[target]] -= probability
        absorption_rhs.append(row.transitions.get(GOAL, F(0)))
        cost_rhs.append(row.cost)
    absorption = linear_solve(a, absorption_rhs)
    if absorption is None or any(p != 1 for p in absorption):
        return None
    values = linear_solve(a, cost_rhs)
    if values is None or any(v < 0 for v in values):
        return None
    return dict(zip(names, values)) | {GOAL: F(0)}


def proper_policy_values(model: Mapping[str, Sequence[Row]], root: str) -> list[F]:
    """Exhaustive stationary-policy check for the TINY first example only."""
    if any(not rows for rows in model.values()):
        raise ValueError("Every nonterminal state needs at least one action")
    results = []
    for rows in product(*(model[s] for s in model)):
        evaluated = evaluate_policy(dict(zip(model, rows)))
        if evaluated is not None:
            results.append(evaluated[root])
    return sorted(results)


def row_value(row: Row, values: Mapping[str, F]) -> F:
    row.validate()
    return row.cost + sum((p * values[t] for t, p in row.transitions.items()), F(0))


def all_action_subsolution(model: Mapping[str, Sequence[Row]], values: Mapping[str, F]) -> bool:
    return values[GOAL] == 0 and all(v >= 0 for v in values.values()) and all(
        values[s] <= row_value(row, values)
        for s, rows in model.items() for row in rows
    )


def main() -> dict[str, object]:
    checks: dict[str, bool] = {}
    def check(name: str, condition: bool) -> None:
        checks[name] = bool(condition)
        if not condition:
            raise AssertionError(name)

    # A: the root's direct controller does not need a route from q to certify
    # a LOWER there. All mass, including the destructive exit d, is retained.
    finish = Row("finish", F(10), {GOAL: F(1)})
    alternative = Row("deviation", F(2), {GOAL: F(1, 2), "q": F(1, 2)})
    retry = Row("retry_then_destructive_exit", F(1), {"q": F(1, 2), "d": F(1, 2)})
    model_a = {
        "r": [finish, alternative],
        "q": [retry],
        "d": [Row("rebuild", F(40), {GOAL: F(1)})],
    }
    values_a = {"r": F(10), "q": F(42), "d": F(40), GOAL: F(0)}
    check("A_complete_subsolution", all_action_subsolution(model_a, values_a))
    check("A_enumerated_root_costs", proper_policy_values(model_a, "r") == [F(10), F(23)])
    check("A_strict_action_separation", row_value(alternative, values_a) == 23 > 10)

    weak = dict(model_a)
    weak["d"] = [Row("optimistic_boundary_payment", F(4), {GOAL: F(1)})]
    check("A_weak_model_ceiling", proper_policy_values(weak, "r") == [F(5), F(10)])
    threshold = 2 * (F(10) - 3)
    check("A_boundary_threshold", threshold == 14)
    omitted = dict(model_a)
    omitted["q"] = [retry, Row("overlooked_finish", F(1), {GOAL: F(1)})]
    check("A_missing_action_refutes_certificate", not all_action_subsolution(omitted, values_a))
    check("A_missing_action_true_optimum", min(proper_policy_values(omitted, "r")) == F(5, 2))
    better = dict(model_a)
    better["q"] = [retry, Row("real_cheaper_completion", F(8), {GOAL: F(1)})]
    check("A_cheaper_candidate_not_a_proof_defect", min(proper_policy_values(better, "r")) == F(6))
    closed_cycle = {
        "r": Row("option_one", F(1), {"q": F(1)}),
        "q": Row("option_two", F(1), {"r": F(1)}),
    }
    check("A_local_option_exit_not_global_properness", evaluate_policy(closed_cycle) is None)

    # B: enumerate 256 synthetic labels only as an independent arithmetic
    # comparison. The cut query itself uses two complete prefix events.
    count = 256
    labels = [f"junk_{i}" for i in range(count)]
    b_alt = Row("broad_action", F(1), {GOAL: F(1, 4)} | {s: F(3, 4 * count) for s in labels})
    b_values = {GOAL: F(0), "r": F(13, 2)} | {s: F(8) for s in labels}
    model_b: dict[str, Sequence[Row]] = {
        "r": [Row("finish", F(13, 2), {GOAL: F(1)}), b_alt]
    }
    for i, s in enumerate(labels):
        model_b[s] = [
            Row("repair", F(4), {GOAL: F(1, 2), s: F(1, 2)}),
            Row("label_visible_test", F(100 + i % 2), {GOAL: F(1)}),
        ]
    full = row_value(b_alt, b_values)
    cut = F(1) + F(1, 4) * 0 + F(3, 4) * 8
    check("B_all_action_subsolution", all_action_subsolution(model_b, b_values))
    check("B_complete_mass", sum(b_alt.transitions.values(), F(0)) == 1)
    check("B_query_equals_full_expectation", full == cut == 7)
    check("B_root_strict_separation", full > b_values["r"])
    check("B_repair_proper_cost", F(4) / F(1, 2) == 8)
    changed = dict(b_values)
    changed[labels[0]] = 0
    new_query = row_value(b_alt, changed)
    check("B_stale_query_must_not_be_relabelled", new_query == F(893, 128) < cut)
    hidden_values = [F(1) + (1 - p) * 8 for p in [F(1, 2), F(1, 4)]]
    hidden_mean = sum(hidden_values, F(0)) / 2
    check("B_context_specific_values", hidden_values == [F(5), F(7)] and hidden_mean == 6)
    check("B_average_is_not_uniform_lower", hidden_mean > min(hidden_values))
    offers = [(F(0), F(8)), (F(8), F(0))]
    post_observation = sum((min(x) for x in offers), F(0)) / 2
    precommit = min(sum((x[i] for x in offers), F(0)) / 2 for i in range(2))
    check("B_observation_timing", post_observation == 0 and precommit == 4)
    # Independent event support is reusable; its minimizing allocation is not.
    old_allocation_new_values = F(3, 5) * 10 + F(2, 5) * 0
    new_min = F(2, 5) * 10 + F(3, 5) * 0
    check("B_changed_value_minimizer", old_allocation_new_values == 6 > new_min == 4)

    # Arithmetic on REPORTED measurements: neither a new measurement nor an
    # empirical prediction. These figures are kept separate from the SSPs.
    strict_s = F("178.287")
    total_s = F("240.690")
    lower = F("198.8334996747695")
    upper = F("5218.040949685988")
    gap = upper - lower
    return {
        "classification": "synthetic exact rational mathematical illustrations; no native execution",
        "checks": checks,
        "number_of_checks": len(checks),
        "A": {
            "proper_stationary_root_costs": [str(x) for x in proper_policy_values(model_a, "r")],
            "boundary_value_for_tie": str(threshold),
            "boundary_value_for_strict_retirement": ">14",
            "weak_auxiliary_root_optimum": "5",
            "omitted_action_native_toy_optimum": "5/2",
            "genuinely_better_toy_policy": "6",
        },
        "B": {
            "synthetic_labels": count,
            "cut_contributions": 2,
            "exact_full_and_cut_Q": str(cut),
            "changed_potential_query": str(new_query),
            "hidden_context_values": [str(x) for x in hidden_values],
            "hidden_context_mixture_value": str(hidden_mean),
            "post_observation_cost": str(post_observation),
            "precommit_cost": str(precommit),
            "note": "An old independently valid native lower can survive as a separate certificate; it is not the changed-potential query.",
        },
        "arithmetic_from_retained_reports_not_new_measurements": {
            "F7_reported_strict_seconds": str(strict_s),
            "F7_reported_total_seconds": str(total_s),
            "other_time_seconds": str(total_s - strict_s),
            "maximum_fraction_removable_by_eliminating_strict_phase": float(strict_s / total_s),
            "F7_root_gap": float(gap),
            "20_percent_gap_reduction_target": float(F(4, 5) * gap),
            "historical_partial3to5_ratio_approx_using_rounded_old_cost": float(F("794067.4530398862") / F("80720.79")),
            "warning": "Time ceiling holds only with the same other work and trajectory; no speedup or regression is established.",
        },
    }

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Optional JSON destination")
    args = parser.parse_args()
    result = main()
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print(text)
