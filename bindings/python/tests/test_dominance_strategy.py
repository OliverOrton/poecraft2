"""Finite authored Dominance checks; no sampled runs or automatic search."""
import json
import os
from pathlib import Path

import pytest
from poecraft_engine import EngineError, load_data, load_economy

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = Path(os.environ.get("POECRAFT_TEST_ARTIFACT", ROOT / json.loads(
    (ROOT / "apps/web/runtime.lock.json").read_text())["runtime_directory"]))
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"
TARGET = "ElementalDamageCannotBeReflectedPercentUberMaven"
HELPER = "AdditionalCriticalStrikeChanceWithSpellsUber2_"


def graph(level, physical=False, tail=None):
    source = ("Physical" if physical else "Elemental") + "DamageCannotBeReflectedPercentUber1"
    nodes = [{"id": "s", "kind": "start"},
             {"id": "d", "kind": "operation", "operation": {"type": "dominance"}},
             {"id": "yes", "kind": "terminal", "terminal": "success"},
             {"id": "no", "kind": "terminal", "terminal": "failure"}]
    edges = [{"id": "begin", "from": "s", "to": "d"}]
    last = "d"
    if tail:
        nodes.append({"id": "tail", "kind": "operation", "operation": {"type": tail}})
        edges.append({"id": "continue", "from": "d", "to": "tail"})
        last = "tail"
    edges.extend([
        {"id": "hit", "from": last, "to": "yes", "priority": 0,
         "condition": {"type": "mod_count", "mod_keys": [TARGET], "min": 1}},
        {"id": "miss", "from": last, "to": "no", "priority": 999, "is_default": True}])
    return {"version": "v1", "start_node_id": "s",
            "base_state": {"base_key": BASE, "item_level": level, "rarity": "rare",
                           "generic_influence_bits": 40, "prefixes": [source], "suffixes": [HELPER]},
            "nodes": nodes, "edges": edges}


@pytest.mark.parametrize("level", [1, 86])
@pytest.mark.parametrize("physical", [False, True])
@pytest.mark.parametrize("tail", [None, "annul", "dominance"])
def test_exact_dominance_original_root_continuation(level, physical, tail):
    document = graph(level, physical, tail)
    with load_data(ARTIFACT) as data, data.create_session(BASE, level) as session:
        with session.compile_strategy(document) as strategy, load_economy(
                {"version": "v1", "prices": {"dominance": 7, "annul": 2}}) as economy:
            exact = strategy.evaluate(economy=economy)
            assert exact["converged"]
            probability = 0.5 if not physical and tail is None else 0
            assert exact["terminals"]["success"] == pytest.approx(probability)
            assert exact["terminals"]["action_not_applied"] == pytest.approx(1 if tail == "dominance" else 0)
            assert exact["terminals"]["failure"] == pytest.approx(0 if tail == "dominance" else 1 - probability)
            for key in ("stop", "no_matching_edge", "unresolved"):
                assert exact["terminals"][key] == pytest.approx(0)
            assert exact["accounting"]["totals"]["per_invocation"]["total_expected_cost"] == pytest.approx(9 if tail == "annul" else 7)
            # Recompilation preserves request input and evaluated law.
            with session.compile_strategy(json.loads(json.dumps(document))) as repeated:
                again = repeated.evaluate(economy=economy)
                assert again["terminals"] == exact["terminals"]


def test_dominance_wider_continuation_refused():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        with session.compile_strategy(graph(86, tail="veiled_exalt")) as strategy:
            with pytest.raises(EngineError, match="Dominance.*continuations"):
                strategy.evaluate()


@pytest.mark.parametrize("case", ["stop", "unmatched", "corrupted", "mirrored"])
def test_dominance_complete_absorption_and_no_apply_cost(case):
    document = graph(86)
    if case == "stop":
        next(node for node in document["nodes"] if node["id"] == "no")["terminal"] = "stop"
    elif case == "unmatched":
        document["edges"] = [edge for edge in document["edges"] if edge["id"] != "miss"]
    else:
        document["base_state"][case] = True
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        with session.compile_strategy(document) as strategy, load_economy(
                {"version": "v1", "prices": {"dominance": 7}}) as economy:
            result = strategy.evaluate(economy=economy)
            assert result["converged"]
            terminal = result["terminals"]
            expected = {key: 0 for key in ("success", "failure", "stop", "action_not_applied", "no_matching_edge", "unresolved")}
            if case in ("corrupted", "mirrored"):
                expected["action_not_applied"] = 1
            else:
                expected["success"] = 0.5
                expected["stop" if case == "stop" else "no_matching_edge"] = 0.5
            for key, probability in expected.items():
                assert terminal[key] == pytest.approx(probability)
            assert result["accounting"]["totals"]["per_invocation"]["total_expected_cost"] == pytest.approx(0 if case in ("corrupted", "mirrored") else 7)


def test_dominance_refuses_owner_unapproved_dual_lock_carrier():
    document = graph(86, tail="scour")
    document["base_state"]["prefixes"].append({
        "mod_key": "DexMasterItemGenerationCannotChangeSuffixes", "crafted": True})
    document["base_state"]["suffixes"].append({
        "mod_key": "StrMasterItemGenerationCannotChangePrefixes", "crafted": True})
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        with session.compile_strategy(document) as strategy:
            with pytest.raises(EngineError, match="Dominance.*dual-lock"):
                strategy.evaluate()
