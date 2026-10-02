"""Real-data boundary witnesses for the approved currency expansion."""
import json
import math
import os
from pathlib import Path
import sqlite3

import pytest
from poecraft_engine import load_data, load_economy, SimulationOptions
from poecraft_ingest.engine_selection import resolve_base_selection, select_session_mods

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = Path(os.environ.get("POECRAFT_TEST_ARTIFACT", ROOT / json.loads(
    (ROOT / "apps/web/runtime.lock.json").read_text())["runtime_directory"]))
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"


@pytest.mark.parametrize("name", ["Jewelled Foil", "Vaal Greatsword", "Siege Axe", "Karui Maul", "Spine Bow", "Archdemon Crown", "Vaal Regalia"])
@pytest.mark.parametrize("level", [1, 68, 86])
def test_canonical_selectors_reach_native_session(name, level):
    with sqlite3.connect(f"file:{ROOT / 'data/sqlite/poecraft.db'}?mode=ro", uri=True) as c, load_data(ARTIFACT) as data:
        c.row_factory = sqlite3.Row
        base = resolve_base_selection(c, name, level)
        expected = {m.key for m in select_session_mods(c, base)}
        with data.create_session(base.metadata_path, level) as session, session.create_action_context(19) as ctx:
            mods = [session.mod_info(i) for i in range(session.mod_count)]
            assert {m.key for m in mods if m.reach_kind in (0, 1)} == expected
            if level == 86:
                assert len({m.reach_influence for m in mods if m.reach_kind == 1}) == 6
            item = session.create_item("rare")
            assert not any(row["reach_kind"] == 1 for row in ctx.debug_pool(item, "exalt"))
            if level == 86:
                for influence in ["shaper", "elder", "crusader", "hunter", "redeemer", "warlord"]:
                    made = item.copy()
                    assert ctx.apply(made, {"type": "influence_exalt", "influence": influence}).applied
                    assert made.explicit_count == 1 and made.generic_influence_bits


@pytest.mark.parametrize("action,rarity", [("foulborn_augment", "magic"), ("foulborn_regal", "magic"), ("foulborn_exalt", "rare")])
def test_foulborn_execution_exact_strategy_and_cost(action, rarity):
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(93) as ctx:
        item = session.create_item(rarity)
        pool_item = session.create_item("rare" if action == "foulborn_regal" else rarity)
        rows = ctx.debug_pool(pool_item, action)
        assert rows and sum(row["final_weight"] for row in rows) > 0
        expected = {r["session_mod_id"] for r in rows}
        for _ in range(100):
            made = item.copy()
            assert ctx.apply(made, action).applied
            assert set(made.prefix_mod_ids + made.suffix_mod_ids) <= expected
        graph = {"version": "v1", "name": action, "start_node_id": "start",
            "base_state": {"base_key": BASE, "item_level": 86, "rarity": rarity},
            "nodes": [{"id": "start", "kind": "start"},
                {"id": "craft", "kind": "operation", "operation": {"type": action, "params": {}}},
                {"id": "success", "kind": "terminal", "terminal": "success"}],
            "edges": [{"id": "begin", "from": "start", "to": "craft"},
                {"id": "done", "from": "craft", "to": "success"}]}
        with session.compile_strategy(graph) as strategy, load_economy({"version": "v1", "prices": {action: 7}}) as economy:
            exact = strategy.evaluate(economy=economy)
            assert math.isclose(exact["expected_actions"], 1)
            assert math.isclose(exact["accounting"]["totals"]["per_invocation"]["known_expected_cost"], 7)
            with strategy.create_simulator(economy) as sim:
                result = sim.run(SimulationOptions(target_runs=1000, seed=93))
                assert result.summary["success_count"] == 1000
                assert result.summary["known_total_cost"] == 7000


