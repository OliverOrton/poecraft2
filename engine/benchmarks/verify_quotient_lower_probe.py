"""Verify a saved micro, support-phase or probabilistic native export; no census.

Usage: py -3 engine/benchmarks/verify_quotient_lower_probe.py native.json[.gz] result.json
   or: py -3 engine/benchmarks/verify_quotient_lower_probe.py --ordinary control.json treatment.json native.json result.json
The archived rational LP is imported only in micro mode. Phase mode checks
the retained support inequalities and integer-weight composition directly.
"""
import json
import sys
from collections import Counter
from fractions import Fraction as F
from pathlib import Path
import gzip

sys.dont_write_bytecode = True
archive = Path(__file__).resolve().parents[2] / "docs/archive/2026-09-04-free-value-bellman-research"
sys.path.insert(0, str(archive))


def load_native(path):
    raw=Path(path).read_bytes()
    if str(path).endswith(".gz"): raw=gzip.decompress(raw)
    return json.loads(raw.decode("utf-8-sig"))


def verify(path):
    from free_value_fixtures import Row, lp_simplex, valid
    native = load_native(path)
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
    native = load_native(path)
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
    native = load_native(path)
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



def verify_joint(path):
    """Exact coefficients/minimization audit; native C++ owns history semantics."""
    from itertools import permutations
    native = load_native(path)
    boundary = native["pilot"] == "native-side-boundary-lower-v1"
    assert boundary or native["pilot"] == "native-joint-goal-lower-v1"
    assert native["solver_steps"] == 0 and not native["production_authority"]
    p = native["probabilistic_donor"]
    coupled = boundary and p["continuation"] == 2
    retention = p.get("retention",0)
    coordinates = [c+[0]*(5-len(c)) if c is not None else None for c in p.get("coordinates",[])]
    filter_sides = {i+1:next(b["side"] for b in native.get("bench_audit",[]) if b["mod"]==mod)
                    for i,mod in enumerate(p.get("filter_mods",[])) if mod != 2**32-1}
    values = list(map(F, p["values"]))
    if not coupled: values += [F(0), F(p["restart_boundary_lower"])]
    prior = list(map(F, native["joint_control" if boundary else "marginal_control"]["values"]))
    if coupled:
        assert not p["fixed_boundary_used"] and p["fresh_cell"] == 1538 and (len(values) >= 3074 if retention else len(values) == 3074)
        assert values[1536] == values[1537] == 0
        assert values[1538] > F(p["restart_boundary_lower"])
        assert any(r["cell"] == 1538 for r in p["checked_relations"])
        for r in p["checked_relations"]:
            assert r["reason"] != "fixed_independent_boundary"
            if r["action"] == "restart": assert r["exits"] == [[1538,1]]
    mass = 1 << 24
    draws = p["native_draw_witnesses"]
    assert len({(w["action"],w["slot"],w["guaranteed"],w.get("pool_filter_mod",2**32-1)) for w in draws})==len(draws)
    for w in draws:
        assert w.get("pool_filter_mod",2**32-1) in [2**32-1]+p.get("filter_mods",[])
    if p.get("filter_modes"):
        assert filter_sides and any(w.get("pool_filter_mod",2**32-1)!=2**32-1 for w in draws)
        for i,c in enumerate(coordinates):
            if c is None or not c[4]: continue
            assert c[4] in filter_sides and c[2+filter_sides[c[4]]] >= 1
            assert values[i]>0 # occupied non-goal filter never silently becomes terminal

    def conditional(w, same, other):
        counts = (same, other) if w["side"] == 0 else (other, same)
        remaining = w["other"]
        for side in range(2):
            for i in range(counts[side]):
                remaining -= min(remaining, w["removal"][side][i])
        return F(w["target"], w["target"] + remaining) if w["target"] else F(0)

    nonempty = p.get("nonempty_witnesses", [])
    for w in nonempty:
        draw = draws[w["draw"]]
        assert draw["action"] == w["action"] and not draw["guaranteed"]
        assert w["action"] in ("exalt", "eldritch_exalt", "alchemy")
        assert (w["prefixes"],w["suffixes"])[draw["side"]] < 3
        assert w["phase"] < 0 or w["phase"] == draw["side"]
        remaining = draw["other"]
        for side,count in enumerate((w["prefixes"],w["suffixes"])):
            for removed in draw["removal"][side][:count]: remaining -= min(remaining, removed)
        assert remaining == w["remaining_other"] > 0
    if retention in (3,4): assert nonempty
    refills=p.get("refill_witnesses",[])
    for w in refills:
        assert w["action"]=="alchemy" and w["target"]==4
        assert 0<=w["prefixes"]<=3 and 0<=w["suffixes"]<=3
        assert w["prefixes"]+w["suffixes"]<=w["minimum"]<=max(w["target"],w["prefixes"]+w["suffixes"])
        for pp in range(w["prefixes"],4):
            for ss in range(w["suffixes"],4):
                if pp+ss>=w["minimum"]: continue
                assert any(n["action"]==w["action"] and n["prefixes"]==pp and n["suffixes"]==ss and n["phase"]==-1 for n in nonempty), ("unproved conditional refill",w,pp,ss)
    if p.get("minimum_reforge_occupancy"):
        assert refills and any(w["minimum"]==4 and w["prefixes"]==w["suffixes"]==0 for w in refills)
    if p.get("applied_alchemy"):
        minimum=max((w["minimum"] for w in refills if w["action"]=="alchemy" and w["prefixes"]==w["suffixes"]==0),default=0)
        alchemy_rows=0
        for r in p["checked_relations"]:
            if r["action"]!="alchemy" or r["independent_price"]: continue
            alchemy_rows+=1
            for event in r["events"]:
                # Existing export is [mask, minimum-cell, capacity]. It must
                # not smuggle the Normal source into a failed observed event.
                target=event[1]
                base=coordinates[target][0]
                local=base-p["fresh_cell"] if base>=p["fresh_cell"] else base
                assert local//(32*16)==2 and (local//4)%4+local%4>=minimum
                assert (local//16)%32==event[0]
        assert alchemy_rows
    annul_checks = 0
    side_masks=[sum(1<<slot for slot in range(5) if next(d["side"] for d in draws if d["slot"]==slot)==side) for side in range(2)]

    events = []
    for w in p["joint_events"]:
        assert w["subset"] & (w["retained"] | w["forced"]) == 0
        k, m = w["subset"].bit_count(), w["positions"]
        assert k == len(w["conditional"]) <= 3 and m <= 3
        assert len(w["natural_draws"]) == k
        assert len(w["guaranteed_draws"]) in (0, k)
        exact = []
        slots = [i for i in range(5) if w["subset"] & (1 << i)]
        for i, slot in enumerate(slots):
            n = draws[w["natural_draws"][i]]
            assert n["action"] == w["action"] and n["slot"] == slot and n["side"] == w["side"]
            bounds = []
            for j in range(m):
                same = w["initial_same_side"] + (m-1 if w["uniform_history"] else j)
                bound = conditional(n, same, w["other_side_blockers"])
                if w["guaranteed_draws"]:
                    g = draws[w["guaranteed_draws"][i]]
                    assert g["action"] == w["action"] and g["slot"] == slot and g["guaranteed"]
                    bound = max(bound, conditional(g, same, w["other_side_blockers"]))
                assert bound <= F(w["conditional"][i][j]) <= bound + F("1e-14")
                bounds.append(bound)
            exact.append(bounds)
        joint = sum((__import__("functools").reduce(lambda a,b:a*b,
                     (exact[i][j] for i,j in enumerate(assignment)), F(1))
                     for assignment in permutations(range(m), k)), F(0))
        joint = min(F(1), joint)
        assert joint <= F(w["joint_upper"]) <= joint + F("1e-13")
        assert joint <= F(w["capacity"], mass)
        assert w["capacity"] == -((-F(w["joint_upper"])*mass).numerator // (-F(w["joint_upper"])*mass).denominator)
        events.append(dict(action=w["action"], subset=w["subset"], exact_conditional_union=str(joint),
                           capacity=w["capacity"], probability_upper=float(F(w["capacity"],mass))))
    measured = next(e for e in events if e["action"] == "harvest_reforge:physical" and e["subset"] == 7)
    assert measured["exact_conditional_union"] == "16/55165" and measured["capacity"] == 4867

    by_source = {}
    optimizer_checks = 0
    for r in p["checked_relations"]:
        assert r["events"] and all(0 <= cap <= mass for _,_,cap in r["events"])
        remaining = mass
        minimum = F(0)
        for _, target, cap in sorted(r["events"], key=lambda e: values[e[1]]):
            take = min(cap, remaining)
            minimum += F(take, mass)*values[target]
            remaining -= take
            if not remaining:
                break
        assert remaining == 0
        if r.get("removable_affixes",0):
            annul_checks += 1
            n = r["removable_affixes"]
            assert r["action"] in ("annul","eldritch_annul") and 0<n<=6
            assert len(r["events"]) == n
            assert all(cap == (mass+n-1)//n for _,_,cap in r["events"])
            base,cm,jp,js,filter_mode = coordinates[r["cell"]]
            offset=1538 if base>=1538 else 0
            local=base-offset
            rarity,mask,prefixes,suffixes=local//512,(local//16)%32,(local//4)%4,local%4
            frame=0 if offset else p["fractured_mask"]
            phase=r["phase_branch"] if r["action"]=="eldritch_annul" else -1
            expected=[]
            def target(m,ps,ss,c,cp,cs,f=filter_mode): return (offset+((rarity*32+m)*4+ps)*4+ss,c,cp,cs,f)
            for slot in range(5):
                bit=1<<slot; side=int(bool(side_masks[1]&bit))
                if mask&bit and not frame&bit and (phase<0 or phase==side):
                    expected.append(target(mask&~bit,prefixes-(side==0),suffixes-(side==1),cm&~bit,jp,js))
            for side in range(2):
                if phase>=0 and phase!=side: continue
                junk=(prefixes,suffixes)[side]-(mask&side_masks[side]).bit_count()
                crafted=(jp,js)[side]
                expected += [target(mask,prefixes-(side==0),suffixes-(side==1),cm,jp,js)]*(junk-crafted)
                filtered=int(bool(filter_mode and filter_sides[filter_mode]==side))
                expected += [target(mask,prefixes-(side==0),suffixes-(side==1),cm,jp-(side==0),js-(side==1))]*(crafted-filtered)
                expected += [target(mask,prefixes-(side==0),suffixes-(side==1),cm,jp-(side==0),js-(side==1),0)]*filtered
            assert sorted(tuple(coordinates[t]) for _,t,_ in r["events"])==sorted(expected)
            exact_uniform=sum((values[t] for _,t,_ in r["events"]),F(0))/n
            assert minimum<=exact_uniform
        actual = sum((F(prob)*values[t] for t,prob in r["exits"]),F(0))
        assert sum(F(prob) for _,prob in r["exits"]) == 1
        assert actual == minimum, ("nonminimal or stale event row",r["cell"],r["action"])
        assert values[r["cell"]] <= F(r["cost"]) + minimum
        by_source.setdefault(r["cell"], []).append(r)
        optimizer_checks += 1
    for cell, rows in by_source.items():
        floor = min(F(r["cost"])+sum((F(prob)*values[t] for t,prob in r["exits"]),F(0)) for r in rows)
        for r in rows:
            if r["reason"] == "candidate_price_shortcut":
                assert F(r["cost"]) > floor + F("1e-12"), ("limiting computational shortcut",cell,r["action"])
    assert p["price_reactivations"]
    assert any(r["cost"] != r["value"] for r in p["price_reactivations"])  # downward-repaired ties
    for r in p["price_reactivations"]:
        assert abs(r["cost"]-r["minimum_rhs"]) < 1e-10
    assert any("CurrencyDelveCrafting" in r["action"] for r in p["price_reactivations"])
    sources = []
    for s, root in zip(native["sources"], [1405,1369]):
        program = s["program_after"]
        assert program["modifier_exits"] == len(program["weighted_exits"]) == 72
        assert sum(e["weight"] for e in program["weighted_exits"]) == program["total_weight"] == 55700
        assert sum(e["weight"] for e in program["weighted_exits"] if e["goal"]) == program["goal_weight"]
        exact_new = exact_old = F(program["cost_lower"])
        for e in program["weighted_exits"]:
            assert F(e["lower"]) == (0 if e["goal"] else values[e["cell"]])
            q = F(e["weight"],program["total_weight"])
            exact_new += q*F(e["lower"])
            exact_old += q*(0 if e["goal"] else (prior[coordinates[e["cell"]][0] if retention else e["cell"]] if prior else F(s["support_donor"])))
        assert F(program["lower"]) <= exact_new < F(program["lower"])+F("1e-10")
        assert F(s["program_before"]["lower"]) <= exact_old < F(s["program_before"]["lower"])+F("1e-10")
        before, after = s["complete_models"]
        for model in (before,after):
            assert model["admitted"] == 28 and model["inapplicable"] == 6 and model["open_families"] == 8
            assert model["eldritch_descriptions"] == (6 if s["second_source"] else 3) and model["imprint_scope_excluded"]
            assert model["lower"] == min(r["lower"] for r in model["ranked_constraints"])
        donor = F(s["probabilistic_donor"])
        assert donor == values[root] and donor > F(s["independent_lower"])
        assert all(F(r["lower"]) >= donor for r in after["ranked_constraints"])
        assert after["lower"] == s["probabilistic_donor"] and after["portfolio"] > before["portfolio"]
        limiting = min(by_source[root],key=lambda r:F(r["cost"])+sum(F(prob)*values[t] for t,prob in r["exits"]))
        if not boundary:
            assert limiting["action"] == "eldritch_chaos" and limiting["reason"] == "unsupported_effect"
            assert limiting["exits"] == [[1536,1]]
        elif coupled and not retention:
            assert limiting["action"] == "harvest_reforge:physical" and limiting["reason"] == "probability_envelope"
        elif not coupled:
            assert limiting["action"] == "restart" and limiting["reason"] == "fixed_independent_boundary"
        ceiling = F(limiting["cost"])+sum(F(prob)*values[t] for t,prob in limiting["exits"])
        assert donor <= ceiling and ceiling-donor < F("1e-7")
        ranked = after["ranked_constraints"]
        sources.append(dict(second_source=s["second_source"], donor=float(donor),
            prior_donor=float(prior[root]) if prior else None, donor_gain=float(donor-prior[root]) if prior else None,
            program_before=s["program_before"]["lower"], program_after=program["lower"],
            program_gain=program["lower"]-s["program_before"]["lower"],
            compatible_action_gain=s["local_compatible_gain"],
            complete_model_before=before["lower"], complete_model_after=after["lower"],
            complete_model_gain=after["lower"]-before["lower"],
            portfolio_gain=after["portfolio"]-before["portfolio"],
            exact_program=str(exact_new), **({"first_limiting_rhs_exact":str(ceiling)} if boundary else {"exact_model_ceiling":str(ceiling)}),
            limiting_relation=limiting["action"], limiting_reason=limiting["reason"],
            complete_model_ties=[r["id"] for r in ranked if r["lower"]==after["lower"]],
            next_complete_ceiling=min((r["lower"] for r in ranked if r["lower"]>after["lower"]),default=None)))
    assert native["sources"][0]["source"] != native["sources"][1]["source"]
    assert [s["program_after"]["goal_weight"] for s in native["sources"]] == [500,0]
    assert native["resources"]["reused_draw_witnesses"] == ((115 if retention else 78) if prior else 0)
    if not boundary: assert native["resources"]["new_draw_witnesses"] == 0
    budget = native["resources"].get("proof_budget_bytes",16<<20)
    assert budget in (16<<20,32<<20,64<<20) and native["resources"]["combined_additional_peak_bytes"] <= budget
    assert native["process_peak_working_set_bytes"] <= 1 << 30
    policy_ceiling = None
    if coupled:
        policy = {c:min(rs,key=lambda r:F(r["cost"])+sum(F(q)*values[t] for t,q in r["exits"])) for c,rs in by_source.items()}
        reachable, pending = set(), [1405,1369,1538]
        while pending:
            c=pending.pop()
            if c in reachable or c not in policy: continue
            reachable.add(c); pending.extend(t for t,q in policy[c]["exits"] if q)
        ids=sorted(reachable); pos={c:i for i,c in enumerate(ids)}; n=len(ids)
        assert n<=128, "ceiling control must stay small"
        matrix=[[F(int(i==j)) for j in range(n)]+[F(policy[c]["cost"])] for i,c in enumerate(ids)]
        for i,c in enumerate(ids):
            for target,q in policy[c]["exits"]:
                if target in pos: matrix[i][pos[target]]-=F(q)
                else: matrix[i][-1]+=F(q)*values[target]
        # Finite stochastic policy is proper iff every state has a path to a
        # stopping boundary. This excludes closed nonterminal bottom SCCs.
        proper={c for c in ids if any(q and target not in pos for target,q in policy[c]["exits"])}
        while True:
            expanded=proper|{c for c in ids if any(q and target in proper for target,q in policy[c]["exits"])}
            if expanded==proper: break
            proper=expanded
        assert proper==set(ids)
        for j in range(n):
            pivot=next(i for i in range(j,n) if matrix[i][j])
            matrix[j],matrix[pivot]=matrix[pivot],matrix[j]
            scale=matrix[j][j]; matrix[j]=[x/scale for x in matrix[j]]
            for i in range(n):
                if i!=j and matrix[i][j]:
                    scale=matrix[i][j]; matrix[i]=[a-scale*b for a,b in zip(matrix[i],matrix[j])]
        solved={c:matrix[pos[c]][-1] for c in ids}
        for c in ids:
            assert solved[c]>=values[c]
            assert solved[c]==F(policy[c]["cost"])+sum(F(q)*(solved[target] if target in solved else values[target]) for target,q in policy[c]["exits"])
        policy_ceiling=dict(scope="proper fixed policy of the optimistic probability-box model only; no native upper authority",
            states=n, values={str(c):str(solved[c]) for c in [1405,1369,1538]},
            decimal_values={str(c):float(solved[c]) for c in [1405,1369,1538]},
            actions={str(c):policy[c]["action"] for c in ids},
            coordinates={str(c):coordinates[c] for c in ids} if retention else {})
    return dict(evidence_scope="native C++ owns uniform conditional-history semantics; exact audit checks integer derivation, assignment bounds, complete box minima and finite inequalities",
        optimistic_policy_ceiling=policy_ceiling, joint_events=events, measured_prefix_event=measured,
        old_prefix_capacity=3050403, old_prefix_probability_upper=3050403/mass,
        probability_cap_ratio=float(F(3050403,measured["capacity"])),
        optimizer_checks=optimizer_checks, checked_relations=len(p["checked_relations"]),
        nonempty_integer_checks=len(nonempty), refill_history_checks=len(refills), annul_integer_checks=annul_checks,
        reactivated_shortcuts=len(p["price_reactivations"]), source_results=sources,
        production_authority=False)


def verify_ordinary(control_path, treatment_path, native_path):
    before,after,native=map(load_native,(control_path,treatment_path,native_path))
    assert before["pilot"]==after["pilot"]=="native-retention-ordinary-v1"
    assert before["source"]==after["source"]==native["sources"][0]["source"]
    assert before["scope"]==after["scope"]==dict(profile="calculator_product_v1",
        goal_progress_gated_reforges=1,allow_economic_restart=0,
        consider_imprint_programs=0,high_impact_executable_uppers=1)
    for run in (before,after):
        assert run["budget_ns"]==60_000_000_000 and run["proof_cap_bytes"]==32<<20
        assert run["total_cap_bytes"]==1<<30 and not run["default_enabled"]
        assert run["native_peak_bytes"]<=run["proof_cap_bytes"]
        assert max(run["process_peak_working_set_bytes"],run["peak_owned_bytes"],run["live_owned_bytes"])<=run["total_cap_bytes"]
        assert run["elapsed_ns"]>=run["budget_ns"] or run["done"]
        assert run["ordinary_setup_ns"]>=run["native_prepare_ns"]
        assert run["stepping_ns"]+run["ordinary_setup_ns"]<=run["elapsed_ns"]
        assert not run["refusal"]
    assert not before["native_prepare_attempts"] and not before["native_prepared"]
    assert after["native_prepare_attempts"]==1 and after["native_prepared"]
    assert after["native_lookups"]>=after["native_hits"]>=after["native_selected_calls"]>0
    assert after["public_lower"]==after["independent_root_floor"]==native["sources"][0]["probabilistic_donor"]
    assert after["public_lower"]>before["public_lower"]
    held={h["case"]:h for h in after["held_outs"]}
    assert set(held)=={"distinct_prefix_removed","unfractured_entry","native_exalt_natural_junk","native_bench_crafted_junk"}
    assert held["distinct_prefix_removed"]["source"]==native["sources"][1]["source"]
    for h in held.values():
        assert h["physical_lower"]==native["probabilistic_donor"]["values"][h["cell"]]
        assert h["common_lower"]>=h["lower"]>=0
        if h["uniform_projection_accepted"]:
            assert h["lower"]==h["physical_lower"]
        else:
            assert h["lower"]==0 and h["unsafe_junk_classes"]>0
    assert held["distinct_prefix_removed"]["uniform_projection_accepted"]
    assert held["unfractured_entry"]["uniform_projection_accepted"]
    # Actual native producers yield distinct natural/crafted coordinates. A
    # physical member's value is never broadcast after a coarse guard refuses.
    coords=native["probabilistic_donor"]["coordinates"]
    natural=coords[held["native_exalt_natural_junk"]["cell"]]
    crafted=coords[held["native_bench_crafted_junk"]["cell"]]
    assert natural[0]==crafted[0] and natural[1:4]==[0,0,0] and crafted[1:4]==[0,0,1]
    return dict(scope="one anchored ordinary request; read-only 60-second cooperative observations including all setup",
        public_lower_before=before["public_lower"],public_lower_after=after["public_lower"],
        public_lower_gain=after["public_lower"]-before["public_lower"],
        native_preparation_seconds=after["native_prepare_ns"]/1e9,
        native_selected_calls=after["native_selected_calls"],
        rows_before=before["rows"],rows_after=after["rows"],
        held_outs={name:{k:v for k,v in h.items() if k not in ("source","abstract_identity")} for name,h in held.items()},
        ordinary_exact_result=False,verified_upper_available=after["verified_upper"] is not None,
        speedup_established=False,default_enabled=False)


def verify_numerical_reuse(directory):
    """Matched numerical treatment; the existing native verifier owns semantics.

    Run the ordinary native verifier on proposal-warm.json to produce reference.json
    first. This comparison does not manufacture a second native certificate.
    """
    directory=Path(directory)
    read=lambda name: load_native(directory / (name + ".json"))
    cold,warm=read("proposal-cold"),read("proposal-warm")
    reference=read("reference")
    provenance=read("probe-provenance")
    def semantic(value):
        if isinstance(value,dict):
            return {k:semantic(v) for k,v in value.items() if not k.endswith("_ns")}
        if isinstance(value,list): return [semantic(v) for v in value]
        return value
    assert cold["pilot"]==warm["pilot"] and not warm["production_authority"]
    assert cold["probabilistic_donor"]==warm["probabilistic_donor"]
    assert semantic(cold["sources"])==semantic(warm["sources"])
    assert reference["checked_relations"]==len(warm["probabilistic_donor"]["checked_relations"])
    for run in (cold,warm):
        assert run["resources"]["proof_budget_bytes"]==32<<20
        assert run["resources"]["combined_additional_peak_bytes"]<=32<<20
        assert run["process_peak_working_set_bytes"]<=1<<30
    cp,wp=cold["preparation_profile"],warm["preparation_profile"]
    assert cp["cold_solve_rounds"]==29 and not cp["seeded_rounds"]
    assert wp["seeded_rounds"]==12 and wp["untrusted_rounds"]==17
    assert not any(wp[k] for k in ("cold_solve_rounds","seed_refusals","zero_fallbacks"))
    sources=[]
    for source in warm["sources"]:
        after=next(model for model in source["complete_models"] if model["treatment"])
        sources.append(dict(second_source=source["second_source"],
            donor_before=source["probabilistic_donor"],donor_after=source["probabilistic_donor"],donor_gain=0,
            program_before=source["program_after"]["lower"],program_after=source["program_after"]["lower"],program_gain=0,
            complete_model_before=after["lower"],complete_model_after=after["lower"],complete_model_gain=0,
            portfolio_before=after["portfolio"],portfolio_after=after["portfolio"],portfolio_gain=0))
    before,after=read("ordinary-cold"),read("ordinary-warm")
    for field in ("pilot","source","scope","budget_ns","total_cap_bytes","proof_cap_bytes","held_outs"):
        assert before[field]==after[field],field
    assert before["source"]==warm["sources"][0]["source"]
    for run in (before,after):
        assert run["treatment"] and not run["default_enabled"] and not run["refusal"]
        assert run["native_prepare_attempts"]==1 and run["native_prepared"]
        assert run["budget_ns"]==60_000_000_000 and run["proof_cap_bytes"]==32<<20
        assert run["total_cap_bytes"]==1<<30 and run["native_peak_bytes"]<=32<<20
        assert max(run["peak_owned_bytes"],run["process_peak_working_set_bytes"])<=1<<30
        assert run["public_lower"]==sources[0]["donor_after"]
        for held in run["held_outs"]:
            assert held["physical_lower"]==warm["probabilistic_donor"]["values"][held["cell"]]
            assert held["lower"]==(held["physical_lower"] if held["uniform_projection_accepted"] else 0)
    assert not before["numerical_reuse"] and after["numerical_reuse"]
    dc,dw=read("development-cold"),read("development-warm")
    assert dc["artifact"]==dw["artifact"]
    dc,dw=dc["cases"][0],dw["cases"][0]
    assert dc["id"]==dw["id"]=="conquest-lamellar-allflame-fractured-4-to-5-product8"
    assert {k:v for k,v in dc["input"].items() if k!="native_retention_diagnostic"}=={
        k:v for k,v in dw["input"].items() if k!="native_retention_diagnostic"}
    dev=[]
    for case,mode in ((dc,"cold"),(dw,"reuse")):
        assert case["input"]["native_retention_diagnostic"]==mode
        assert not case["errors"] and case["cap_checks"]["all_passed"]
        assert case["input"]["verification"]["runs"]==0 and case["verification"] is None
        assert case["input"]["caps"]["max_solver_owned_bytes"]==1<<30
        evaluation=case["exact_strategy_evaluation"]
        assert evaluation["completed"] and evaluation["status"]=="matched"
        assert evaluation["cost_complete"] and evaluation["zero_off_policy_mass"] and evaluation["cost_reconciled"]
        assert evaluation["success_probability"]==1 and evaluation["off_policy_mass"]==0
        summary=case["solve_summary"]
        assert summary["upper_bound"]==summary["evaluated_policy_cost"]==evaluation["total_expected_cost"]
        assert case["phase_wall_ms"]["total"]<=60_000 and not case["execution"]["watchdog_expired"]
        target=provenance["development_case"]["verified_upper_target"]
        dev.append(dict(mode=mode,total_seconds=case["phase_wall_ms"]["total"]/1000,
            lower=summary["lower_bound"],verified_upper=summary["upper_bound"],
            absolute_gap=summary["absolute_optimality_gap"],expanded_states=summary["expanded_states"],
            native_peak_owned_bytes=case["memory"]["native_peak_owned_bytes"],
            verified_upper_target=target,target_met=summary["upper_bound"]<=target+provenance["development_case"]["target_absolute_tolerance"],
            exact_closure=summary["converged"] and summary["policy_status"]=="exact",
            stop=summary["termination"]))
    fields=("elapsed_ns","native_prepare_ns","ordinary_setup_ns","rows","native_hits","native_selected_calls",
            "peak_owned_bytes","native_peak_bytes","verified_upper","done")
    reduction=1-wp["total_ns"]/cp["total_ns"]
    return dict(baseline="same-executable cold numerical initialization; same native model and complete scope",
        sources=sources,probability_cap_gain=0,checked_relations=reference["checked_relations"],
        compact=dict(cold=cp,reuse=wp,total_preparation_reduction=reduction,
            fixed_gate=provenance["criteria"]["minimum_total_preparation_reduction"],
            gate_passed=reduction>=provenance["criteria"]["minimum_total_preparation_reduction"],
            peak_bytes_before=cold["resources"]["combined_additional_peak_bytes"],
            peak_bytes_after=warm["resources"]["combined_additional_peak_bytes"],
            optimistic_policy_ceiling=reference["optimistic_policy_ceiling"]),
        ordinary=dict(control={k:before[k] for k in fields},treatment={k:after[k] for k in fields},
            public_lower_gain=after["public_lower"]-before["public_lower"],
            scope=after["scope"],held_out_coverage_unchanged=True),development=dev,
        exact_closure_gain=int(dev[1]["exact_closure"])-int(dev[0]["exact_closure"]),
        development_gap_reduction=dev[0]["absolute_gap"]-dev[1]["absolute_gap"],
        development_target_gain=int(dev[1]["target_met"])-int(dev[0]["target_met"]),default_enabled=False,
        outcome="preparation reduction only; no improved gap, target attainment or exact closure; single sequential matched observations")


if __name__ == "__main__":
    if len(sys.argv)==4 and sys.argv[1]=="--numerical-reuse":
        result=verify_numerical_reuse(sys.argv[2])
        Path(sys.argv[3]).write_bytes((json.dumps(result,indent=2)+"\n").encode("utf-8"))
        print("matched numerical reuse checks passed"); raise SystemExit(0)

    if len(sys.argv)==6 and sys.argv[1]=="--ordinary":
        result=verify_ordinary(*sys.argv[2:5])
        Path(sys.argv[5]).write_bytes((json.dumps(result,indent=2)+"\n").encode("utf-8"))
        print(json.dumps(result)); raise SystemExit(0)
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pilot = load_native(sys.argv[1]).get("pilot")
    phase = pilot == "uniform-phase-lower-v1"
    probabilistic = pilot == "native-probabilistic-lower-v1"
    joint = pilot in ("native-joint-goal-lower-v1","native-side-boundary-lower-v1")
    result = verify_joint(sys.argv[1]) if joint else (verify_probability(sys.argv[1]) if probabilistic else (verify_phase(sys.argv[1]) if phase else verify(sys.argv[1])))
    Path(sys.argv[2]).write_bytes((json.dumps(result, indent=2) + "\n").encode("utf-8"))
    if phase or probabilistic or joint:
        print(json.dumps(result))
    else:
        print(json.dumps({"checked_models": len(result["records"]),
            "normalized_roots": [r["checked_native_root"] for r in result["records"]
                                 if r["coefficient_mode"] == "normalized_stored_reference"],
            "raw_mass_defects": result["raw_mass_defects"]}))
