"""Verify a saved micro or uniform-phase native export; never runs a census.

Usage: py -3 engine/benchmarks/verify_quotient_lower_probe.py native.json result.json
The archived rational LP is imported only in micro mode. Phase mode checks
the retained support inequalities and integer-weight composition directly.
"""
import json
import sys
from collections import Counter
from fractions import Fraction as F
from pathlib import Path

sys.dont_write_bytecode = True
archive = Path(__file__).resolve().parents[2] / "docs/archive/2026-09-04-free-value-bellman-research"
sys.path.insert(0, str(archive))


def verify(path):
    from free_value_fixtures import Row, lp_simplex, valid
    native = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    old = json.loads((archive / "native-micro.json").read_text())
    states = {str(s["id"]): s for s in native["states"]}
    base = {s: F(v["lower"]) for s, v in states.items()}
    goals = {s: F(0) for s, v in states.items() if v["goal"]}
    caller = {"transmute", "alteration", "restart"}
    assert set(native["caller_actions"]) == caller
    counts = Counter((str(r["source"]), r["action"]) for r in native["rows"])
    legal = {s: {} for s in states if s not in goals}
    masses = []
    for s in legal:
        assert {a for t, a in counts if t == s} == caller
        assert all(counts[(s, a)] == 1 for a in caller)
    for row in native["rows"]:
        assert row["supported"]
        if not row["applicable"]:
            assert row["inapplicability_owner"] == "native_action_legal"
            assert not row["entries"]
            continue
        successors = tuple((str(e["target"]), F(e["p"])) for e in row["entries"])
        assert all(t in states and p > 0 for t, p in successors)
        mass = sum(p for _, p in successors)
        masses.append(mass)
        legal[str(row["source"])][row["action"]] = (F(row["cost"]), successors, mass)

    def signature(data):
        return [(r["source"], r["action"], r["cost"], r["applicable"], r["entries"])
                for r in data["rows"]]
    assert signature(native) == signature(old), "native kernel changed since archive"
    assert [(s["id"], s["canonical_item_key"]) for s in native["states"]] == [
        (s["id"], s["canonical_item_key"]) for s in old["states"]]

    def model(domain, normalized, scalar=False):
        rows = {}
        for s in domain:
            rows[s] = []
            for action, (cost, successors, mass) in legal[s].items():
                if scalar:
                    rows[s].append(Row(action, max(base[s], cost), (("sink", F(1)),)))
                else:
                    rows[s].append(Row(action, cost, tuple(
                        (t, p / mass if normalized else p) for t, p in successors)))
        boundary = {s: value for s, value in base.items() if s not in domain} | goals | {"sink": F(0)}
        return rows, boundary

    records = []
    normalized_oracle = None
    for measured in native["models"]:
        stage = measured["stage"]
        normalized = measured["coefficient_mode"] == "normalized_stored_reference"
        domain = list(legal) if stage == 3 else ["0", "7"] if stage == 2 else ["0"]
        rows, boundary = model(domain, normalized, stage == 0)
        optimum, pivots = lp_simplex(rows, boundary)
        candidate = {s: F(measured["values"][int(s)]) for s in domain}
        assert valid(rows, boundary, candidate), (stage, normalized, "unsafe native certificate")
        error = optimum["0"] - candidate["0"]
        assert 0 <= error < F("0.0000001"), (stage, normalized, str(error))
        if normalized:
            expected = [2.15, 2.2235586973264385, 2.2415219923456653, 23.79][stage]
            assert abs(float(optimum["0"]) - expected) < 1e-9
        if normalized and stage == 3:
            normalized_oracle = optimum
        joined = boundary | optimum
        tight = {s: [r.action for r in rs if r.cost + sum(p * joined[t] for t, p in r.successors) == optimum[s]]
                 for s, rs in rows.items()}
        records.append(dict(stage=stage, coefficient_mode=measured["coefficient_mode"],
            checked_native_root=measured["root"], exact_reference_root=str(optimum["0"]),
            reference_root=float(optimum["0"]), exact_shortfall=str(error),
            all_raw_inequalities_pass=True, simplex_pivots=pivots, limiting_actions=tight))
    complete, boundary = model(list(legal), True)
    root_only, root_boundary = model(["0"], True)
    assert valid(complete, boundary, normalized_oracle)
    assert not valid(root_only, root_boundary, {"0": normalized_oracle["0"]})
    return dict(evidence="rebuilt native query plus exact rational comparison on identical declared coefficients",
        production_authority=False, underlying_native_probability_law_certified=False,
        canonical_action_obligations=len(counts), native_inapplicabilities=7,
        archived_kernel_and_exact_member_identity_parity=True,
        raw_mass_defects=sorted({str(m - 1) for m in masses}),
        records=records, fixed_J_complete_normalized_control=True,
        fixed_J_root_with_independent_frontier_control=False,
        local_model_optimum_authority="independent rational reference only; native result is checked finite lower")