def test_awakener_transfers_stable_tiers_and_consumes_donor_atomically():
    from poecraft_engine import EngineError
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as ds, data.create_session("Metadata/Items/Armours/BodyArmours/BodyInt16", 1) as rs, rs.create_action_context(51) as ctx:
        donor = ds.create_item("rare")
        receiver = rs.create_item("rare")
        dm = next(ds.mod_info(i) for i in range(ds.mod_count) if ds.mod_info(i).reach_kind == 1 and ds.mod_info(i).reach_influence == 6 and ds.mod_info(i).required_level > 1)
        rm = next(rs.mod_info(i) for i in range(rs.mod_count) if rs.mod_info(i).reach_kind in (1, 10) and rs.mod_info(i).reach_influence in (-1, 4) and "elder" in rs.mod_info(i).reach_via and rs.mod_info(i).primary_group_id != dm.primary_group_id)
        donor.add_mod(dm); receiver.add_mod(rm)
        donor._state.generic_influence_bits = 32
        receiver._state.generic_influence_bits = 8
        receiver._state.quality = 20
        original = bytes(receiver._state)
        with pytest.raises(EngineError, match="distinct|self-donation"):
            ctx.apply_multi("awakener", [("same", "donor", donor), ("same", "receiver", receiver)])
        assert bytes(receiver._state) == original and donor.lifecycle == 0
        result = ctx.apply_multi("awakener", [("donor-1", "donor", donor), ("receiver-1", "receiver", receiver)])
        assert donor.lifecycle == 1 and receiver.lifecycle == 0
        assert receiver.generic_influence_bits == 40 and receiver._state.quality == 20
        keys = {rs.mod_info(i).key for i in receiver.prefix_mod_ids + receiver.suffix_mod_ids}
        assert dm.key in keys and rm.key in keys
        retained = rs.find_mod(dm.key)
        assert retained.reach_kind == 10
        empty = rs.create_item("rare"); empty._state.generic_influence_bits = 40
        assert retained.session_mod_id not in {r["session_mod_id"] for r in ctx.debug_pool(empty, "exalt")}
        assert result["cost_keys"] == ["awakener"]
        assert [r["effect"] for r in result["resources"]] == [3, 1]
        current = bytes(receiver._state)
        with pytest.raises(EngineError, match="live"):
            ctx.apply_multi("awakener", [("donor-1", "donor", donor), ("receiver-1", "receiver", receiver)])
        assert bytes(receiver._state) == current


def test_memory_state_refusal_and_imprint():
    from poecraft_engine import EngineError
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(1) as ctx:
        item = session.create_item("magic")
        assert item.memory_strands == 0
        for bad in [-1, 101, 1.5, True]:
            with pytest.raises(ValueError): item.memory_strands = bad
        item.memory_strands = 73
        assert item.copy().memory_strands == 73
        before = bytes(item._state)
        for action in ["augment", "foulborn_augment", "remembrance", "unravelling"]:
            with pytest.raises(EngineError, match="unavailable|unresolved"):
                ctx.apply(item, action)
            assert bytes(item._state) == before
        compound = session.create_bestiary_state(item)
        assert compound.apply("bestiary:imprint").applied
        item.memory_strands = 12
        assert compound.apply("bestiary:restore_imprint").applied
        assert item.memory_strands == 73


