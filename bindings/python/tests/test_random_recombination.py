"""Finite native random-pair transport and transaction contracts."""
import ctypes as ct
import math
import os
from pathlib import Path

import pytest
from poecraft_engine import EngineError, RandomRecombination, load_data

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = Path(os.environ.get("POECRAFT_TEST_ARTIFACT", ROOT / "data/compiled/current"))
MODEL = "poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1"


def image(item):
    return ct.string_at(ct.byref(item._state), ct.sizeof(item._state))


@pytest.fixture
def inputs():
    with load_data(ARTIFACT) as data, data.create_session("Metadata/Items/Rings/Ring1", 60) as low, data.create_session("Metadata/Items/Rings/Ring2", 86) as high:
        left, right = low.create_item("rare"), high.create_item("rare")
        mod = next(high.mod_info(i) for i in range(high.mod_count)
                   if high.mod_info(i).side == "prefix" and high.mod_info(i).required_level > 75
                   and high.mod_info(i).reach_kind == 0)
        right.add_mod(mod)
        right._state.prefixes[0].roll_count = 2
        right._state.prefixes[0].rolls[0], right._state.prefixes[0].rolls[1] = -7, 91
        left._state.quality, right._state.quality = 21, 31
        left.memory_strands, right.memory_strands = 23, 11
        yield low, high, left, right, mod


def test_versioned_mixed_base_outcomes_retain_high_tier_identity(inputs):
    low, high, left, right, mod = inputs
    before = image(left), image(right)
    with RandomRecombination(left, right, left_identity="a", right_identity="b") as pair:
        result = pair.calculate()
        assert result["model_id"] == MODEL and result["pair_version"] == 1
        assert result["game_odds_estimated"] and result["apply_supported"]
        assert result["gold_cost"] is None and result["dust_cost"] is None and not result["cost_complete"]
        assert sum(o["probability"] for o in result["outcomes"]) == pytest.approx(1, abs=1e-12)
        for carrier in (0, 1):
            assert sum(o["probability"] for o in result["outcomes"] if o["carrier"] == carrier) == pytest.approx(.5)
            with pair.output_session(carrier) as session:
                transferred = session.find_mod(mod.key)
                assert transferred.required_level > 75 and transferred.reach_kind == 13
                for outcome in result["outcomes"]:
                    if outcome["carrier"] != carrier:
                        continue
                    item = outcome["item"]
                    assert item["memory_strands"] == (23 if carrier == 0 else 11)
                    for slot in item["prefixes"]:
                        assert slot["global_mod_id"] == mod.global_mod_id
                        assert slot["rolls"] == [-7, 91]
        assert (image(left), image(right)) == before


def test_apply_stale_and_collision_refusals_are_atomic(inputs):
    low, high, left, right, _ = inputs
    with low.create_action_context(seed=51) as context, RandomRecombination(left, right, left_identity="a", right_identity="b") as pair:
        right._state.quality += 1
        before = image(left), image(right)
        with pytest.raises(EngineError, match="stale"):
            pair.apply(context, output_identity="new")
        assert (image(left), image(right)) == before
        right._state.quality -= 1
        third = left.copy()
        before = image(left), image(right), image(third)
        with pytest.raises(EngineError, match="already exists"):
            pair.apply(context, output_identity="new", resources=[("a", "left", left), ("b", "right", right), ("new", "spare", third)])
        assert (image(left), image(right), image(third)) == before


def test_apply_receipt_sessions_survive_pair_close_and_replay_refuses(inputs):
    low, high, left, right, _ = inputs
    with low.create_action_context(seed=51) as context, RandomRecombination(left, right, left_identity="a", right_identity="b") as pair:
        result = pair.apply(context, output_identity="new")
        assert left.lifecycle == right.lifecycle == 1
        assert [change["effect"] for change in result["resources"]] == [3, 3, 2]
        assert result["resources"][0]["before"].lifecycle == result["resources"][1]["before"].lifecycle == 0
        assert result["gold_cost"] is None and result["dust_cost"] is None and not result["cost_complete"]
        before = image(left), image(right)
        with pytest.raises(EngineError, match="stale"):
            pair.apply(context, output_identity="replay")
        assert (image(left), image(right)) == before
    with result["session"]:
        assert result["item"].rarity == "rare" and result["item"].lifecycle == 0
        assert result["item"].memory_strands in (23, 11)


def test_input_alias_and_unsupported_fracture_refuse(inputs):
    low, high, left, right, _ = inputs
    with pytest.raises(EngineError, match="aliased"):
        RandomRecombination(left, left, left_identity="a", right_identity="b")
    right._state.prefixes[0].flags = 1
    before = image(left), image(right)
    with pytest.raises(EngineError, match="classification"):
        RandomRecombination(left, right, left_identity="a", right_identity="b")
    assert (image(left), image(right)) == before
