"""Approved approximate information-state contracts; no adaptive solver claim."""
import copy
import pytest
from poecraft_engine import EngineError, load_data
from test_hinekora_lock import ARTIFACT, BASE, _fields


def test_apply_without_selection_observe_switch_and_commit_complete_identity():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(17) as ctx:
        item = session.create_item("rare")
        before = _fields(item._state)
        with ctx.hinekora_lock(item) as lock:
            paid = lock.export()
            assert paid["version"] == "independent-cached-lock-v1" and not paid["selected"]
            assert lock.active and item.item_flags & 16
            with pytest.raises(EngineError, match="Observe"):
                lock.commit()
            exalt, outcome = lock.observe("exalt")
            chaos, _ = lock.observe("chaos")
            alias, _ = lock.observe({"type": "exalt", "tier": 4})  # irrelevant field normalizes away
            assert _fields(alias._state) == _fields(exalt._state)
            marked = item.copy(); marked._state.item_flags &= ~16
            assert _fields(marked._state) == before
            assert ctx.apply(item, "exalt") == outcome
            assert _fields(item._state) == _fields(exalt._state) and not lock.active
            with pytest.raises(EngineError, match="consumed or invalidated"):
                lock.observe("chaos")


def test_reload_before_observation_preserves_seen_and_unseen_requests_without_rng_state():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(17) as ctx, session.create_action_context(999) as restored:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item) as lock:
            before_observation = lock.export()
            assert "seed" not in str(before_observation) and "rng" not in str(before_observation)
            expected = {name: _fields(lock.observe(name)[0]._state) for name in ("exalt", "chaos", "vaal")}
            replacement = item.copy()
            with restored.restore_hinekora_lock(replacement, None, before_observation) as undo:
                for name in ("vaal", "chaos", "exalt"):
                    assert _fields(undo.observe(name)[0]._state) == expected[name]
                checkpoint = undo.export()
                replacement2 = replacement.copy()
                with ctx.restore_hinekora_lock(replacement2, "exalt", checkpoint) as reload:
                    assert _fields(reload.observe("chaos")[0]._state) == expected["chaos"]
                    assert ctx.apply(replacement2, "chaos").applied and not reload.active


@pytest.mark.parametrize("currency", ["transmute", "veiled_exalt", {"type": "harvest_reforge", "target_tag": "fire"}, "unravelling"])
def test_refused_observation_and_action_preserve_paid_lock_atomically(currency):
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(7) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item) as lock:
            preview = _fields(lock.observe("exalt")[0]._state)
            before = lock.export()
            with pytest.raises(EngineError, match="Unsupported or inapplicable"):
                lock.observe(currency)
            assert lock.export() == before and lock.active
            if currency == "transmute":
                assert not ctx.apply(item, currency).applied
            elif currency == "veiled_exalt":
                with pytest.raises(EngineError): ctx.apply(item, currency)
            assert lock.active and _fields(lock.preview()[0]._state) == preview
            assert ctx.apply(item, "scour").applied and not lock.active


def test_incomplete_or_inconsistent_reservations_refuse_restore_atomically():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(17) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item) as lock:
            lock.observe("exalt")
            good = lock.export()
            for edit in ("missing", "identity", "outcome"):
                bad = copy.deepcopy(good)
                if edit == "missing": bad["reservations"].pop()
                elif edit == "identity": bad["reservations"][0][0][0] = 999
                else: bad["outcome"][1] += 1
                with pytest.raises(EngineError): ctx.restore_hinekora_lock(item, "exalt", bad)
                assert lock.active and lock.export() == good


def test_unselected_checkpoint_refuses_inconsistent_selection_atomically():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(17) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item) as lock:
            good = lock.export()
            for field in ("action", "preview", "outcome", "request"):
                bad = copy.deepcopy(good)
                if field == "action": bad[field][0] = 999
                elif field != "request": bad[field] = []
                with pytest.raises(EngineError, match="Unselected Lock checkpoint"):
                    ctx.restore_hinekora_lock(item, "exalt" if field == "request" else None, bad)
                assert lock.active and lock.export() == good