def test_authored_resources_are_acquired_charged_and_consumed():
    from poecraft_engine import EngineError
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        dm = next(session.mod_info(i) for i in range(session.mod_count) if session.mod_info(i).reach_influence == 6)
        rm = next(session.mod_info(i) for i in range(session.mod_count) if session.mod_info(i).reach_influence == 4 and session.mod_info(i).primary_group_id != dm.primary_group_id)
        def state(mod, bits):
            return {"base_key": BASE, "item_level": 86, "rarity": "rare", "generic_influence_bits": bits,
                    "prefixes" if mod.generation_type == 0 else "suffixes": [{"mod_key": mod.key}]}
        graph = {"version": "v1", "name": "acquire donor then awaken", "start_node_id": "start",
            "base_state": state(rm, 8),
            "resources": [{"id": "donor", "base_state": state(dm, 32), "acquisition_price_key": "resource:donor"}],
            "nodes": [{"id": "start", "kind": "start"},
                {"id": "buy", "kind": "operation", "operation": {"type": "acquire_resource", "params": {"resource_id": "donor"}}},
                {"id": "awaken", "kind": "operation", "operation": {"type": "awakener", "params": {"roles": {"donor": "donor", "receiver": "current"}}}},
                {"id": "done", "kind": "terminal", "terminal": "success"}],
            "edges": [{"id": "a", "from": "start", "to": "buy"}, {"id": "b", "from": "buy", "to": "awaken"}, {"id": "c", "from": "awaken", "to": "done"}]}
        with session.compile_strategy(graph) as strategy, load_economy({"version": "v1", "prices": {"resource:donor": 11, "awakener": 7}}) as economy:
            with pytest.raises(EngineError, match="inventory|multi-item"):
                strategy.evaluate(economy=economy)
            with strategy.create_simulator(economy) as simulator:
                result = simulator.run(SimulationOptions(target_runs=1000, seed=52))
                assert result.summary["success_count"] == 1000
                assert result.summary["known_total_cost"] == 18000
                resource = result.traces[0].entries[-1].resources[0]
                assert resource["acquisitions"] == 1 and resource["lifecycle"] == 1
        # Restoring the receiver's Imprint cannot recreate its spent donor.
        import copy
        restored = copy.deepcopy(graph)
        restored["base_state"]["rarity"] = "magic"
        restored["nodes"].extend([
            {"id": "imprint", "kind": "operation", "operation": {"type": "bestiary:imprint"}},
            {"id": "restore", "kind": "operation", "operation": {"type": "bestiary:restore_imprint"}},
            {"id": "again", "kind": "operation", "operation": copy.deepcopy(graph["nodes"][2]["operation"])}])
        restored["edges"] = [{"id": str(i), "from": a, "to": b} for i,(a,b) in enumerate(zip(
            ["start", "imprint", "buy", "awaken", "restore", "again"],
            ["imprint", "buy", "awaken", "restore", "again", "done"]))]
        imprint = next(action for action in data.bestiary_actions if action.id == "bestiary:imprint")
        prices = {"resource:donor": 11, "awakener": 7, **{key: 1 for key in imprint.cost_keys}}
        with session.compile_strategy(restored) as strategy, load_economy({"version": "v1", "prices": prices}) as economy, strategy.create_simulator(economy) as sim:
            result = sim.run(SimulationOptions(target_runs=1000, seed=52))
            assert result.summary["success_count"] == 0
            assert result.summary["known_total_cost"] == (18 + len(imprint.cost_keys)) * 1000
            assert result.traces[0].entries[-1].resources[0]["lifecycle"] == 1
            assert result.traces[0].entries[-1].resources[0]["acquisitions"] == 1
        graph["edges"][0]["to"] = "awaken"
        with session.compile_strategy(graph) as strategy, load_economy({"version": "v1", "prices": {"awakener": 7}}) as economy, strategy.create_simulator(economy) as simulator:
            result = simulator.run(SimulationOptions(target_runs=1000, seed=52))
            assert result.summary["success_count"] == 0
            assert result.summary["known_total_cost"] == 0


@pytest.mark.parametrize("action,reason", [("tempering", "weights"), ("tailoring", "weights"), ("double_corruption", "reforge"), ("remembrance", "distribution"), ("unravelling", "probabilities")])
def test_unknown_currency_laws_refuse_before_mutation(action, reason):
    from poecraft_engine import EngineError
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(1) as ctx:
        item = session.create_item("rare"); before = bytes(item._state)
        with pytest.raises(EngineError, match=reason): ctx.apply(item, action)
        assert bytes(item._state) == before
        with pytest.raises(EngineError, match="single explicit-mod pool"): ctx.debug_pool(item, action)


