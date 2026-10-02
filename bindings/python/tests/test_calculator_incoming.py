"""Incoming-carrier regression checks through the public Calculator binding."""
import json
import math
import os
from pathlib import Path

import pytest

from poecraft_engine import EngineError, load_data

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = Path(os.environ.get("POECRAFT_TEST_ARTIFACT", ROOT / json.loads(
    (ROOT / "apps/web/runtime.lock.json").read_text())["runtime_directory"]))
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"
TARGET = "LocalIncreasedEnergyShield11"
FIXTURES = [
    ["LocalIncreasedEnergyShieldPercent8", "FireResist8", "ColdResist8", "LightningResist8"],
    ["LocalIncreasedEnergyShieldPercent8", "LocalIncreasedEnergyShieldPercentAndStunRecovery6", "FireResist8", "ColdResist8"],
    ["LocalIncreasedEnergyShieldPercent8", "LocalIncreasedEnergyShieldPercentAndStunRecovery6", "LocalBaseEnergyShieldAndLife4_", "FireResist8"],
    ["LocalIncreasedEnergyShieldPercent8", "LocalIncreasedEnergyShieldPercentAndStunRecovery6", "LocalBaseEnergyShieldAndLife4_", "FireResist8", "ColdResist8", "LightningResist8"],
]


def target_goal(rarity="rare"):
    # A cleanup-only caller must not dictate the requested action's layout.
    return {"version": "v1", "rarity": rarity, "allow_extra_modifiers": True,
            "slots": [{"family_mod_key": TARGET, "min_tier": 0}], "actions": ["scour"]}


def assert_calculation(session, ctx, item, action, rarity):
    before = bytes(item._state)
    pool_item = item.copy()
    if action.endswith("regal"):
        pool_item._state.rarity = 2
    cap = 1 if pool_item._state.rarity == 1 else 3
    prefix_open = pool_item._state.prefix_count < cap
    suffix_open = pool_item._state.suffix_count < cap
    side = None if prefix_open and suffix_open else "prefix" if prefix_open else "suffix"
    rows = ctx.debug_pool(pool_item, action, side=side) if prefix_open or suffix_open else []
    target_family = session.find_mod(TARGET).family_id
    total = sum(row["final_weight"] for row in rows)
    weight = sum(row["final_weight"] for row in rows
                 if session.mod_info(row["session_mod_id"]).family_id == target_family)
    expected = weight / total if total else 0
    result = session.calculate_currency(item, action, target_goal(rarity))
    assert result["supported"]
    assert result["success_probability"] == pytest.approx(expected, abs=1e-12)
    assert result["slot_satisfied"][0] == pytest.approx(expected, abs=1e-12)
    assert math.isclose(sum(row["probability"] for row in result["outcomes"]), 1, abs_tol=1e-12)
    assert bytes(item._state) == before
    # Inspector construction takes a separate C-API path with no explicit goal.
    inspection = session.calculate_currency(item, action)
    assert inspection["legal"] == result["legal"]
    assert math.isclose(sum(row["probability"] for row in inspection["outcomes"]), 1, abs_tol=1e-12)
    assert bytes(item._state) == before
    return result


@pytest.mark.parametrize("keys", FIXTURES)
def test_foulborn_retained_carriers_match_native_execution_pool(keys):
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(0) as ctx:
        item = session.create_item("rare", with_implicits=False)
        for key in keys:
            item.add_mod(key)
        result = assert_calculation(session, ctx, item, "foulborn_exalt", "rare")
        assert result["legal"] == (len(keys) < 6)
        if keys == FIXTURES[0]:
            assert result["success_probability"] == pytest.approx(0.21153846153846154, abs=1e-12)
        if keys == FIXTURES[1]:
            assert result["success_probability"] == pytest.approx(0.12471655328798185, abs=1e-12)


@pytest.mark.parametrize("action,rarity", [("augment", "magic"), ("foulborn_augment", "magic"),
    ("regal", "rare"), ("foulborn_regal", "rare"), ("exalt", "rare"), ("foulborn_exalt", "rare")])
def test_ordinary_and_foulborn_adds_ignore_the_callers_cleanup_layout(action, rarity):
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(0) as ctx:
        item = session.create_item("rare" if action.endswith("exalt") else "magic", with_implicits=False)
        item.add_mod("FireResist8", fractured=True)
        result = assert_calculation(session, ctx, item, action, rarity)
        assert result["legal"] and result["success_probability"] > 0


def test_disabled_foulborn_family_still_refuses_calculation():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        item = session.create_item("rare", with_implicits=False)
        before = bytes(item._state)
        with pytest.raises(EngineError, match="disabled family"):
            session.calculate_currency(item, "foulborn_exalt", {
                **target_goal(), "disabled_action_families": ["foulborn"]})
        assert bytes(item._state) == before
