from pathlib import Path
import ctypes as ct
import json
import os
import pytest
from poecraft_engine import EngineError, load_data
from poecraft_engine._binding import _lib, _error, _ActionResult, _ItemState, _handle

ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = Path(os.environ.get("POECRAFT_TEST_ARTIFACT", ROOT / json.loads((ROOT / "apps/web/runtime.lock.json").read_text())["runtime_directory"]))
BASE = "Metadata/Items/Amulets/Amulet3"

def _fields(value):
    if isinstance(value, ct.Structure): return tuple((name, _fields(getattr(value, name))) for name, _ in value._fields_)
    if isinstance(value, ct.Array): return tuple(_fields(entry) for entry in value)
    return value


@pytest.mark.parametrize("currency,rarity", [("transmute", "normal"), ("alchemy", "normal"),
    ("alteration", "magic"), ("regal", "magic"), ("chaos", "rare"), ("exalt", "rare"),
    ("vaal", "rare"), ("foulborn_regal", "magic"), ("foulborn_exalt", "rare")])
def test_lock_preserves_complete_native_marginal_and_numerical_preview(currency, rarity):
    # Compare all native item fields/rolls against actual unlocked execution.
    # These 1000 seeds qualify primitive foresight; no strategy simulator runs.
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session:
        for seed in range(1000):
            item = session.create_item(rarity)
            control = item.copy()
            before = bytes(item._state)
            with session.create_action_context(seed) as ctx, session.create_action_context(seed) as ordinary:
                expected = ordinary.apply(control, currency)
                with ctx.hinekora_lock(item, currency) as lock:
                    assert bytes(item._state) == before
                    preview, observed = lock.preview()
                    assert lock.active and observed == expected
                    assert bytes(preview._state) == bytes(control._state)
                    assert bytes(lock.preview()[0]._state) == bytes(preview._state)
                    assert ctx.apply(item, currency) == expected
                    assert bytes(item._state) == bytes(control._state)
                    assert not lock.active
                    with pytest.raises(EngineError, match="consumed or invalidated"):
                        lock.commit()


def test_item_identity_decline_and_intervening_actions():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(91) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item, "exalt") as lock:
            predicted = bytes(lock.preview()[0]._state)
            with pytest.raises(EngineError, match="Modify the item"):
                ctx.hinekora_lock(item, "exalt")
            clone = item.copy()
            error, active = _error(), ct.c_int32()
            assert _lib.pc_hinekora_lock_status(lock._handle, ct.byref(clone._state), ct.byref(active), ct.byref(error)) != 0
            assert lock.active
            assert not ctx.apply(item, "transmute").applied
            assert lock.active and bytes(lock.preview()[0]._state) == predicted
            assert ctx.apply(item, "chaos").applied
            assert not lock.active
            with pytest.raises(EngineError, match="consumed or invalidated"):
                lock.preview()
            with ctx.hinekora_lock(item, "annul") as next_lock:
                assert next_lock.commit().applied
                assert not next_lock.active


def test_no_free_decline_refresh_or_aliasing():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(2) as ctx:
        item = session.create_item("rare")
        before = bytes(item._state)
        with ctx.hinekora_lock(item, "exalt") as lock:
            error, result = _error(), _ActionResult()
            assert _lib.pc_hinekora_lock_preview(lock._handle, ct.byref(item._state), ct.byref(item._state), ct.byref(result), ct.byref(error)) != 0
            assert bytes(item._state) == before and lock.active
            lock.invalidate()
        with pytest.raises(EngineError, match="Modify the item"):
            ctx.hinekora_lock(item, "exalt")


@pytest.mark.parametrize("currency,rarity", [("exalt", "normal"), ("harvest_reforge", "rare"),
    ("unravelling", "rare"), ("remembrance", "normal"), ("double_corruption", "rare")])