def test_vaal_socketless_branch_and_refusal_contract():
    from poecraft_engine import EngineError
    with sqlite3.connect(f"file:{ROOT / 'data/sqlite/poecraft.db'}?mode=ro", uri=True) as c:
        c.row_factory = sqlite3.Row
        base = resolve_base_selection(c, "Metadata/Items/Belts/Belt3", 86)
    with load_data(ARTIFACT) as data, data.create_session(base.metadata_path, 86) as session, session.create_action_context(920) as ctx:
        counts = {"implicit": 0, "reforge": 0, "unchanged": 0}
        item = session.create_item("normal")
        implicit = tuple(s.mod_id for s in item._state.implicits[:item._state.implicit_count])
        for _ in range(1000):
            made = item.copy()
            assert ctx.apply(made, "vaal").applied
            assert made._state.item_flags & 1
            if made.explicit_count: counts["reforge"] += 1; assert made.explicit_count == 6
            elif tuple(s.mod_id for s in made._state.implicits[:made._state.implicit_count]) != implicit: counts["implicit"] += 1
            else: counts["unchanged"] += 1
            assert not ctx.apply(made, "vaal").applied
        assert 180 < counts["implicit"] < 320 and 180 < counts["reforge"] < 320
        assert 420 < counts["unchanged"] < 580
        graph = {"version": "v1", "start_node_id": "s", "base_state": {"base_key": base.metadata_path, "item_level": 86},
            "nodes": [{"id": "s", "kind": "start"}, {"id": "v", "kind": "operation", "operation": {"type": "vaal"}},
                {"id": "t", "kind": "terminal", "terminal": "success"}],
            "edges": [{"id": "a", "from": "s", "to": "v"}, {"id": "b", "from": "v", "to": "t"}]}
        with session.compile_strategy(graph) as strategy, load_economy({"version": "v1", "prices": {"vaal": 3}}) as economy:
            with pytest.raises(EngineError, match="corruption.*Pro"): strategy.evaluate(economy=economy)
            with strategy.create_simulator(economy) as sim:
                result = sim.run(SimulationOptions(target_runs=1000, seed=920))
                assert result.summary["known_total_cost"] == 3000 and result.summary["success_count"] == 1000
    with load_data(ARTIFACT) as data, data.create_session("Metadata/Items/Jewels/JewelStr", 86) as session, session.create_action_context(920) as ctx:
        item = session.create_item("rare"); before = bytes(item._state)
        with pytest.raises(EngineError, match="jewel.*unique"): ctx.apply(item, "vaal")
        assert bytes(item._state) == before


@pytest.mark.parametrize("base", [BASE, "Metadata/Items/Weapons/OneHandWeapons/OneHandSwords/OneHandSword14",
    "Metadata/Items/Rings/Ring15", "Metadata/Items/Quivers/QuiverNew7", "Metadata/Items/Amulets/Amulet1"])
@pytest.mark.parametrize("level", [1, 68, 86])
def test_vaal_equipment_affix_projection(base, level):
    with load_data(ARTIFACT) as data, data.create_session(base, level) as session, session.create_action_context(920) as ctx:
        item = session.create_item("normal")
        item._state.socket_count = 6
        item._state.socket_colors[:] = [0, 1, 2, 3, 0, 0]
        item._state.link_mask = 31
        item._state.quality = 20
        branches = [0, 0, 0]
        original = tuple(s.mod_id for s in item._state.implicits[:item._state.implicit_count])
        for _ in range(1000):
            made = item.copy()
            assert ctx.apply(made, "vaal").applied
            assert made._state.item_flags & 1
            assert made._state.socket_count == 6 and made._state.link_mask == 31
            assert list(made._state.socket_colors) == [0, 1, 2, 3, 0, 0]
            assert made._state.quality == 20
            if made.explicit_count:
                branches[0] += 1
                assert made.explicit_count == (5 if level == 1 and "Quiver" in base else 6)
                assert all(s.roll_count for s in list(made._state.prefixes[:made._state.prefix_count]) +
                           list(made._state.suffixes[:made._state.suffix_count]))
            elif tuple(s.mod_id for s in made._state.implicits[:made._state.implicit_count]) != original:
                branches[1] += 1
            else: branches[2] += 1
        assert all(180 < n < 320 for n in branches[:2]) and 420 < branches[2] < 580


