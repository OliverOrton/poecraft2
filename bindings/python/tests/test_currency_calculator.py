"""Exact single-action witnesses using the current canonical artifact."""
import json
import math
from pathlib import Path
import sqlite3

import pytest
from poecraft_engine import EngineError, load_data
from poecraft_ingest.engine_selection import resolve_base_selection

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = ROOT / json.loads((ROOT / "apps/web/runtime.lock.json").read_text())["runtime_directory"]
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"
PAIRS = [
    ("LocalIncreaseSocketedActiveGemLevelUber1", "LocalIncreaseSocketedActiveGemLevelUberMaven"),
    ("AdditionalCriticalStrikeChanceWithSpellsUber2_", "AdditionalCriticalStrikeChanceWithSpellsUberMaven"),
    ("PercentageIntelligenceUber2", "PercentageIntelligenceUberMaven__"),
]


def goal(*keys, extras=True, threshold=None):
    return {"version": "v1", "rarity": "rare", "fossil_mode": "goal_relevant",
            "allow_extra_modifiers": extras,
            **({"min_satisfied_slots": threshold} if threshold else {}),
            "slots": [{"family_mod_key": key} for key in keys]}


def conserved(result):
    assert result["supported"] and result["legal"]
    assert math.isclose(sum(r["probability"] for r in result["outcomes"]), 1, abs_tol=1e-12)
    assert result["success_probability"] == pytest.approx(sum(
        r["probability"] for r in result["outcomes"] if r["is_goal"]), abs=1e-12)


def test_dominance_ordered_pairs_threshold_and_clean_goal():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 1) as session:
        item = session.create_item("rare")
        for original, _ in PAIRS: item.add_mod(original)
        before = bytes(item._state)
        result = session.calculate_currency(item, "dominance", goal(PAIRS[0][1], PAIRS[1][0]))
        conserved(result)
        assert result["success_probability"] == pytest.approx(1 / 6)
        assert result["slot_satisfied"][:2] == pytest.approx([1 / 3, 1 / 3])
        either = session.calculate_currency(item, "dominance", goal(PAIRS[0][1], PAIRS[1][0], threshold=1))
        assert either["success_probability"] == pytest.approx(1 / 2)
        clean = session.calculate_currency(item, "dominance", goal(PAIRS[0][1], extras=False))
        assert clean["success_probability"] == 0
        assert bytes(item._state) == before
        item._state.prefixes[0].flags = 1
        protected = session.calculate_currency(item, "dominance", goal(PAIRS[0][1]))
        assert protected["success_probability"] == 0


def test_dominance_already_elevated_and_illegal_input():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 1) as session:
        item = session.create_item("rare")
        for _, elevated in PAIRS[:2]: item.add_mod(elevated)
        result = session.calculate_currency(item, "dominance", goal(PAIRS[0][1], extras=False))
        conserved(result)
        assert result["success_probability"] == pytest.approx(0.5)
        item._state.item_flags |= 1
        assert not session.calculate_currency(item, "dominance")["legal"]
        fresh = session.create_item("normal")
        assert not session.calculate_currency(fresh, "dominance")["legal"]


def test_awakener_retained_pair_selection_cross_session_and_no_consumption():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as ds, data.create_session(BASE, 1) as rs:
        donor = ds.create_item("rare")
        receiver = rs.create_item("rare")
        donor._state.generic_influence_bits = 32  # Shaper
        receiver._state.generic_influence_bits = 8  # Elder
        donor_mods = [ds.find_mod(PAIRS[0][0]), ds.find_mod(PAIRS[1][0])]
        excluded = {m.primary_group_id for m in donor_mods}
        receiver_mods = []
        for i in range(rs.mod_count):
            mod = rs.mod_info(i)
            if "elder" in mod.reach_via and mod.generation_type in (0, 1) and mod.required_level > 1 and mod.primary_group_id not in excluded:
                receiver_mods.append(mod)
                excluded.add(mod.primary_group_id)
                if len(receiver_mods) == 2: break
        assert len(receiver_mods) == 2
        for mod in donor_mods: donor.add_mod(mod)
        for mod in receiver_mods: receiver.add_mod(mod)
        before = bytes(donor._state), bytes(receiver._state)
        result = rs.calculate_currency(receiver, "awakener", goal(donor_mods[0].key, receiver_mods[0].key), donor)
        conserved(result)
        assert result["success_probability"] == pytest.approx(0.25)
        assert result["slot_satisfied"][:2] == pytest.approx([0.5, 0.5])
        assert all(r["prefixes"] + r["suffixes"] >= 4 for r in result["outcomes"])
        assert (bytes(donor._state), bytes(receiver._state)) == before
        with pytest.raises(ValueError, match="distinct"):
            rs.calculate_currency(receiver, "awakener", donor=receiver)
        receiver._state.generic_influence_bits = 32
        with pytest.raises(EngineError, match="distinct single influences"):
            rs.calculate_currency(receiver, "awakener", donor=donor)


@pytest.mark.parametrize("name", ["Vaal Regalia", "Archdemon Crown", "Spine Bow", "Ruby Ring"])
def test_vaal_canonical_weights_and_full_branch_mass(name):
    with sqlite3.connect(ROOT / "data/sqlite/poecraft.db") as sql, load_data(ARTIFACT) as data:
        sql.row_factory = sqlite3.Row
        base = resolve_base_selection(sql, name, 86)
        with data.create_session(base.metadata_path, 86) as session:
            item = session.create_item("normal")
            before = bytes(item._state)
            result = session.calculate_currency(item, "vaal")
            conserved(result)
            assert result["vaal_branches"] == dict.fromkeys(["implicit", "sockets", "reforge", "unchanged"], 0.25)
            rolls = [r for r in result["implicit_outcomes"] if r["weight"]]
            total = sum(r["weight"] for r in rolls)
            assert sum(r["added_probability"] for r in rolls) == pytest.approx(0.25)
            for row in rolls:
                mod = session.mod_info(row["mod"])
                source = json.loads(sql.execute("select source_json from mod where key=?", (mod.key,)).fetchone()[0])
                weight = next((r["weight"] for r in source["spawn_weights"] if r["tag"] in base.tags), 0)
                assert row["weight"] == weight > 0
                assert row["added_probability"] == pytest.approx(0.25 * weight / total)
            assert sum(r["probability"] for r in result["outcomes"] if r["rarity"] == 2) == pytest.approx(0.25)
            assert bytes(item._state) == before


def test_vaal_existing_implicit_survival_and_fracture_retention():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item("rare")
        item.add_mod("LocalIncreasedEnergyShield11", fractured=True)
        initial = session.calculate_currency(item, "vaal", goal("LocalIncreasedEnergyShield11"))
        chosen = next(r for r in initial["implicit_outcomes"] if r["weight"])
        item._state.implicit_count = 1
        item._state.implicits[0].mod_id = chosen["mod"]
        result = session.calculate_currency(item, "vaal", goal("LocalIncreasedEnergyShield11"))
        conserved(result)
        assert result["success_probability"] == pytest.approx(1)
        row = next(r for r in result["implicit_outcomes"] if r["mod"] == chosen["mod"])
        assert row["present_probability"] == pytest.approx(0.75 + row["added_probability"])
        item._state.memory_strands = 1
        with pytest.raises(EngineError, match="memory"):
            session.calculate_currency(item, "vaal")