def test_refused_creation_is_atomic(currency, rarity):
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(21) as ctx, session.create_action_context(21) as control:
        item = session.create_item(rarity)
        before = bytes(item._state)
        with pytest.raises((EngineError, ValueError)):
            ctx.hinekora_lock(item, currency)
        assert bytes(item._state) == before
        comparison = item.copy()
        next_currency = "alchemy" if rarity == "normal" else "chaos"
        ctx.apply(item, next_currency)
        control.apply(comparison, next_currency)
        assert bytes(item._state) == bytes(comparison._state)


def test_modified_and_restored_bytes_do_not_reactivate_observed_invalidity():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(2) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item, "exalt") as lock:
            item._state.quality = 1
            assert not lock.active
            item._state.quality = 0
            assert not lock.active
            with pytest.raises(EngineError, match="consumed or invalidated"):
                lock.commit()


def test_strand_and_nonlive_laws_are_not_bypassed():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(2) as ctx:
        item = session.create_item("rare")
        item.memory_strands = 10
        with pytest.raises(EngineError, match="Memory-strand"):
            ctx.hinekora_lock(item, "exalt")
        item.memory_strands = 0
        item._state.lifecycle = 1
        with pytest.raises(EngineError, match="inapplicable"):
            ctx.hinekora_lock(item, "exalt")


def test_successful_currency_with_identical_visible_fields_consumes_foresight():
    # Structural native fixture: all rolled affixes are retained. Its purpose is
    # consumed-vs-refused event semantics, not an in-game six-fracture claim.
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(7) as ctx:
        for _ in range(100):
            item = session.create_item("normal")
            ctx.apply(item, "alchemy")
            if item.explicit_count == 6: break
        assert item.explicit_count == 6
        for slots, count in [(item._state.prefixes, item._state.prefix_count), (item._state.suffixes, item._state.suffix_count)]:
            for slot in slots[:count]: slot.flags |= 1
        before = _fields(item._state)
        with ctx.hinekora_lock(item, "chaos") as lock:
            assert _fields(lock.preview()[0]._state) == before
            assert ctx.apply(item, "chaos").applied
            assert _fields(item._state) == before
            assert not lock.active
            with pytest.raises(EngineError, match="consumed or invalidated"):
                lock.preview()
            with ctx.hinekora_lock(item, "chaos") as paid_again:
                assert paid_again.active
                assert paid_again.commit().applied
                assert not paid_again.active


def test_binding_keeps_original_session_identity():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, data.create_session(BASE, 85) as other, session.create_action_context(7) as ctx:
        item = other.create_item("rare")
        with pytest.raises(ValueError, match="different session"):
            ctx.hinekora_lock(item, "exalt")


def test_native_lock_metadata_does_not_admit_non_currency_operations():
    _lib.pc_hinekora_lock_cost_key.restype = ct.c_char_p
    _lib.pc_hinekora_lock_currency_supported.argtypes = [ct.c_int32]
    _lib.pc_hinekora_lock_currency_supported.restype = ct.c_int32
    assert _lib.pc_hinekora_lock_cost_key() == b"hinekora_lock"
    assert _lib.pc_hinekora_lock_currency_supported(6) == 1
    for code in [-1, 10, 11, 14, 15, 16, 17, 29, 30, 32, 33, 35, 36]:
        assert _lib.pc_hinekora_lock_currency_supported(code) == 0


def test_abi_padding_cannot_refresh_or_invalidate_native_foresight():
    with load_data(ARTIFACT) as data, data.create_session(BASE, 86) as session, session.create_action_context(7) as ctx:
        item = session.create_item("rare")
        with ctx.hinekora_lock(item, "exalt") as lock:
            bytes_view = (ct.c_ubyte * ct.sizeof(item._state)).from_buffer(item._state)
            # Header alignment padding precedes prefixes at offset 12.
            bytes_view[9] = 37
            assert lock.active
            with pytest.raises(EngineError, match="Modify the item"):
                ctx.hinekora_lock(item, "exalt")
            assert lock.commit().applied
