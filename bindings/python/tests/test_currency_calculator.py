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


@pytest.mark.parametrize("name", ["Vaal Regalia", "Archdemon Crown", "Spine Bow", "Ruby Ring"])
def test_double_corruption_sequential_pairs_match_canonical_groups(name):
    with sqlite3.connect(ROOT / "data/sqlite/poecraft.db") as sql, load_data(ARTIFACT) as data:
        sql.row_factory = sqlite3.Row
        base = resolve_base_selection(sql, name, 86)
        with data.create_session(base.metadata_path, 86) as session:
            item = session.create_item("normal")
            before = bytes(item._state)
            result = session.calculate_currency(item, "double_corruption")
            conserved(result)
            assert result["double_corruption_branches"] == dict.fromkeys(
                ["implicit", "sockets", "reforge", "destroyed"], 0.25)
            assert {r["terminal"]: r["probability"] for r in result["outcomes"] if "terminal" in r} == {
                "bricked": 0.25, "destroyed": 0.25}
            assert all(not r["is_goal"] and not any(r["slots"])
                       for r in result["outcomes"] if "terminal" in r)
            weights = {r["mod"]: r["weight"] for r in result["implicit_outcomes"] if r["weight"]}
            groups = {mod: set(json.loads(sql.execute("select source_json from mod where key=?",
                (session.mod_info(mod).key,)).fetchone()[0])["groups"]) for mod in weights}
            total = sum(weights.values())
            remaining = {a: sum(w for b, w in weights.items() if a != b and not groups[a] & groups[b])
                         for a in weights}
            expected = {(a, b): 0.25 * weights[a] * weights[b] / total * (1 / remaining[a] + 1 / remaining[b])
                        for a in weights for b in weights if a < b and not groups[a] & groups[b]}
            actual = {tuple(r["mods"]): r["probability"] for r in result["implicit_pairs"]}
            assert actual == pytest.approx(expected, abs=1e-14)
            assert sum(actual.values()) == pytest.approx(0.25)
            assert sum(r["added_probability"] for r in result["implicit_outcomes"]) == pytest.approx(0.5)
            for row in result["implicit_outcomes"]:
                assert row["added_probability"] == pytest.approx(sum(p for pair, p in expected.items() if row["mod"] in pair))
            assert bytes(item._state) == before


def test_double_corruption_explicit_goal_and_existing_implicit_replacement():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item("rare")
        item.add_mod("LocalIncreasedEnergyShield11", fractured=True)
        initial = session.calculate_currency(item, "double_corruption")
        chosen = next(r for r in initial["implicit_outcomes"] if r["weight"])
        item._state.implicit_count = 1
        item._state.implicits[0].mod_id = chosen["mod"]
        before = bytes(item._state)
        result = session.calculate_currency(item, "double_corruption", goal("LocalIncreasedEnergyShield11", extras=False))
        conserved(result)
        assert result["success_probability"] == pytest.approx(0.5)
        assert result["slot_satisfied"][0] == pytest.approx(0.5)
        row = next(r for r in result["implicit_outcomes"] if r["mod"] == chosen["mod"])
        assert row["present_probability"] == pytest.approx(0.25 + row["added_probability"])
        assert bytes(item._state) == before
        item._state.item_flags |= 1
        illegal = session.calculate_currency(item, "double_corruption")
        assert not illegal["legal"] and not illegal["success_probability"]
        assert "double_corruption_branches" not in illegal
        assert sum(r["probability"] for r in illegal["outcomes"]) == pytest.approx(1)


def test_corruption_goal_matches_the_joint_item_not_separate_marginals():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item("rare")
        vaal = session.calculate_currency(item, "vaal")
        target = next(r for r in vaal["implicit_outcomes"] if r["weight"])
        key = session.mod_info(target["mod"]).key
        implicit_goal = {"version": "v1", "rarity": "rare", "slots": [], "implicit_mod_keys": [key], "corrupted": True}
        result = session.calculate_currency(item, "vaal", implicit_goal)
        conserved(result)
        assert result["success_probability"] == pytest.approx(target["added_probability"])
        joint = session.calculate_currency(item, "vaal", {**goal("LocalIncreasedEnergyShield11"), "implicit_mod_keys": [key]})
        conserved(joint)
        assert joint["slot_satisfied"][0] > 0 and joint["implicit_satisfied"][0] > 0
        assert joint["success_probability"] == 0, "Vaal cannot add an explicit and an implicit in the same branch"
        doubled = session.calculate_currency(item, "double_corruption")
        pair = doubled["implicit_pairs"][0]
        paired = session.calculate_currency(item, "double_corruption", {**implicit_goal,
            "implicit_mod_keys": [session.mod_info(id).key for id in pair["mods"]]})
        conserved(paired)
        assert paired["success_probability"] == pytest.approx(pair["probability"])
        assert session.calculate_currency(item, "double_corruption", {**implicit_goal, "corrupted": False})["success_probability"] == 0