def test_decline_and_equal_visible_consumption_keep_no_refresh_distinction():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(7) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item) as declined:
            declined.observe("exalt"); declined.invalidate()
            with pytest.raises(EngineError, match="Modify the item"): ctx.hinekora_lock(item)
            assert not declined.export()["refresh_allowed"]
        # Structural six-fracture fixture, not a claim of game reachability.
        item = session.create_item("normal")
        for _ in range(100):
            ctx.apply(item, "alchemy")
            if item.explicit_count == 6: break
            ctx.apply(item, "scour")
        assert item.explicit_count == 6
        for slots, count in ((item._state.prefixes, item._state.prefix_count), (item._state.suffixes, item._state.suffix_count)):
            for slot in slots[:count]: slot.flags |= 1
        before = _fields(item._state)
        with ctx.hinekora_lock(item) as lock:
            assert _fields(lock.observe("chaos")[0]._state) == before
            assert ctx.apply(item, "chaos").applied
            assert _fields(item._state) == before and not lock.active
            with ctx.hinekora_lock(item) as paid_again:
                assert paid_again.active


@pytest.mark.parametrize("requests", [
    [{"type": "essence", "essence": "Metadata/Items/Currency/CurrencyEssenceAnguish2"},
     {"type": "essence", "essence": "Metadata/Items/Currency/CurrencyEssenceAnguish3"}],
    [{"type": "influence_exalt", "influence": "crusader"},
     {"type": "influence_exalt", "influence": "hunter"}],
    [{"type": "eldritch_ember", "tier": 1}, {"type": "eldritch_ember", "tier": 4}],
])
def test_complete_configured_request_identity_and_replay(requests):
    base = "Metadata/Items/Armours/BodyArmours/BodyInt17"
    with load_data(ARTIFACT) as data, data.create_session(base, 86) as session, session.create_action_context(23) as ctx, session.create_action_context(917) as restored:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item) as lock:
            application = lock.export()
            expected = []
            selected = []
            for request in requests:
                expected.append(_fields(lock.observe(request)[0]._state))
                selected.append(lock.export()["action"])
            assert selected[0] != selected[1]
            assert _fields(lock.observe(requests[0])[0]._state) == expected[0]
            replacement = item.copy()
            with restored.restore_hinekora_lock(replacement, None, application) as replay:
                for index in (1, 0):
                    assert _fields(replay.observe(requests[index])[0]._state) == expected[index]
                assert replay.commit().applied
                assert _fields(replacement._state) == expected[0]


def test_refused_application_and_checkpoint_operations_do_not_draw_rng():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(41) as ctx, session.create_action_context(41) as control:
        unavailable = session.create_item("rare")
        unavailable._state.item_flags |= 1  # corrupted: every supported craft refuses
        before = _fields(unavailable._state)
        with pytest.raises(EngineError, match="No supported currency"):
            ctx.hinekora_lock(unavailable)
        assert _fields(unavailable._state) == before
        ordinary = session.create_item("normal"); comparison = ordinary.copy()
        assert ctx.apply(ordinary, "alchemy") == control.apply(comparison, "alchemy")
        assert _fields(ordinary._state) == _fields(comparison._state)
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(9) as source, session.create_action_context(41) as ctx, session.create_action_context(41) as control:
        item = session.create_item("rare")
        with source.hinekora_lock(item) as paid:
            checkpoint = paid.export()
            replacement = item.copy()
            with ctx.restore_hinekora_lock(replacement, None, checkpoint) as restored:
                restored.observe("exalt"); restored.observe("chaos"); restored.observe("exalt")
                bad = copy.deepcopy(restored.export()); bad["reservations"].pop()
                with pytest.raises(EngineError):
                    ctx.restore_hinekora_lock(replacement, "exalt", bad)
                assert restored.active
                ordinary = session.create_item("normal"); comparison = ordinary.copy()
                assert ctx.apply(ordinary, "alchemy") == control.apply(comparison, "alchemy")
                assert _fields(ordinary._state) == _fields(comparison._state)
