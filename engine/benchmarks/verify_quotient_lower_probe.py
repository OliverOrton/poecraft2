"""Verify a saved micro, support-phase or probabilistic native export; no census.

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


def verify_probability(path):
    native = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    donor = native["probabilistic_donor"]
    assert donor["semantic_acceptance"] and donor["coefficient_acceptance"]
    assert native["proposal_adapter"]["accepted_by_support_control"]
    assert native["proposal_adapter"]["values"] == [0.01165]*31+[0]
    values = list(map(F, donor["values"])) + [F(0), F(donor["restart_boundary_lower"])]
    assert len(values) == 1538 and all(v >= 0 for v in values)
    proposal = list(map(F, donor["proposal_values"])) + values[-2:]
    refusal = donor["first_proposal_refusal"]
    assert refusal["kind"] == "violated_inequality"
    exact_refused_rhs = F(refusal["cost"]) + sum(F(p)*proposal[t] for t, p in refusal["exits"])
    assert F(refusal["lhs"]) > exact_refused_rhs
    assert F(refusal["cost"]) + F(refusal["continuation"]) >= exact_refused_rhs
    stochastic = 0
    for row in donor["checked_relations"]:
        assert sum(F(p) for _, p in row["exits"]) == 1
        rhs = F(row["cost"]) + sum(F(p)*values[t] for t, p in row["exits"])
        assert values[row["cell"]] <= rhs, row
        assert F(row["rhs"]) <= rhs
        if row["probability_aware"]:
            stochastic += 1
            source_mask = row["cell"]//16 % 32
            if row["action"] in ("augment", "regal", "exalt", "eldritch_exalt"):
                for target, probability in row["exits"]:
                    assert target < 1536 and probability >= 0
                    target_mask = target//16 % 32
                    # The saved medium has five disjoint native goal slots.
                    assert (target_mask & source_mask) == source_mask
                    assert (target_mask & ~source_mask).bit_count() <= 1
        assert row["action"] != "transmute"  # native fractured frame has no Normal member
    assert stochastic > 0
    assert native["retention_control"]["selected_from_root_minimum"]
    assert native["retention_control"]["lower"] == .3741
    economy = json.loads((archive / "native-economy.json").read_text(encoding="utf-8"))["prices"]
    mandatory = F(economy["eldritch_ichor:1"]) + F(economy["eldritch_exalt"])
    sources = []
    expected_residuals = {f"residual_family_{k}" for k in (1, 2, 3, 4, 5, 7, 9, 10)}
    for source in native["sources"]:
        p = source["program_after"]
        assert p["modifier_exits"] == len(p["weighted_exits"]) == 72
        assert sum(e["weight"] for e in p["weighted_exits"]) == p["total_weight"]
        assert sum(e["weight"] for e in p["weighted_exits"] if e["goal"]) == p["goal_weight"]
        expectation = F(0)
        for e in p["weighted_exits"]:
            assert F(e["lower"]) == (0 if e["goal"] else values[e["cell"]])
            expectation += F(e["weight"], p["total_weight"])*F(e["lower"])
        assert F(p["cost_lower"]) <= mandatory
        exact_program = mandatory+expectation
        assert F(p["lower"]) <= exact_program
        assert exact_program-F(p["lower"]) < F("1e-10")
        support_exact = mandatory + (1-F(p["goal_weight"], p["total_weight"]))*F(.01165)
        assert F(source["program_before"]["lower"]) <= support_exact
        assert support_exact-F(source["program_before"]["lower"]) < F("1e-12")
        baseline = source["independent_lower"]
        for model in source["complete_models"]:
            assert (model["admitted"], model["inapplicable"], model["open_families"]) == (28, 6, 8)
            assert model["eldritch_descriptions"] == (6 if source["second_source"] else 3)
            assert model["imprint_scope_excluded"]
            ranks = model["ranked_constraints"]
            assert {r["id"] for r in ranks if r["id"].startswith("residual_family_")} == expected_residuals
            assert len({r["id"] for r in ranks}) == len(ranks)
            assert model["lower"] == min(r["lower"] for r in ranks)
            assert model["portfolio"] == max(baseline, model["lower"])
        before, after = source["complete_models"]
        mask = 21 if source["second_source"] else 23
        prefixes = 2 if source["second_source"] else 3
        root = ((2*32+mask)*4+prefixes)*4+1
        limiting = min((r for r in donor["checked_relations"] if r["cell"] == root), key=lambda r: r["rhs"])
        assert limiting["action"] == "harvest_reforge:physical"
        # This fixed admissible distribution is in the conservative event
        # capacity model for every vector: full goal or the fractured-only
        # state. That state has the same row, yielding a geometric ceiling.
        hit = sum(F(p) for t, p in limiting["exits"] if values[t] == 0)
        failures = [t for t, _ in limiting["exits"] if values[t] != 0]
        assert len(failures) == 1
        recurrence = next(r for r in donor["checked_relations"]
                          if r["cell"] == failures[0] and r["action"] == limiting["action"])
        assert recurrence["exits"] == limiting["exits"]
        ceiling = F(limiting["cost"])/hit
        assert values[root] <= ceiling and ceiling-values[root] < F("1e-10")
        probability = F(p["goal_weight"], p["total_weight"])
        tie_donor = (F(baseline)-mandatory)/(1-probability)
        assert ceiling < tie_donor
        sources.append(dict(second_source=source["second_source"], donor=source["probabilistic_donor"],
            donor_gain=source["probabilistic_donor"]-source["support_donor"],
            program_before=source["program_before"]["lower"], program_after=p["lower"],
            program_gain=p["lower"]-source["program_before"]["lower"],
            compatible_action_gain=source["local_compatible_gain"],
            complete_model_gain=after["lower"]-before["lower"], portfolio_gain=after["portfolio"]-before["portfolio"],
            exact_program=str(exact_program), exact_projection_ceiling=str(ceiling),
            program_tie_donor=float(tie_donor), limiting_relation=limiting["action"],
            complete_model_ties=[r["id"] for r in after["ranked_constraints"] if r["lower"] == after["lower"]]))
    assert native["sources"][0]["source"] != native["sources"][1]["source"]
    assert native["sources"][0]["program_after"]["goal_weight"] == 500
    assert native["sources"][1]["program_after"]["goal_weight"] == 0
    assert native["resources"]["combined_additional_peak_bytes"] <= 16 << 20
    assert native["process_peak_working_set_bytes"] <= 1 << 30
    return dict(evidence_scope="native producer owns uniform semantics; this audit checks exact finite coefficients and integer program mass",
        checked_relations=len(donor["checked_relations"]), probability_relations=stochastic,
        exact_first_proposal_rhs=str(exact_refused_rhs), source_results=sources, production_authority=False)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pilot = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8-sig")).get("pilot")
    phase = pilot == "uniform-phase-lower-v1"
    probabilistic = pilot == "native-probabilistic-lower-v1"
    result = verify_probability(sys.argv[1]) if probabilistic else (verify_phase(sys.argv[1]) if phase else verify(sys.argv[1]))
    Path(sys.argv[2]).write_bytes((json.dumps(result, indent=2) + "\n").encode("utf-8"))
    if phase or probabilistic:
        print(json.dumps(result))
    else:
        print(json.dumps({"checked_models": len(result["records"]),
            "normalized_roots": [r["checked_native_root"] for r in result["records"]
                                 if r["coefficient_mode"] == "normalized_stored_reference"],
            "raw_mass_defects": result["raw_mass_defects"]}))