def test_normal_actions_honor_retained_implicits_and_influence_goals():
    with load_data(ARTIFACT) as data, data.create_session("Metadata/Items/Rings/Ring1", 86) as session:
        item = session.create_item("rare", with_implicits=True)
        assert item._state.implicit_count
        key = session.mod_info(item._state.implicits[0].mod_id).key
        target = {"version": "v1", "rarity": "rare", "slots": [], "allow_extra_modifiers": True,
                  "implicit_mod_keys": [key], "influence_bits": 32}
        result = session.calculate_currency(item, "influence_exalt:shaper", target)
        conserved(result)
        assert result["success_probability"] == pytest.approx(1)
        assert session.calculate_currency(item, "influence_exalt:shaper", {**target, "influence_bits": 8})["success_probability"] == 0
        item._state.generic_influence_bits = 32
        assert session.calculate_currency(item, "chaos", target)["success_probability"] == pytest.approx(1)
        assert session.calculate_currency(item, "restart", {**target, "rarity": "normal", "influence_bits": 0})["success_probability"] == pytest.approx(1)


def test_native_item_editor_is_atomic_and_keeps_eldritch_state_consistent():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item("rare")
        mods = [session.mod_info(i) for i in range(session.mod_count)]
        enchant = next(m for m in mods if m.reach_via.startswith("retained:heist"))
        before = bytes(item._state)
        with pytest.raises(EngineError, match="not an implicit"):
            item.edit(corrupted=True, add_implicit=enchant.key)
        assert bytes(item._state) == before
        item.edit(influence_bits=40, corrupted=True)
        assert item._state.generic_influence_bits == 40 and item._state.item_flags & 1
        before = bytes(item._state)
        with pytest.raises(EngineError, match="at most two"):
            item.edit(influence_bits=41, corrupted=False)
        assert bytes(item._state) == before
        item.edit(influence_bits=0, corrupted=False)
        with session.create_action_context(123) as context:
            assert context.apply(item, {"type": "eldritch_ember", "tier": 1}).applied
        key = session.mod_info(item._state.implicits[0].mod_id).key
        item.edit(remove_implicit=key)
        assert item._state.implicit_count == 0 and item._state.searing_exarch_tier == 0
        item.edit(add_implicit=key)
        assert item._state.implicit_count == 1 and item._state.searing_exarch_tier == 1
        before = bytes(item._state)
        with pytest.raises(EngineError, match="cannot coexist"):
            item.edit(influence_bits=32)
        assert bytes(item._state) == before
        item.add_mod("LocalIncreasedEnergyShield11")
        before = bytes(item._state)
        with pytest.raises(EngineError, match="excess explicit"):
            item.edit(rarity="normal")
        assert bytes(item._state) == before
        with pytest.raises(EngineError, match="Calculator"):
            session.goal_feasibility({**goal("LocalIncreasedEnergyShield11"), "implicit_mod_keys": [key]}, item)


def test_eldritch_implicit_goal_uses_canonical_weights_and_preserves_opposite_side():
    with sqlite3.connect(ROOT / "data/sqlite/poecraft.db") as sql, load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        sql.row_factory = sqlite3.Row
        base = resolve_base_selection(sql, "Vaal Regalia", 86)
        item = session.create_item("rare")
        with session.create_action_context(55) as context:
            assert context.apply(item, {"type": "eldritch_ember", "tier": 1}).applied
            searing = session.mod_info(item._state.implicits[0].mod_id)
            assert context.apply(item, {"type": "eldritch_ichor", "tier": 1}).applied
        eater = next(session.mod_info(item._state.implicits[i].mod_id) for i in range(item._state.implicit_count)
                     if item._state.implicits[i].mod_id != searing.session_mod_id)
        tags = set(base.tags) | {"no_tier_1_eldritch_implicit"}
        sources = [json.loads(row[0]) for row in sql.execute("select source_json from mod")]
        selected_source = json.loads(sql.execute("select source_json from mod where key=?", (searing.key,)).fetchone()[0])
        eligible = [source for source in sources if source["generation_type"] == selected_source["generation_type"]]
        weight = lambda source: next((r["weight"] for r in source["spawn_weights"] if r["tag"] in tags), 0)
        expected = weight(selected_source) / sum(weight(source) for source in eligible)
        before = bytes(item._state)
        result = session.calculate_currency(item, "eldritch_ember:1", {**goal(),
            "implicit_mod_keys": [searing.key, eater.key], "influence_bits": 0, "corrupted": False})
        conserved(result)
        assert result["success_probability"] == pytest.approx(expected)
        assert result["implicit_satisfied"] == pytest.approx([expected, 1])
        assert bytes(item._state) == before


