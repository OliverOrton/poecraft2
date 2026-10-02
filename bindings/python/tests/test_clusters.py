from pathlib import Path
import json
import math

import pytest
from poecraft_engine import load_data, EngineError

ARTIFACT = Path(__file__).resolve().parents[3] / "data/compiled/current"
BASE = "Metadata/Items/Jewels/JewelPassiveTreeExpansion"
KEYS = {"Small": "affliction_maximum_life", "Medium": "affliction_fire_damage_over_time_multiplier", "Large": "affliction_axe_and_sword_damage"}
COUNTS = {"Small": 2, "Medium": 4, "Large": 8}


def rows(ctx, item):
    prefix_open, suffix_open = item._state.prefix_count < (1 if item.rarity == "magic" else 2), item._state.suffix_count < (1 if item.rarity == "magic" else 2)
    if not prefix_open and not suffix_open:
        return []
    side = None if prefix_open and suffix_open else "prefix" if prefix_open else "suffix"
    return list(ctx.debug_pool(item, "augment" if item.rarity == "magic" else "exalt", side=side))


@pytest.mark.parametrize("size", KEYS)
@pytest.mark.parametrize("level", [1, 50, 68, 73, 75, 78, 84])
def test_configured_native_pool_domain_level_and_cache_parity(size, level):
    with load_data(ARTIFACT) as data, data.create_cluster_session(BASE + size, level, passive_key=KEYS[size], passive_count=COUNTS[size]) as session:
        conf = session.cluster_configuration
        assert (conf["passive_key"], conf["passive_count"], conf["item_level"]) == (KEYS[size], COUNTS[size], level)
        item = session.create_item("rare")
        with session.create_action_context(1) as warm, session.create_action_context(1) as fresh:
            initial = list(warm.debug_pool(item, "exalt"))
            assert initial and initial == list(fresh.debug_pool(item, "exalt"))
            chosen = session.mod_info(initial[0]["session_mod_id"])
            item.add_mod(chosen)
            changed = list(warm.debug_pool(item, "exalt"))
            assert changed == list(fresh.debug_pool(item, "exalt"))
            assert all(session.mod_info(row["session_mod_id"]).required_level <= level for row in changed)
            warm.apply(item, "scour")
            assert session.cluster_configuration == conf
            assert list(warm.debug_pool(item, "exalt")) == initial


def test_shared_tag_preserves_configuration_keys_and_stats():
    with load_data(ARTIFACT) as data:
        values = []
        for kind in ["attack", "spell"]:
            with data.create_cluster_session(BASE + "Small", 84, passive_key="affliction_chance_to_block_" + kind + "_damage", passive_count=2) as session:
                values.append(session.cluster_configuration)
        assert values[0]["passive_tag"] == values[1]["passive_tag"]
        assert values[0]["passive_key"] != values[1]["passive_key"]
        assert values[0]["passive_stats"] != values[1]["passive_stats"]


def test_magic_calculator_matches_independent_two_draw_native_pool():
    with load_data(ARTIFACT) as data, data.create_cluster_session(BASE + "Small", 84, passive_key=KEYS["Small"], passive_count=2) as session, session.create_action_context(0) as ctx:
        magic = session.create_item("magic")
        first = rows(ctx, magic)
        target = session.mod_info(first[0]["session_mod_id"])
        # Select a stable notable if available (the native domain remains authoritative).
        for row in first:
            mod = session.mod_info(row["session_mod_id"])
            if "AfflictionNotable" in mod.key:
                target = mod
                break
        total = sum(row["final_weight"] for row in first)
        one = two = 0.0
        for row in first:
            mod = session.mod_info(row["session_mod_id"])
            p = row["final_weight"] / total
            if mod.family_id == target.family_id:
                one += p
                two += p
            else:
                next_item = magic.copy()
                next_item.add_mod(mod)
                second = rows(ctx, next_item)
                denom = sum(r["final_weight"] for r in second)
                numerator = sum(r["final_weight"] for r in second if session.mod_info(r["session_mod_id"]).family_id == target.family_id)
                two += p * (numerator / denom if denom else 0)
        normal = session.create_item("normal")
        goal = {"rarity": "magic", "slots": [{"family_mod_key": target.key, "min_tier": 1}], "allow_extra_modifiers": True, "automatic_candidates": False, "actions": ["transmute"]}
        # For a multi-tier family the independent event above includes all tiers.
        goal["slots"][0]["min_tier"] = max(session.mod_info(r["session_mod_id"]).family_tier_index for r in first if session.mod_info(r["session_mod_id"]).family_id == target.family_id)
        result = session.calculate_currency(normal, "transmute", goal)
        assert result["supported"] and result["legal"]
        assert result["success_probability"] == pytest.approx((one + two) / 2, abs=1e-12)
        assert math.isclose(sum(row["probability"] for row in result["outcomes"]), 1, abs_tol=1e-12)
        assert normal.explicit_count == 0 and normal.rarity == "normal"