def test_vaal_preserves_locked_values_and_enchantments():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(29) as ctx:
        item = session.create_item("rare")
        item.add_mod("LocalIncreasedEnergyShield11")
        item.add_mod("StrMasterItemGenerationCannotChangePrefixes")
        item._state.prefixes[0].roll_count = 1
        item._state.prefixes[0].rolls[0] = 123
        enchant = next(session.mod_info(i) for i in range(session.mod_count) if session.mod_info(i).reach_kind == 12)
        item._state.enchantment_count = 1
        item._state.enchantments[0].mod_id = enchant.session_mod_id
        def values(slot):
            # C struct padding is not part of persisted item identity.
            return (slot.mod_id, slot.flags, slot.roll_count, tuple(slot.rolls),
                    slot.veiled_option_count, tuple(slot.veiled_option_mod_ids), slot.veiled_chosen_mod_id)
        prefix, enchanted = values(item._state.prefixes[0]), values(item._state.enchantments[0])
        for _ in range(100):
            made = item.copy()
            assert ctx.apply(made, "vaal").applied
            assert values(made._state.prefixes[0]) == prefix
            assert values(made._state.enchantments[0]) == enchanted


def test_dominance_upgrade_removal_values_and_protection():
    # T1/elevated explicit mappings remain usable below their roll item level.
    pairs = [
        ("LocalIncreaseSocketedActiveGemLevelUber1", "LocalIncreaseSocketedActiveGemLevelUberMaven"),
        ("AdditionalCriticalStrikeChanceWithSpellsUber2_", "AdditionalCriticalStrikeChanceWithSpellsUberMaven"),
        ("PercentageIntelligenceUber2", "PercentageIntelligenceUberMaven__"),
    ]
    with load_data(ARTIFACT) as data, data.create_session(BASE, 1) as session, session.create_action_context(194) as ctx:
        mods = [session.find_mod(a) for a, _ in pairs]
        elevated = {session.find_mod(b).session_mod_id for _, b in pairs}
        item = session.create_item("rare")
        for mod in mods: item.add_mod(mod)
        item._state.generic_influence_bits = 32
        counts = {}
        for _ in range(1000):
            made = item.copy()
            assert ctx.apply(made, "dominance").applied
            assert made.explicit_count == 2
            ids = set(made.prefix_mod_ids + made.suffix_mod_ids)
            upgraded = next(iter(ids & elevated))
            removed = next(m.session_mod_id for m in mods if m.session_mod_id not in ids and
                           session.find_mod(pairs[mods.index(m)][1]).session_mod_id != upgraded)
            counts[upgraded, removed] = counts.get((upgraded, removed), 0) + 1
            slot = next(s for s in list(made._state.prefixes[:made._state.prefix_count]) +
                        list(made._state.suffixes[:made._state.suffix_count]) if s.mod_id == upgraded)
            assert slot.roll_count > 0
        assert len(counts) == 6 and all(110 < n < 230 for n in counts.values())
        # Fractured affixes and their concrete values are not selectable.
        item._state.prefixes[0].flags = 1
        item._state.prefixes[0].roll_count = 1
        item._state.prefixes[0].rolls[0] = 17
        protected = bytes(item._state.prefixes[0])
        made = item.copy()
        assert ctx.apply(made, "dominance").applied
        assert bytes(made._state.prefixes[0]) == protected
        assert not ctx.apply(made, "dominance").applied
        # A tier upgrade can exceed the base's item level and is only one step.
        low = session.create_item("rare")
        low.add_mod("AdditionalCriticalStrikeChanceWithSpellsUber1_")
        low.add_mod(pairs[0][0])
        seen = set()
        for _ in range(30):
            made = low.copy()
            assert ctx.apply(made, "dominance").applied
            seen.update(session.mod_info(i).key for i in made.prefix_mod_ids + made.suffix_mod_ids)
        assert seen == {pairs[1][0], pairs[0][1]}
        locked = session.create_item("rare")
        for mod in mods: locked.add_mod(mod)
        locked.add_mod("StrMasterItemGenerationCannotChangePrefixes")
        assert ctx.apply(locked, "dominance").applied
        assert locked.prefix_mod_ids == (mods[0].session_mod_id,)
        # An already elevated modifier stays elevated when selected again.
        item = session.create_item("rare")
        for _, key in pairs[:2]: item.add_mod(session.find_mod(key))
        for _ in range(30):
            made = item.copy()
            assert ctx.apply(made, "dominance").applied
            assert set(made.prefix_mod_ids + made.suffix_mod_ids) <= elevated
            assert made.explicit_count == 1