def test_awakener_goal_observes_receiver_implicits_and_union_of_influences():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 1) as session:
        donor, receiver = session.create_item("rare"), session.create_item("rare")
        donor.edit(influence_bits=32)
        receiver.edit(influence_bits=8)
        donor.add_mod("LocalIncreaseSocketedActiveGemLevelUber1")
        receiver.add_mod("AdditionalCriticalStrikeChanceWithAttacksUber1")
        target_mod = session.calculate_currency(receiver, "vaal")["implicit_outcomes"][0]["mod"]
        key = session.mod_info(target_mod).key
        receiver.edit(add_implicit=key)
        # Explicit fixture override isolates retained implicit observation.
        receiver.edit(corrupted=False)
        before = bytes(donor._state), bytes(receiver._state)
        target = {**goal("LocalIncreaseSocketedActiveGemLevelUber1", "AdditionalCriticalStrikeChanceWithAttacksUber1"),
                  "implicit_mod_keys": [key], "influence_bits": 40, "corrupted": False}
        result = session.calculate_currency(receiver, "awakener", target, donor)
        conserved(result)
        assert result["success_probability"] == pytest.approx(1)
        assert session.calculate_currency(receiver, "awakener", {**target, "influence_bits": 8}, donor)["success_probability"] == 0
        assert (bytes(donor._state), bytes(receiver._state)) == before


def test_bloodstained_fossil_observes_joint_implicit_and_corrupted_goal():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 1) as session:
        item = session.create_item("rare")
        vaal = session.calculate_currency(item, "vaal")
        chosen = next(row for row in vaal["implicit_outcomes"] if row["weight"])
        key = session.mod_info(chosen["mod"]).key
        action = "fossil:Metadata/Items/Currency/CurrencyDelveCraftingVaal"
        target = {**goal(), "requested_fossil_actions": [action], "implicit_mod_keys": [key], "corrupted": True}
        before = bytes(item._state)
        result = session.calculate_currency(item, action, target)
        conserved(result)
        assert result["success_probability"] == pytest.approx(chosen["added_probability"] * 4)
        assert session.calculate_currency(item, action, {**target, "corrupted": False})["success_probability"] == 0
        assert bytes(item._state) == before


def test_unmodeled_enchantment_effects_do_not_silently_enter_ordinary_odds():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 1) as session:
        item = session.create_item("rare")
        enchant = next(session.mod_info(i) for i in range(session.mod_count)
                       if session.mod_info(i).reach_via.startswith("retained:heist"))
        item._state.enchantment_count = 1
        item._state.enchantments[0].mod_id = enchant.session_mod_id
        with pytest.raises(EngineError, match="retained enchantments"):
            session.calculate_currency(item, "chaos", goal())
        conserved(session.calculate_currency(item, "vaal", goal()))


def test_modifier_authoring_sets_native_state_and_keeps_corrupted_fixtures_editable():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item("rare")
        chosen = next(row for row in session.calculate_currency(item, "vaal")["implicit_outcomes"] if row["weight"])
        key = session.mod_info(chosen["mod"]).key
        item.edit(add_implicit=key)
        assert item._state.item_flags & 1
        item.edit(add_explicit="LocalIncreasedEnergyShield11")
        assert item._state.prefix_count == 1 and item._state.item_flags & 1
        item.edit(add_explicit="LocalIncreaseSocketedActiveGemLevelUber1")
        assert item._state.generic_influence_bits == 32
        assert item._state.prefix_count == 2 and item._state.item_flags & 1
        before = bytes(item._state)
        with pytest.raises(EngineError, match="conflicting explicit"):
            item.edit(add_explicit="LocalIncreasedEnergyShield11")
        assert bytes(item._state) == before
        item.edit(remove_implicit=key)
        assert item._state.implicit_count == 0 and item._state.item_flags & 1
        assert not session.calculate_currency(item, "chaos", goal("LocalIncreasedEnergyShield11"))["legal"], "Editing must not weaken actual crafting legality"
        fresh = session.create_item("rare")
        before = bytes(fresh._state)
        with pytest.raises(EngineError, match="cannot coexist"):
            fresh.edit(add_explicit="LocalIncreaseSocketedActiveGemLevelUber1", fractured=True)
        assert bytes(fresh._state) == before
