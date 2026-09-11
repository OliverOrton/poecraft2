#!/usr/bin/env python3
"""Small exact arithmetic checks for a proposed post-incumbent investigation.

Synthetic finite SSPs only. This does not import or execute poecraft2, establish
its native semantics, or predict its performance. Python standard library only.
"""
from __future__ import annotations
import argparse
import json
from fractions import Fraction as F
from pathlib import Path


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(r) != n for r in a):
        raise ValueError("Expected square system")
    aug = [list(r) + [v] for r, v in zip(a, b)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if aug[i][j]), None)
        if pivot is None:
            raise ValueError("Singular system; not a finite policy evaluation")
        aug[j], aug[pivot] = aug[pivot], aug[j]
        d = aug[j][j]
        aug[j] = [v / d for v in aug[j]]
        for i in range(n):
            if i != j:
                d = aug[i][j]
                aug[i] = [x - d*y for x, y in zip(aug[i], aug[j])]
    return [row[-1] for row in aug]


def main() -> dict:
    checks: dict[str, bool] = {}
    def check(name: str, ok: bool) -> None:
        checks[name] = bool(ok)
        if not ok:
            raise AssertionError(name)

    # pi: r costs 1, returns to r with 1/2 and reaches q with 1/2;
    # q finishes at cost 10. mu: r -> q costs 1; q finishes at cost 5.
    p_pi = [[F(1,2), F(1,2)], [F(0), F(0)]]
    p_mu = [[F(0), F(1)], [F(0), F(0)]]
    c_pi, c_mu = [F(1), F(10)], [F(1), F(5)]
    matrix = lambda p: [[F(i == j)-p[i][j] for j in range(2)] for i in range(2)]
    j_pi, j_mu = solve(matrix(p_pi), c_pi), solve(matrix(p_mu), c_mu)
    check("old_policy_values", j_pi == [12, 10])
    check("new_policy_values", j_mu == [6, 5])
    advantage = [c_mu[i] + sum(p_mu[i][j]*j_pi[j] for j in range(2)) - j_pi[i]
                 for i in range(2)]
    check("complete_domain_advantage", advantage == [-1, -5])
    difference = solve(matrix(p_mu), advantage)
    check("performance_difference_identity", difference == [j_mu[i]-j_pi[i] for i in range(2)])
    # Expected visits from r solve (I-P)^T d=e_r.
    visits = lambda p: solve([list(x) for x in zip(*matrix(p))], [F(1), F(0)])
    d_pi, d_mu = visits(p_pi), visits(p_mu)
    check("old_visitation", d_pi == [2, 1])
    check("new_visitation", d_mu == [1, 1])
    check("root_cost_attribution", sum(d_pi[i]*c_pi[i] for i in range(2)) == j_pi[0])
    check("new_visits_give_actual_gain", sum(d_mu[i]*advantage[i] for i in range(2)) == -6)
    check("old_visits_are_not_the_new_gain", sum(d_pi[i]*advantage[i] for i in range(2)) == -7)

    # Taking an action once and following pi is a different controller from
    # replacing pi's decision at every return to the state.
    once = F(1) + F(1,2)*10
    repeated = F(1)/(1-F(1,2))
    check("one_time_switch_cost", once == 6)
    check("repeated_switch_cost", repeated == 2)
    check("one_time_and_repeated_are_distinct", once != repeated)

    # A cheap lower at an uncovered tail cannot become a policy continuation.
    estimated_using_lower = F(1) + F(0)
    actual_completion = F(1) + F(100)
    check("lower_tail_cannot_certify_upper", estimated_using_lower < 10 < actual_completion)

    # Bellman equality alone permits a zero-cost self trap; properness required.
    check("improper_tie_can_have_zero_residual", F(0) + F(1)*10 == 10)
    try:
        solve([[F(0)]], [F(0)])
    except ValueError:
        check("zero_cost_trap_has_no_finite_absorption_system", True)
    else:
        check("zero_cost_trap_has_no_finite_absorption_system", False)

    # Observation-local choice, not precommitting to an ordinal.
    offers = [(F(0), F(8)), (F(8), F(0))]
    post = sum((min(o) for o in offers), F(0))/2
    pre = min(sum((o[i] for o in offers), F(0))/2 for i in range(2))
    check("observed_choice_timing", post == 0 and pre == 4)

    ring, amulet = F("582192.807187538"), F("40215428.995558396")
    # Arithmetic on recorded N2 allocation, NOT the final Ring refusal.
    old_n2_required = 456_471_809 + 790_419_257
    return {
        "classification": "synthetic exact arithmetic; no native solver, tests, or simulation executed",
        "reviewed_source": "edd8ec25f40b11c8ba4f4aaf157db9405b593ed2",
        "checks": checks,
        "check_count": len(checks),
        "performance_difference": {
            "old_values": list(map(str, j_pi)), "new_values": list(map(str, j_mu)),
            "advantage": list(map(str, advantage)), "root_actual_difference": "-6",
            "old_visitation_weighted_advantage_not_actual_difference": "-7"
        },
        "proposed_20_percent_same_budget_cost_targets": {
            "ring": str(F(4,5)*ring), "ring_decimal": float(F(4,5)*ring),
            "amulet": str(F(4,5)*amulet), "amulet_decimal": float(F(4,5)*amulet),
            "status": "engineering targets, not predicted native outcomes; freeze against the matched control"
        },
        "recorded_N2_allocation_arithmetic_not_final_refusal": {
            "required_bytes": old_n2_required,
            "required_GiB": old_n2_required / (1 << 30),
            "excess_above_1_GiB_bytes": old_n2_required - (1 << 30),
            "warning": "Does not predict the final run's peak or show that a higher cap yields a better policy."
        }
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    text = json.dumps(main(), indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print(text)
