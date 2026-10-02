"""Finite original-root structural cluster checks; no solver benchmark or Simulator."""
import copy
from pathlib import Path
import pytest
from poecraft_engine import EngineError, load_data, load_economy

ARTIFACT = Path(__file__).resolve().parents[3] / "data/compiled/current"
BASE = "Metadata/Items/Jewels/JewelPassiveTreeExpansionSmall"
KEY = "affliction_maximum_life"
TARGET = "AfflictionNotableFettle"


def graph(action="alteration", *, rarity="magic", prefixes=None, suffixes=None, repeat=False):
    return {"version": "v1", "start_node_id": "s",
        "base_state": {"base_key": BASE, "item_level": 75, "rarity": rarity,
            "cluster": {"passive_key": KEY, "passive_count": 2}, "with_implicits": False,
            "prefixes": prefixes or [], "suffixes": suffixes or []},
        "nodes": [{"id": "s", "kind": "start"}, {"id": "a", "kind": "operation", "operation": {"type": action}},
            {"id": "yes", "kind": "terminal", "terminal": "success"}, {"id": "no", "kind": "terminal", "terminal": "failure"}],
        "edges": [{"id": "begin", "from": "s", "to": "a"},
            {"id": "hit", "from": "a", "to": "yes", "priority": 0,
             "condition": {"type": "mod_count", "mod_keys": [TARGET], "min": 1}},
            {"id": "miss", "from": "a", "to": "a" if repeat else "no", "priority": 999, "is_default": True}]}


def fettle_probability(session, ctx):
    empty = session.create_item("magic")
    first = list(ctx.debug_pool(empty, "augment"))
    total = sum(r["final_weight"] for r in first)
    one = two = 0.
    for row in first:
        p = row["final_weight"] / total
        mod = session.mod_info(row["session_mod_id"])
        if mod.key == TARGET:
            one += p; two += p
        else:
            child = empty.copy(); child.add_mod(mod)
            rows = list(ctx.debug_pool(child, "augment", side="suffix" if mod.side == "prefix" else "prefix"))
            denom = sum(r["final_weight"] for r in rows)
            two += p * sum(r["final_weight"] for r in rows if session.mod_info(r["session_mod_id"]).key == TARGET) / denom
    return (one + two) / 2


@pytest.mark.parametrize("action", ["transmute", "alteration"])
def test_original_root_magic_graph_agrees_with_independent_draws(action):
    with load_data(ARTIFACT) as d, d.create_cluster_session(BASE, 75, passive_key=KEY, passive_count=2) as s, s.create_action_context(0) as ctx:
        expected = fettle_probability(s, ctx)
        document = graph(action, rarity="normal" if action == "transmute" else "magic")
        with s.compile_strategy(document) as strategy, load_economy({"version": "v1", "prices": {action: 3}}) as economy:
            result = strategy.evaluate(economy=economy, max_states=5000, max_pairs=10000, max_transitions=50000, max_owned_bytes=128*1024*1024)
            assert result["converged"]
            assert result["terminals"]["success"] == pytest.approx(expected, abs=1e-12)
            assert result["terminals"]["failure"] == pytest.approx(1-expected, abs=1e-12)
            assert sum(v for v in result["terminals"].values() if isinstance(v, (int, float))) == pytest.approx(1, abs=1e-12)
            assert result["accounting"]["totals"]["per_invocation"]["total_expected_cost"] == pytest.approx(3)


def test_proper_magic_renewal_cost_is_checked_from_original_root():
    # A small native pool qualifies absorption/cost without widening experiment
    # caps; the separate ilvl75 one-draw tests above qualify Fettle itself.
    with load_data(ARTIFACT) as d, d.create_cluster_session(BASE, 1, passive_key=KEY, passive_count=2) as s, s.create_action_context(0) as ctx:
        target = next(s.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(s.create_item("magic"), "augment") if s.mod_info(r["session_mod_id"]).side == "prefix")
        goal={"rarity":"magic","slots":[{"family_mod_key":target.key,"min_tier":target.family_tier_index}],"allow_extra_modifiers":True}
        p=s.calculate_currency(s.create_item("magic"),"alteration",goal)["success_probability"]
        doc=graph(repeat=True);doc["base_state"]["item_level"]=1
        doc["edges"][1]["condition"]={"type":"has_mod_family","family_mod_key":target.key,"min_tier":target.family_tier_index}
        with s.compile_strategy(doc) as strategy, load_economy({"version": "v1", "prices": {"alteration": 3}}) as economy:
            result = strategy.evaluate(economy=economy, max_states=5000, max_pairs=10000, max_transitions=100000, max_owned_bytes=128*1024*1024)
            assert result["converged"] and result["terminals"]["success"] == pytest.approx(1, abs=1e-10)
            assert result["accounting"]["totals"]["per_invocation"]["total_expected_cost"] == pytest.approx(3/p, rel=1e-9)


