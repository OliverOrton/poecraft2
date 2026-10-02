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
        assert conf["jewel_socket_count"] == {"Small": 0, "Medium": 1, "Large": 2}[size]
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
                ctx.apply(item, "vaal")
            with pytest.raises(EngineError, match="not yet approved"):
                session.calculate_currency(item, "vaal")


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


@pytest.mark.parametrize("size", KEYS)
@pytest.mark.parametrize("action,rarity,ordinary", [("foulborn_augment", "magic", "augment"), ("foulborn_regal", "magic", "regal"), ("foulborn_exalt", "rare", "exalt")])
def test_foulborn_uses_current_native_culling_pool(size, action, rarity, ordinary):
    with load_data(ARTIFACT) as data, data.create_cluster_session(BASE + size, 84, passive_key=KEYS[size], passive_count=COUNTS[size]) as session, session.create_action_context(23) as ctx:
        item = session.create_item(rarity)
        # Occupied native identities alter available tiers/groups before culling.
        mod = next(session.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(item, ordinary) if session.mod_info(r["session_mod_id"]).side == "suffix")
        item.add_mod(mod, fractured=True)
        prepared = item.copy()
        if ordinary == "regal": prepared._state.rarity = 2
        pool = list(ctx.debug_pool(prepared, action, side="prefix" if rarity == "magic" and ordinary != "regal" else None))
        target = session.mod_info(pool[0]["session_mod_id"])
        goal = {"rarity": "rare" if ordinary != "augment" else "magic", "allow_extra_modifiers": True,
                "slots": [{"family_mod_key": target.key, "min_tier": max(session.mod_info(r["session_mod_id"]).family_tier_index for r in pool if session.mod_info(r["session_mod_id"]).family_id == target.family_id)}], "actions": ["scour"], "automatic_candidates": False}
        total = sum(r["final_weight"] for r in pool)
        expected = sum(r["final_weight"] for r in pool if session.mod_info(r["session_mod_id"]).family_id == target.family_id) / total
        before = bytes(item._state)
        result = session.calculate_currency(item, action, goal)
        assert result["supported"] and result["legal"]
        assert result["success_probability"] == pytest.approx(expected, abs=1e-12)
        assert bytes(item._state) == before
        assert ctx.apply(item, action).applied
        assert item.fractured_mod_ids == (mod.session_mod_id,)
        with pytest.raises(EngineError, match="disabled family"):
            session.calculate_currency(prepared, action, {**goal, "disabled_action_families": ["foulborn"]})


@pytest.mark.parametrize("size", KEYS)
def test_cluster_rare_target_is_not_clamped_ordinary_count(size):
    with load_data(ARTIFACT) as data, data.create_cluster_session(BASE + size, 75, passive_key=KEYS[size], passive_count=COUNTS[size]) as session, session.create_action_context(37) as ctx:
        normal = session.create_item("normal")
        identity = session.cluster_configuration
        result = session.calculate_currency(normal, "alchemy", {"rarity": "rare", "slots": [], "allow_extra_modifiers": True})
        counts = {3: 0.0, 4: 0.0}
        for row in result["outcomes"]: counts[row["prefixes"] + row["suffixes"]] += row["probability"]
        assert counts == pytest.approx({3: .65, 4: .35}, abs=1e-12)
        observed = set()
        for _ in range(64):
            item = normal.copy()
            assert ctx.apply(item, "alchemy").applied
            observed.add(item.explicit_count)
            assert item._state.prefix_count <= 2 and item._state.suffix_count <= 2
            assert session.cluster_configuration == identity
        assert observed == {3, 4}


@pytest.mark.parametrize("size", KEYS)
@pytest.mark.parametrize("action", ["chaos", "fossil", "harvest_reforge"])
def test_standard_rare_reforges_count_retained_affixes(size, action):
    fossil="Metadata/Items/Currency/CurrencyDelveCraftingPhysical"
    spec = {"type":"fossil", "fossils":[fossil]} if action=="fossil" else {"type":"harvest_reforge","target_tag":"life"} if action=="harvest_reforge" else "chaos"
    action_id = "fossil:"+fossil if action=="fossil" else "harvest_reforge:life" if action=="harvest_reforge" else action
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE+size,1,passive_key=KEYS[size],passive_count=COUNTS[size]) as session,session.create_action_context(29) as ctx:
        original=session.create_item("rare")
        retained=next(session.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(original,"exalt") if session.mod_info(r["session_mod_id"]).side=="suffix")
        original.add_mod(retained,fractured=True)
        before=bytes(original._state)
        result=session.calculate_currency(original,action_id,{"rarity":"rare","slots":[],"allow_extra_modifiers":True})
        assert result["legal"] and result["supported"]
        counts={3:0.,4:0.}
        for row in result["outcomes"]: counts[row["prefixes"]+row["suffixes"]]+=row["probability"]
        assert counts==pytest.approx({3:.65,4:.35},abs=1e-12)
        assert bytes(original._state)==before
        for _ in range(16):
            child=original.copy();assert ctx.apply(child,spec).applied
            assert child.fractured_mod_ids==(retained.session_mod_id,)
            assert child.explicit_count in (3,4)
            assert child._state.prefix_count<=2 and child._state.suffix_count<=2
            assert session.cluster_configuration["passive_key"]==KEYS[size]


