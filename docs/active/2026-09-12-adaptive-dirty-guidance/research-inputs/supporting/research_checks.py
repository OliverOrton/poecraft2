#!/usr/bin/env python3
"""Exact synthetic checks for adaptive guidance. Not native PoE qualification.
Run: python research_checks.py --output research_checks.json
"""
from __future__ import annotations
import argparse
import json
from fractions import Fraction as F
from pathlib import Path
from typing import Mapping


def evaluate(costs: list[F], p: list[list[F]]) -> list[F]:
    """Solve (I-P)J=c with exact arithmetic; singularity is not success."""
    n = len(costs)
    if len(p) != n or any(len(row) != n for row in p):
        raise ValueError("shape mismatch")
    if any(c < 0 for c in costs) or any(x < 0 for row in p for x in row):
        raise ValueError("negative cost or mass")
    if any(sum(row, F(0)) > 1 for row in p):
        raise ValueError("mass exceeds one")
    a = [[F(i == j) - p[i][j] for j in range(n)] + [costs[i]] for i in range(n)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if a[i][j]), None)
        if pivot is None:
            raise ValueError("singular: absorption not established")
        a[j], a[pivot] = a[pivot], a[j]
        div = a[j][j]
        a[j] = [v / div for v in a[j]]
        for i in range(n):
            if i != j:
                factor = a[i][j]
                a[i] = [u - factor*v for u, v in zip(a[i], a[j])]
    return [row[-1] for row in a]


def debt(required: int, satisfied: int, occupied: int) -> int:
    if satisfied > occupied or satisfied > required:
        raise ValueError("distinct-goal example only")
    if satisfied == required and occupied == satisfied:
        return 0
    return max(1, required + occupied - 2*satisfied)