def test_dominance_authored_strategy_costs_and_exact_boundary():
    from poecraft_engine import EngineError
    graph = {"version": "v1", "start_node_id": "s",
        "base_state": {"base_key": BASE, "item_level": 86, "rarity": "rare", "generic_influence_bits": 32,
            "prefixes": [{"mod_key": "LocalIncreaseSocketedActiveGemLevelUber1"}],
            "suffixes": [{"mod_key": "AdditionalCriticalStrikeChanceWithSpellsUber2_"}]},
        "nodes": [{"id": "s", "kind": "start"},
            {"id": "d", "kind": "operation", "operation": {"type": "dominance"}},
            {"id": "t", "kind": "terminal", "terminal": "success"}],
        "edges": [{"id": "a", "from": "s", "to": "d"}, {"id": "b", "from": "d", "to": "t"}]}
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.compile_strategy(graph) as strategy:
        with load_economy({"version": "v1", "prices": {"dominance": 7}}) as economy:
            exact = strategy.evaluate(economy=economy)
            assert exact["converged"] and exact["terminals"]["success"] == pytest.approx(1)
            assert exact["accounting"]["totals"]["per_invocation"]["total_expected_cost"] == pytest.approx(7)
            with strategy.create_simulator(economy) as sim:
                result = sim.run(SimulationOptions(target_runs=1000, seed=194))
                assert result.summary["success_count"] == 1000
                assert result.summary["known_total_cost"] == 7000


@pytest.mark.parametrize("name,sources", [
    ("Vaal Regalia", {"heist", "harvest"}),
    ("Archdemon Crown", {"labyrinth", "harvest"}),
    ("Sorcerer Gloves", {"labyrinth", "harvest"}),
    ("Sorcerer Boots", {"labyrinth", "harvest"}),
    ("Metadata/Items/Belts/Belt4", {"labyrinth"}),
    ("Iron Ring", {"blight"}),
    ("Jewelled Foil", {"heist", "harvest"}),
    ("Titanium Spirit Shield", {"harvest"}),
    ("Heavy Arrow Quiver", set()),
])
def test_enchantment_catalogue_sources_are_base_specific_and_never_random(name, sources):
    with sqlite3.connect(f"file:{ROOT / 'data/sqlite/poecraft.db'}?mode=ro", uri=True) as c, load_data(ARTIFACT) as data:
        c.row_factory = sqlite3.Row
        base = resolve_base_selection(c, name, 86)
        with data.create_session(base.metadata_path, 86) as session, session.create_action_context(19) as ctx:
            enchants = [m for i in range(session.mod_count) if (m := session.mod_info(i)).reach_kind == 12]
            assert {m.reach_via for m in enchants} == {f"retained:{source}_enchantment" for source in sources}
            item = session.create_item("rare")
            pool = {r["session_mod_id"] for r in ctx.debug_pool(item, "exalt")}
            assert not pool.intersection(m.session_mod_id for m in enchants)
            assert not set(item.implicit_mod_ids).intersection(m.session_mod_id for m in enchants)
            assert all(m.generation_type == -1 for m in enchants)


def test_retained_elevated_enchantments_and_member_unveils_are_not_roll_pool():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(19) as ctx:
        mods = [session.mod_info(i) for i in range(session.mod_count)]
        assert any(m.reach_kind == 11 for m in mods)
        assert any(m.reach_kind == 12 for m in mods)
        named = [m for m in mods if m.reach_via.startswith("veiled:member:")]
        assert named
        pool = {r["session_mod_id"] for r in ctx.debug_pool(session.create_item("rare"), "exalt")}
        assert all(m.session_mod_id not in pool for m in mods if m.reach_kind in (10, 11, 12))
        # Generic native unveil offers must never offer member-specific sources.
        for _ in range(100):
            item = session.create_item("rare")
            assert ctx.apply(item, "veiled_exalt").applied
            offered = {int(m) for slot in list(item._state.prefixes[:item._state.prefix_count])+list(item._state.suffixes[:item._state.suffix_count])
                for m in slot.veiled_option_mod_ids[:slot.veiled_option_count]}
            assert not offered.intersection(m.session_mod_id for m in named)