@pytest.mark.parametrize("fractured",[False,True])
def test_harvest_augment_uses_targeted_pool_before_native_removal(fractured):
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE+"Small",75,passive_key=KEYS["Small"],passive_count=2) as s,s.create_action_context(17) as ctx:
        original=s.create_item("rare")
        retained=next(s.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(original,"exalt") if s.mod_info(r["session_mod_id"]).side=="suffix" and "life" not in s.mod_info(r["session_mod_id"]).classification_tags)
        original.add_mod(retained,fractured=fractured)
        spec={"type":"harvest_augment","target_tag":"life"}
        pool=list(ctx.debug_pool(original,spec));target=s.find_mod("AfflictionNotableFettle")
        expected=sum(r["final_weight"] for r in pool if s.mod_info(r["session_mod_id"]).family_id==target.family_id)/sum(r["final_weight"] for r in pool)
        goal={"rarity":"rare","slots":[{"family_mod_key":target.key,"min_tier":1}],"allow_extra_modifiers":True}
        result=s.calculate_currency(original,"harvest_augment:life",goal)
        assert result["legal"] and result["success_probability"]==pytest.approx(expected,abs=1e-12)
        for _ in range(16):
            child=original.copy();assert ctx.apply(child,spec).applied
            assert child.explicit_count==(2 if fractured else 1)
            assert (retained.session_mod_id in child.suffix_mod_ids)==fractured


def test_harvest_resistance_conversion_preserves_native_level_and_fracture():
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE+"Small",75,passive_key=KEYS["Small"],passive_count=2) as s,s.create_action_context(17) as ctx:
        original=s.create_item("rare")
        source=next(s.mod_info(r["session_mod_id"]) for r in ctx.debug_pool(original,"exalt") if {"resistance","fire"}<=set(s.mod_info(r["session_mod_id"]).classification_tags) and "cold" not in s.mod_info(r["session_mod_id"]).classification_tags)
        original.add_mod(source)
        empty=s.create_item("rare")
        pool=[r for r in ctx.debug_pool(empty,{"type":"harvest_reforge","target_tag":"cold"},side=source.side) if s.mod_info(r["session_mod_id"]).required_level==source.required_level and "resistance" in s.mod_info(r["session_mod_id"]).classification_tags and "fire" not in s.mod_info(r["session_mod_id"]).classification_tags]
        target=s.mod_info(pool[0]["session_mod_id"])
        goal={"rarity":"rare","slots":[{"family_mod_key":target.key,"min_tier":target.family_tier_index}],"allow_extra_modifiers":True}
        expected=sum(r["final_weight"] for r in pool if s.mod_info(r["session_mod_id"]).family_id==target.family_id)/sum(r["final_weight"] for r in pool)
        result=s.calculate_currency(original,"harvest_resist:fire:cold",goal)
        assert result["legal"] and result["success_probability"]==pytest.approx(expected,abs=1e-12)
        assert ctx.apply(original,{"type":"harvest_resist","source_tag":"fire","target_tag":"cold"}).applied
        changed=s.mod_info((original.prefix_mod_ids+original.suffix_mod_ids)[0])
        assert changed.required_level==source.required_level and "cold" in changed.classification_tags and "fire" not in changed.classification_tags
        protected=s.create_item("rare");protected.add_mod(source,fractured=True)
        assert not s.calculate_currency(protected,"harvest_resist:fire:cold",goal)["legal"]
        assert not ctx.apply(protected,{"type":"harvest_resist","source_tag":"fire","target_tag":"cold"}).applied


@pytest.mark.parametrize("key",["Metadata/Items/Currency/CurrencyDelveCraftingMirror","Metadata/Items/Currency/CurrencyDelveCraftingSellPrice"])
def test_fossil_specials_are_native_terminal_outcomes(key):
    # The frozen record, not a legacy fossil name, owns its active flag.
    strings=json.loads((ARTIFACT/"strings.json").read_text(encoding="utf8"))
    fossils=json.loads((ARTIFACT/"game-data.json").read_text(encoding="utf8"))["fossils"]
    names=[strings["strings"][sid-strings.get("string_id_base",0)] for sid in fossils["key_string_ids"]]
    mirrors=bool(fossils["mirrors"][names.index(key)])
    with load_data(ARTIFACT) as d,d.create_cluster_session(BASE+"Small",1,passive_key=KEYS["Small"],passive_count=2) as s,s.create_action_context(17) as ctx:
        original=s.create_item("normal")
        result=s.calculate_currency(original,"fossil:"+key,{"rarity":"rare","slots":[],"allow_extra_modifiers":True})
        assert result["legal"] and sum(r["probability"] for r in result["outcomes"])==pytest.approx(1,abs=1e-12)
        child=original.copy();assert ctx.apply(child,{"type":"fossil","fossils":[key]}).applied
        assert bool(child._state.item_flags&2)==mirrors
        assert child.explicit_count in (3,4) and original.explicit_count==0
