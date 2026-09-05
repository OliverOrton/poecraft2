"""Verify one saved current-source micro export; never runs the research census.

Usage: py -3 engine/benchmarks/verify_quotient_lower_probe.py native.json result.json
Uses the archived independent rational LP only for this retained native query.
"""
import json
import sys
from collections import Counter
from fractions import Fraction as F
from pathlib import Path

sys.dont_write_bytecode = True
archive = Path(__file__).resolve().parents[2] / "docs/archive/2026-09-04-free-value-bellman-research"
sys.path.insert(0, str(archive))
from free_value_fixtures import Row, lp_simplex, valid


def verify(path):
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


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    result = verify(sys.argv[1])
    Path(sys.argv[2]).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"checked_models": len(result["records"]),
        "normalized_roots": [r["checked_native_root"] for r in result["records"]
                             if r["coefficient_mode"] == "normalized_stored_reference"],
        "raw_mass_defects": result["raw_mass_defects"]}))
