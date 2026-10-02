"""Current-law refusal, distinct from legacy compiled mirror-flag transport."""
import json
import os
from pathlib import Path

import pytest
from poecraft_engine import EngineError, load_data

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = Path(os.environ.get("POECRAFT_TEST_ARTIFACT", ROOT / "data/compiled/current"))
FRACTURED = "Metadata/Items/Currency/CurrencyDelveCraftingMirror"
ORDINARY = "Metadata/Items/Currency/CurrencyDelveCraftingRandom"
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"


def current_artifact():
    data = json.loads((ARTIFACT / "game-data.json").read_text(encoding="utf-8"))["fossils"]
    strings = json.loads((ARTIFACT / "strings.json").read_text(encoding="utf-8"))["strings"]
    return any(strings[key] == FRACTURED and not data["mirrors"][i]
               for i, key in enumerate(data["key_string_ids"]))


@pytest.mark.skipif(not ARTIFACT.exists(), reason="compiled artifact absent")
@pytest.mark.parametrize("loadout", [[FRACTURED], [ORDINARY, FRACTURED], [FRACTURED, ORDINARY]])
@pytest.mark.parametrize("carrier", ["ordinary", "fractured", "influenced"])
def test_current_fossil_loadout_refuses_without_item_or_rng_change(loadout, carrier):
    if not current_artifact():
        pytest.skip("legacy mirroring artifact; positive behavior tested separately")
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        with session.create_action_context(99) as context, session.create_action_context(99) as control:
            item = session.create_item("rare", with_implicits=False)
            if carrier == "fractured":
                item.add_mod("LocalIncreasedEnergyShield11", fractured=True)
            if carrier == "influenced":
                item.edit(influence_bits=4)
                assert item.generic_influence_bits == 4
            expected = item.copy()
            before = bytes(item._state)
            action = {"type": "fossil", "fossils": loadout}
            with pytest.raises(EngineError, match="Fractured Fossil is unavailable"):
                context.apply(item, action)
            with pytest.raises(EngineError, match="Fractured Fossil is unavailable"):
                context.debug_pool(item, action)
            assert bytes(item._state) == before
            allowed = {"type": "fossil", "fossils": [ORDINARY]}
            assert context.apply(item, allowed).applied
            assert control.apply(expected, allowed).applied
            assert bytes(item._state) == bytes(expected._state)


@pytest.mark.skipif(not ARTIFACT.exists(), reason="compiled artifact absent")
@pytest.mark.parametrize("loadout", [[FRACTURED], [ORDINARY, FRACTURED]])
def test_current_fossil_calculator_and_authored_graph_refuse(loadout):
    if not current_artifact():
        pytest.skip("legacy mirroring artifact")
    action = "fossil:" + "+".join(sorted(loadout))
    goal = {"version": "v1", "rarity": "rare", "slots": [
        {"family_mod_key": "LocalIncreasedEnergyShield11", "min_tier": 0}
    ], "actions": [action]}
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item(with_implicits=False)
        before = bytes(item._state)
        with pytest.raises(EngineError, match="Fractured Fossil is unavailable"):
            session.calculate_currency(item, action, goal)
        assert bytes(item._state) == before
        strategy = {"version": "v1", "name": "Unqualified fossil", "start_node_id": "start",
            "base_state": {"base_key": BASE, "item_level": 86, "rarity": "normal"},
            "nodes": [{"id": "start", "kind": "start"},
                {"id": "craft", "kind": "operation", "operation": {"type": "fossil", "params": {"fossils": loadout}}},
                {"id": "success", "kind": "terminal", "terminal": "success"}],
            "edges": [{"id": "begin", "from": "start", "to": "craft", "is_default": True},
                {"id": "done", "from": "craft", "to": "success", "is_default": True}]}
        with pytest.raises(EngineError, match="Fractured Fossil is unavailable"):
            session.compile_strategy(strategy)
