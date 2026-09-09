"""Recheck retained campaign comparisons without starting a solver.

Run from any directory after evidence.zip has been retained. The original full
and explicit checked-subsolution audits remain separate evidence owners.
"""
from fractions import Fraction
import json
from pathlib import Path
from zipfile import ZipFile

HERE = Path(__file__).resolve().parent


def compare(archive):
    def load(name):
        return json.loads(archive.read(name))

    old = load("control/coverage-compact.stdout.txt")
    support = load("support-compact.stdout.txt")
    final = load("retained-slot-compact.stdout.txt")
    def without_times(value):
        if isinstance(value, dict):
            return {k: without_times(v) for k, v in value.items() if not k.endswith("_ns")}
        if isinstance(value, list):
            return [without_times(v) for v in value]
        return value

    for key in ("probabilistic_donor", "sources", "resources"):
        assert without_times(old[key]) == without_times(support[key]), ("additive support changed semantics", key)
    checked = load("checked280-subsolution-exact.json")
    assert checked["audit_mode"] == "checked_subsolution"
    assert not checked["auxiliary_tightness_checked"]
    assert checked["optimistic_policy_ceiling"] is None
    reference = load("retained-slot-exact.json")
    assert reference["audit_mode"] == "full_refinement"
    assert reference["optimizer_checks"] == reference["checked_relations"] == 26586

    # A specific previously binding singleton event: the fracture consumes one
    # physical suffix slot. Verify native integer premises and outward rounding.
    def binding_row(data):
        return next(r for r in data["probabilistic_donor"]["checked_relations"]
                    if r["cell"] == 3150 and r["action"] == "harvest_reforge:defences")

    witness = next(w for w in final["probabilistic_donor"]["native_draw_witnesses"]
                   if w["action"] == "harvest_reforge:defences" and w["slot"] == 3
                   and not w["guaranteed"] and w["pool_filter_mod"] == 2**32-1)
    assert witness["side"] == 1 and witness["target"] == 500
    remaining = witness["other"] - sum(witness["removal"][1][:2])
    assert remaining == 35700
    caps = []
    for count, data in ((3, support), (2, final)):
        q = Fraction((1 << 24) * count * witness["target"], witness["target"] + remaining)
        cap = -(-q.numerator // q.denominator)
        assert all(c == cap for mask, _, c in binding_row(data)["events"] if mask in (25, 26, 28))
        caps.append(cap)
    assert caps == [695189, 463460]

    def compact(data):
        profile = data["preparation_profile"]
        return dict(source_lowers=[s["probabilistic_donor"] for s in data["sources"]],
                    preparation_seconds=profile["total_ns"]/1e9,
                    relation_seconds=profile["relations_ns"]/1e9,
                    support_seconds=profile["support_ns"]/1e9,
                    resources=data["resources"])

    names = ["control/coverage-development", "off-development", "support-development",
             "route-off-development", "checked280-development",
             "recovery-verified-off-development", "recovery-checked280-development",
             "bench-capacity-off-development", "final-reuse-development",
             "final-checked280-development"]
    baseline = load(names[0]+".json")["cases"][0]

    def problem(case):
        return {k: v for k, v in case["input"].items()
                if k not in ("native_retention_diagnostic", "native_retention_target_lower")}

    rows = []
    for name in names:
        case = load(name+".json")["cases"][0]
        assert problem(case) == problem(baseline), ("changed comparison problem", name)
        assert case["product_action_ids"] == baseline["product_action_ids"]
        summary = case.get("solve_summary") or {}
        evaluation = case.get("exact_strategy_evaluation") or {}
        telemetry = case.get("solver_telemetry") or {}
        row = dict(run=name, status=case["actual_status"], expectation_met=case["expectation_met"],
                   setup_seconds=telemetry.get("timings_ns", {}).get("solve_setup", 0)/1e9,
                   total_seconds=case["phase_wall_ms"]["total"]/1000,
                   lower=summary.get("lower_bound"), upper=summary.get("upper_bound"),
                   absolute_gap=summary.get("absolute_optimality_gap"),
                   expanded=summary.get("expanded_states"), stop=summary.get("termination"),
                   evaluation=evaluation.get("status"), target_met=summary.get("target_met"),
                   bench=telemetry.get("cache", {}).get("primitive_families", {}).get("bench"),
                   cap_checks=case["cap_checks"], errors=case["errors"])
        if case["expectation_met"]:
            assert evaluation["status"] == "matched"
            assert evaluation["success_probability"] == 1
            assert evaluation["zero_off_policy_mass"] and evaluation["cost_reconciled"]
            assert summary["upper_bound"] == evaluation["total_expected_cost"]
            assert not summary["converged"] and not summary["target_met"]
        rows.append(row)

    before, after = rows[5], rows[7]
    assert (before["lower"], before["upper"], before["expanded"]) == (after["lower"], after["upper"], after["expanded"])
    assert before["bench"]["rows"] - after["bench"]["rows"] == 133581
    return dict(baseline="a1b85956d4c438ab3ea92ea114a3f1138c505467",
                scope="two anchored compact controls and unchanged fractured four-to-five product8 request; no empty clean-five lower",
                compact={"control": compact(old), "additive_support": compact(support), "retained_slot": compact(final)},
                singleton_caps=caps, development=rows,
                exact_closure=False, historical_verified_target=2698.87479601436,
                timing_limitations="Single sequential observations, including a post-limit resumed treatment; no timing distribution. Finalization is included; watchdog failures remain failures.")


if __name__ == "__main__":
    with ZipFile(HERE/"evidence.zip") as archive:
        result = compare(archive)
    (HERE/"comparison.json").write_bytes((json.dumps(result, indent=2)+"\n").encode("utf-8"))
    print("Frozen problem, complete support payload, slot-cap and result comparisons passed")