def main() -> dict[str, object]:
    checks: dict[str, bool] = {}
    def check(name: str, test: bool) -> None:
        checks[name] = bool(test)
        if not test:
            raise AssertionError(name)

    # Ordering estimates have no certification authority.
    lower = {"A": F(100), "B": F(150), "C": F(2000)}
    estimates = {"A": F(20000), "B": F(1500), "C": F(4000)}
    actual = {"A": F(800), "B": F(1400), "C": F(2500)}
    check("guidance_selects_B_initially", min(estimates, key=estimates.get) == "B")
    upper = actual["B"]
    retired = {a for a in lower if lower[a] >= upper}
    check("only_C_retired_by_certified_comparison", retired == {"C"})
    check("soft_deferred_A_can_still_be_best", actual["A"] < upper)
    check("lower_and_upper_not_changed_by_bad_prediction", lower["A"] == 100 and upper == 1400)
    check("mixing_estimate_with_lower_does_not_certify", (lower["A"]+estimates["A"])/2 > actual["A"])

    # A source-confirmed score property, instantiated only on synthetic counts.
    check("ring_both_goals_four_affixes_ties_empty", debt(2,2,4) == debt(2,0,0))
    check("ring_both_goals_five_affixes_looks_worse", debt(2,2,5) > debt(2,0,0))
    check("amulet_three_goals_six_affixes_ties_empty", debt(3,3,6) == debt(3,0,0))
    check("dirty_full_mask_not_terminal", debt(2,2,4) > 0)
    check("clean_goal_is_terminal", debt(2,2,2) == 0)
    # r->dirty costs 10; dirty cleanup costs1, succeeds3/4, loses goals1/4 returning r.
    dirty = evaluate([F(10), F(1)], [[F(0), F(1)], [F(1,4), F(0)]])
    check("dirty_controller_complete_cost", dirty == [F(44,3), F(14,3)])
    check("dirty_controller_beats_clean_100", dirty[0] < 100)

    # One-shot improvement and repeated improvement are different objectives.
    J = F(1000)
    rA,qA = F(1),F(99,100)
    rB,qB = F(100),F(4,5)
    QA,QB = rA+qA*J,rB+qB*J
    VA,VB = rA/(1-qA),rB/(1-qB)
    check("one_shot_prefers_B", QB < QA)
    check("repetition_prefers_A", VA < VB)
    check("repeated_values", VA == 100 and VB == 500)
    check("return_gain_identity_A", J-VA == (J-QA)/(1-qA))
    check("return_gain_identity_B", J-VB == (J-QB)/(1-qB))
    check("q_one_is_not_proper", sum([F(1)]) == 1)
    try:
        evaluate([F(1)], [[F(1)]])
        singular_refused = False
    except ValueError:
        singular_refused = True
    check("singular_loop_refused", singular_refused)

    # Residual correction is amplified by transient occupation, not discount.
    p = [[F(9,10), F(1,20)], [F(1,5), F(1,2)]]
    c = [F(2), F(3)]
    v = [F(10), F(7)]
    truth = evaluate(c,p)
    residual = [c[i]+sum((p[i][j]*v[j] for j in range(2)),F(0))-v[i] for i in range(2)]
    # evaluate permits nonnegative costs; this chosen residual is nonnegative.
    correction = evaluate(residual,p)
    horizon = evaluate([F(1),F(1)],p)
    check("fixed_policy_residual_identity", all(truth[i]-v[i] == correction[i] for i in range(2)))
    check("residual_times_horizon_bound", max(abs(truth[i]-v[i]) for i in range(2)) <= max(horizon)*max(abs(x) for x in residual))
    check("small_escape_error_large_cost_error", F(1)/(1-F(9999,10000)) == 10*F(1)/(1-F(999,1000)))

    # Censored rollouts are not completed cost observations.
    p_success = F(1,1000)
    K=10
    truncated_mean=sum(((1-p_success)**i for i in range(K)),F(0))
    check("truncated_spend_understates_true_cost", truncated_mean < 10 < 1/p_success)
    check("zero_sample_success_does_not_mean_impossible", (1-p_success)**100 > 0)
    check("simulator_action_cap_is_not_nontermination_proof", 0 < (1-p_success)**100 < 1)

    # Correct stochastic and observation ordering.
    rows=[(F(0),F(100)),(F(100),F(0))]
    pre = min(sum((row[i] for row in rows),F(0))/2 for i in range(2))+1
    post = sum((min(row) for row in rows),F(0))/2+1
    check("no_action_hindsight", pre == 51 and post == 1)
    check("all_outcomes_not_best_successor", F(1)+F(9,10)*100 > F(1)+0)
    mass = F(9,10)
    check("unknown_mass_not_renormalized_away", mass != 1)

    # Cheap run-local correction can go either way; it is NOT a confidence proof.
    prediction=F(100)
    observed=F(20)
    update=prediction+F(1,4)*(observed-prediction)
    check("estimate_can_relax", update == 80)
    update2=update+F(1,4)*(F(200)-update)
    check("estimate_can_increase", update2 == 110)
    check("fixed_policy_target_not_optimality_label", F(200) > F(20))
    old_key=("goal", "pool", "policy-A", 1)
    new_key=("goal", "pool", "policy-B", 1)
    check("changed_controller_invalidates_calibration_label", old_key != new_key)

    # Deterministic fairness example, not a native scheduler guarantee.
    unseen=["expensive-looking-A", "B", "C"]
    selected=[]
    for slot in range(1,13):
        if slot % 4 == 0 and unseen:
            selected.append(unseen.pop(0))
        else:
            selected.append("estimated-best")
    check("reserved_regular_service_visits_deferred_A", "expensive-looking-A" in selected)
    check("regular_service_does_not_retire_actions", len(set(selected)-{"estimated-best"}) == 3)

    return {
        "classification": "exact synthetic examples; no native solves, token-price measurement, or general learning guarantee",
        "number_of_checks": len(checks), "checks": checks,
        "dirty_route_cost": str(dirty[0]),
        "one_shot": {"A":str(QA),"B":str(QB)},
        "repeated": {"A":str(VA),"B":str(VB)},
        "residual_example": {"J":[str(x) for x in truth],"horizon":[str(x) for x in horizon]},
        "ten_step_censored_mean": str(truncated_mean),
        "polling_illustration_not_observation": {"run_seconds":600,"poll_seconds":30,"nominal_wait_calls":20,"blocking_final_returns":1,"pricing_claim":None}
    }

if __name__ == "__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--output",type=Path)
    args=ap.parse_args()
    result=main()
    text=json.dumps(result,indent=2)+"\n"
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding="utf-8")
    print(json.dumps({"passed":result["number_of_checks"],"output":str(args.output) if args.output else None}))