@pytest.mark.parametrize("action,rarity", [("augment","magic"),("regal","magic"),("exalt","rare"),
    ("foulborn_augment","magic"),("foulborn_regal","magic"),("foulborn_exalt","rare")])
def test_retained_identity_single_add_graph(action, rarity):
    with load_data(ARTIFACT) as d, d.create_cluster_session(BASE, 75, passive_key=KEY, passive_count=2) as s, s.create_action_context(0) as ctx:
        item=s.create_item(rarity)
        suffix=next(s.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(item,"exalt") if s.mod_info(r["session_mod_id"]).side=="suffix")
        item.add_mod(suffix, fractured=True)
        expected=s.calculate_currency(item,action,{"rarity":"magic" if action.endswith("augment") else "rare", "slots":[{"family_mod_key":TARGET,"min_tier":1}],"allow_extra_modifiers":True})["success_probability"]
        document=graph(action,rarity=rarity,suffixes=[{"mod_key":suffix.key,"fractured":True}])
        with s.compile_strategy(document) as strategy:
            result=strategy.evaluate(max_states=5000,max_pairs=10000,max_transitions=50000,max_owned_bytes=128*1024*1024)
            assert result["converged"]
            assert result["terminals"]["success"]==pytest.approx(expected,abs=1e-12)
            assert result["terminals"]["action_not_applied"]==pytest.approx(0)


@pytest.mark.parametrize("action,fractured,success,no_apply", [("annul",False,.5,0),("annul",True,1,0),("scour",False,0,0),("scour",True,1,0)])
def test_removal_preserves_fracture_identity_and_absorption(action, fractured, success, no_apply):
    with load_data(ARTIFACT) as d, d.create_cluster_session(BASE,75,passive_key=KEY,passive_count=2) as s,s.create_action_context(0) as ctx:
        item=s.create_item("rare"); item.add_mod(TARGET,fractured=fractured)
        suffix=next(s.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(item,"exalt") if s.mod_info(r["session_mod_id"]).side=="suffix")
        doc=graph(action,rarity="rare",prefixes=[{"mod_key":TARGET,"fractured":fractured}],suffixes=[suffix.key])
        with s.compile_strategy(doc) as strategy,load_economy({"version":"v1","prices":{action:2}}) as economy:
            result=strategy.evaluate(economy=economy,max_states=5000,max_pairs=10000,max_transitions=50000,max_owned_bytes=128*1024*1024)
            assert result["converged"]
            assert result["terminals"]["success"]==pytest.approx(success)
            assert result["terminals"]["action_not_applied"]==pytest.approx(no_apply)
            assert result["accounting"]["totals"]["per_invocation"]["total_expected_cost"]==pytest.approx(2)


@pytest.mark.parametrize("property",["corrupted","mirrored"])
def test_nonapplication_consumes_no_currency(property):
    doc=graph();doc["base_state"][property]=True
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE,75,passive_key=KEY,passive_count=2) as s,s.compile_strategy(doc) as strategy,load_economy({"version":"v1","prices":{"alteration":3}}) as economy:
        result=strategy.evaluate(economy=economy)
        assert result["terminals"]["action_not_applied"]==pytest.approx(1)
        assert result["accounting"]["totals"]["per_invocation"]["total_expected_cost"]==pytest.approx(0)


@pytest.mark.parametrize("mutation",["missing","key","count","fraction"])
def test_strategy_configuration_cannot_cross_session(mutation):
    doc=graph()
    if mutation=="missing": del doc["base_state"]["cluster"]
    else: doc["base_state"]["cluster"]["passive_key" if mutation=="key" else "passive_count"]={"key":"affliction_armour","count":3,"fraction":2.5}[mutation]
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE,75,passive_key=KEY,passive_count=2) as s:
        with pytest.raises(EngineError,match="cluster.*identity"):
            s.compile_strategy(doc)


def test_unsupported_operation_and_strand_context_refuse():
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE,75,passive_key=KEY,passive_count=2) as s:
        with pytest.raises(EngineError,match="Memory operation.*unresolved"):
            s.compile_strategy(graph("remembrance"))
        doc=graph();doc["base_state"]["memory_strands"]=1
        with s.compile_strategy(doc) as strategy:
            with pytest.raises(EngineError,match="unsupported item context"):
                strategy.evaluate()


def test_physical_fettle_renewal_reports_transition_cap():
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE,75,passive_key=KEY,passive_count=2) as s,s.compile_strategy(graph(repeat=True)) as strategy:
        with pytest.raises(EngineError,match="max_transitions"):
            strategy.evaluate(max_states=5000,max_pairs=10000,max_transitions=100000,max_owned_bytes=128*1024*1024)