def verify_phase(path):
    """Exact arithmetic audit; native source owners establish the semantic relation."""
    native = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    assert native["pilot"] == "uniform-phase-lower-v1"
    assert native["solver_steps"] == 0 and not native["production_authority"]
    donor = native["native_donor"]
    assert donor["semantic_acceptance"] and donor["numeric_acceptance"]
    assert donor["family_mask"] == (1 << 11) - 2
    values = list(map(F, donor["values_by_goal_mask"]))
    assert len(values) == 32 and values[-1] == 0 and min(values) >= 0
    actions = donor["primitive_cover"]
    assert len({a["id"] for a in actions}) == len(actions)
    inequalities = 0
    for mask, value in enumerate(values):
        for bit in (1, 2, 4, 8, 16):
            assert value >= values[mask | bit]
        for action in actions:
            if not action["priced"]:
                continue
            # This is the pointwise native support cover, not a claim that the
            # deterministic optimistic row is any physical member's kernel.
            assert value <= F(action["price_lower"]) + values[mask | action["reach"]]
            inequalities += 1
    economy = json.loads((archive / "native-economy.json").read_text(encoding="utf-8"))["prices"]
    mandatory = F(economy["eldritch_ichor:1"]) + F(economy["eldritch_exalt"])
    programs = []
    for name in ("program", "second_program"):
        r = native[name]
        assert r["semantic_acceptance"] and r["numeric_acceptance"]
        probability = F(r["goal_weight"], r["total_weight"])
        assert 0 <= probability <= F(r["goal_probability_upper"]) <= 1
        assert 0 <= F(r["cost_lower"]) <= mandatory
        assert r["modifier_exits"] == 72
        # Both measured sources have at most five affixes after add. For the
        # first source all-five-goal exits are true goals; the second retains
        # a missing prefix. The frozen table is constant on every other mask.
        assert len(set(values[:-1])) == 1
        assert F(r["failure_lower_min"]) == F(r["failure_lower_max"]) == values[0]
        exact = mandatory + (1-probability)*values[0]
        assert F(r["lower"]) <= exact
        assert exact - F(r["lower"]) < F("0.000000000001")
        programs.append(dict(source=name, exact_probability=str(probability),
            exact_cost_plus_failure=str(exact), checked_lower=r["lower"],
            directed_shortfall=str(exact-F(r["lower"]))))
    assert native["program"]["source"] != native["second_program"]["source"]
    assert native["second_program"]["goal_weight"] == 0
    assert native["reuse"]["fresh_table_and_output_equal"]
    old_cover = json.loads((archive.parent / "2026-09-04-operator-complete-frontier-bellman-lower-pilot-v2/medium-coverage.json").read_text(encoding="utf-8"))
    floors = native["prepared_action_floors"]
    assert {r["id"] for r in floors} == {r["id"] for r in old_cover["canonical_actions"]}
    assert len(floors) == 28 and sum(r["inapplicable"] for r in floors) == 6
    baseline = native["baseline"]["independent_root"]
    assert all(r["lower"] >= baseline and r["lower"] >= r["analytic"] and r["lower"] >= r["operator_lower"] for r in floors)
    for model in native["complete_models"]:
        assert model["open_families"] == 10
        ranks = model["ranked_constraints"]
        ids = [r["id"] for r in ranks]
        assert len(ids) == len(set(ids))
        assert {"residual_family_"+str(i) for i in range(1, 11)} <= set(ids)
        assert model["lower"] == min(r["lower"] for r in ranks)
        assert model["portfolio"] == max(baseline, model["lower"])
    before, after = native["complete_models"]
    ties = [r["id"] for r in after["ranked_constraints"] if r["lower"] == after["lower"]]
    return dict(native_relation="producer-owned pointwise support cover; verifier checks its finite arithmetic",
        checked_donor_inequalities=inequalities, programs=programs,
        donor_value=native["baseline"]["new_post_lower"],
        compatible_local_action_gain=max(baseline, native["program"]["lower"])-baseline,
        complete_model_gain=after["lower"]-before["lower"],
        portfolio_gain=after["portfolio"]-before["portfolio"],
        limiting_ties=ties, production_authority=False)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    phase = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8-sig")).get("pilot") == "uniform-phase-lower-v1"
    result = verify_phase(sys.argv[1]) if phase else verify(sys.argv[1])
    Path(sys.argv[2]).write_bytes((json.dumps(result, indent=2) + "\n").encode("utf-8"))
    if phase:
        print(json.dumps(result))
    else:
        print(json.dumps({"checked_models": len(result["records"]),
            "normalized_roots": [r["checked_native_root"] for r in result["records"]
                                 if r["coefficient_mode"] == "normalized_stored_reference"],
            "raw_mass_defects": result["raw_mass_defects"]}))