@pytest.mark.parametrize("count", [True, 2.0, "2", 0, 4])
def test_invalid_fixed_counts_are_refused(count):
    with load_data(ARTIFACT) as data:
        with pytest.raises((ValueError, EngineError)):
            data.create_cluster_session(BASE + "Small", 84, passive_key=KEYS["Small"], passive_count=count)


def test_unconfigured_unknown_and_unapproved_actions_are_explicit():
    with load_data(ARTIFACT) as data:
        with pytest.raises(EngineError):
            data.create_session(BASE + "Small", 84)
        with pytest.raises(EngineError):
            data.create_cluster_session(BASE + "Small", 84, passive_key="affliction_chance_to_block", passive_count=2)
        with data.create_cluster_session(BASE + "Small", 84, passive_key=KEYS["Small"], passive_count=2) as session, session.create_action_context(0) as ctx:
            item = session.create_item("normal")
            with pytest.raises(EngineError, match="not yet approved"):
                ctx.apply(item, "alchemy")
            with pytest.raises(EngineError, match="not yet approved"):
                session.calculate_currency(item, "alchemy")


@pytest.mark.parametrize("size", KEYS)
def test_basic_craft_path_and_calculator_rows_use_configured_caps(size):
    with load_data(ARTIFACT) as data, data.create_cluster_session(BASE + size, 84, passive_key=KEYS[size], passive_count=COUNTS[size]) as session, session.create_action_context(17) as ctx:
        item = session.create_item("normal")
        identity = session.cluster_configuration
        for action in ["transmute", "augment", "regal", "exalt", "exalt", "annul", "exalt", "scour"]:
            before = item.copy()
            result = session.calculate_currency(item, action, {"rarity": "rare", "slots": [], "allow_extra_modifiers": True})
            assert result["supported"]
            assert sum(row["probability"] for row in result["outcomes"]) == pytest.approx(1, abs=1e-12)
            applied = ctx.apply(item, action).applied
            assert applied == result["legal"]
            assert item._state.prefix_count <= (1 if item.rarity == "magic" else 2)
            assert item._state.suffix_count <= (1 if item.rarity == "magic" else 2)
            assert session.cluster_configuration == identity
            assert any(row["prefixes"] == item._state.prefix_count and row["suffixes"] == item._state.suffix_count for row in result["outcomes"])
            assert before._state.prefix_count <= 2 and before._state.suffix_count <= 2
        assert item.rarity == "normal" and item.explicit_count == 0


def test_small_second_notable_and_full_side_are_refused_by_native_editor():
    with load_data(ARTIFACT) as data, data.create_cluster_session(BASE + "Small", 84, passive_key=KEYS["Small"], passive_count=2) as session, session.create_action_context(0) as ctx:
        item = session.create_item("rare")
        pool = list(ctx.debug_pool(item, "exalt"))
        notables = [session.mod_info(r["session_mod_id"]) for r in pool if "AfflictionNotable" in session.mod_info(r["session_mod_id"]).key]
        assert len(notables) >= 2
        item.add_mod(notables[0])
        with pytest.raises(EngineError, match="not eligible"):
            item.add_mod(notables[1])
        other_prefix = next(session.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(item, "exalt", side="prefix"))
        item.add_mod(other_prefix)
        assert item._state.prefix_count == 2
        with pytest.raises(EngineError):
            item.add_mod(notables[1])
