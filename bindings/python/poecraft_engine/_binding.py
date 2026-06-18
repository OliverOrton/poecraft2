from __future__ import annotations

import ctypes as ct
import ctypes.util
import os
from collections.abc import Iterable, Iterator, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ABI_VERSION = 1
RESULT_OK = 0
RESULT_BUFFER_TOO_SMALL = 7
MAX_FOSSILS = 4
MOD_NONE = 0xFFFFFFFF
MOD_SLOT_FRACTURED = 1

_ACTION_TYPES = {
    "transmute": 0,
    "augment": 1,
    "alteration": 2,
    "regal": 3,
    "alchemy": 4,
    "chaos": 5,
    "exalt": 6,
    "annul": 7,
    "scour": 8,
    "essence": 9,
    "fossil": 10,
}
_RARITIES = {"normal": 0, "magic": 1, "rare": 2}


class _ErrorInfo(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("code", ct.c_int32),
        ("message", ct.c_char * 256),
    ]


class _SessionOptions(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("base_metadata_path", ct.c_char_p),
        ("item_level", ct.c_uint32),
    ]


class _ContextOptions(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("seed", ct.c_uint64),
    ]


class _ItemInitOptions(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("rarity", ct.c_uint8),
        ("with_implicits", ct.c_int32),
    ]


class _ModSlot(ct.Structure):
    _fields_ = [
        ("mod_id", ct.c_uint32),
        ("group_id", ct.c_uint16),
        ("flags", ct.c_uint8),
        ("roll_count", ct.c_uint8),
        ("rolls", ct.c_int32 * 8),
        ("veiled_option_count", ct.c_uint8),
        ("veiled_option_mod_ids", ct.c_uint32 * 3),
        ("veiled_chosen_mod_id", ct.c_uint32),
    ]


class _ItemState(ct.Structure):
    _fields_ = [
        ("rarity", ct.c_uint8),
        ("quality", ct.c_uint8),
        ("item_flags", ct.c_uint8),
        ("prefix_count", ct.c_uint8),
        ("suffix_count", ct.c_uint8),
        ("implicit_count", ct.c_uint8),
        ("enchantment_count", ct.c_uint8),
        ("prefixes", _ModSlot * 3),
        ("suffixes", _ModSlot * 3),
        ("implicits", _ModSlot * 8),
        ("enchantments", _ModSlot * 4),
        ("generic_influence_bits", ct.c_uint8),
        ("searing_exarch_tier", ct.c_uint8),
        ("eater_of_worlds_tier", ct.c_uint8),
        ("socket_count", ct.c_uint8),
        ("socket_colors", ct.c_uint8 * 6),
        ("link_mask", ct.c_uint8),
    ]


class _ActionRequest(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("action_type", ct.c_int32),
        ("essence_key", ct.c_char_p),
        ("fossil_count", ct.c_uint32),
        ("fossil_keys", ct.c_char_p * MAX_FOSSILS),
    ]


class _ActionResult(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("applied", ct.c_int32),
        ("added", ct.c_int32),
        ("removed", ct.c_int32),
    ]


class _BatchSummary(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("item_count", ct.c_uint32),
        ("applied_count", ct.c_uint32),
        ("total_added", ct.c_uint64),
        ("total_removed", ct.c_uint64),
    ]


class _PoolQueryRequest(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("action", _ActionRequest),
        ("side_filter", ct.c_int32),
        ("include_rejected", ct.c_int32),
    ]


class _PoolDebugEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("session_mod_id", ct.c_uint32),
        ("global_mod_id", ct.c_uint32),
        ("key", ct.c_char_p),
        ("generation_type", ct.c_int32),
        ("reach_kind", ct.c_int32),
        ("reach_via", ct.c_char_p),
        ("tag_signature_id", ct.c_uint32),
        ("normal_random_member", ct.c_int32),
        ("side_allowed", ct.c_int32),
        ("mechanic_allowed", ct.c_int32),
        ("influence_allowed", ct.c_int32),
        ("group_allowed", ct.c_int32),
        ("positively_weighted", ct.c_int32),
        ("first_failure", ct.c_int32),
        ("blocking_group_id", ct.c_uint32),
        ("active_spawn_row", ct.c_int32),
        ("active_spawn_tag", ct.c_char_p),
        ("active_spawn_weight", ct.c_uint32),
        ("active_generation_row", ct.c_int32),
        ("active_generation_tag", ct.c_char_p),
        ("active_generation_pct", ct.c_uint32),
        ("generation_applied", ct.c_int32),
        ("special_multiplier_pct", ct.c_uint32),
        ("final_weight", ct.c_uint32),
    ]


class _PoolDebugSummary(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("tag_signature_id", ct.c_uint32),
        ("cache_hit", ct.c_int32),
        ("candidate_count", ct.c_uint32),
        ("prefix_total_weight", ct.c_uint64),
        ("suffix_total_weight", ct.c_uint64),
        ("combined_total_weight", ct.c_uint64),
        ("cache_hits", ct.c_uint64),
        ("cache_misses", ct.c_uint64),
    ]


class _DataSummary(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("artifact_schema_version", ct.c_uint32),
        ("complete_dataset", ct.c_int32),
        ("string_count", ct.c_uint32),
        ("tag_count", ct.c_uint32),
        ("item_class_count", ct.c_uint32),
        ("base_item_count", ct.c_uint32),
        ("ordinary_session_base_count", ct.c_uint32),
        ("cluster_unsupported_base_count", ct.c_uint32),
        ("unsupported_domain_base_count", ct.c_uint32),
        ("mod_count", ct.c_uint32),
        ("group_count", ct.c_uint32),
    ]


class _ModInfo(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("session_mod_id", ct.c_uint32),
        ("global_mod_id", ct.c_uint32),
        ("key", ct.c_char_p),
        ("generation_type", ct.c_int32),
        ("reach_kind", ct.c_int32),
        ("reach_influence", ct.c_int32),
        ("reach_via", ct.c_char_p),
        ("primary_group_id", ct.c_uint32),
        ("required_level", ct.c_uint32),
    ]


class EngineError(RuntimeError):
    def __init__(self, code: int, message: str):
        self.code = code
        self.message = message
        super().__init__(f"poecraft engine error {code}: {message}")


def _decode(value: bytes | None) -> str:
    return value.decode("utf-8") if value else ""


def _library_candidates() -> Iterator[Path | str]:
    configured = os.environ.get("POECRAFT_ENGINE_LIBRARY")
    if configured:
        yield Path(configured)
    package_dir = Path(__file__).resolve().parent
    names = (
        "poecraft_engine.dll",
        "libpoecraft_engine.so",
        "libpoecraft_engine.dylib",
    )
    directories: list[Path] = []
    if (
        package_dir.parent.name == "python"
        and package_dir.parent.parent.name == "bindings"
    ):
        # Source checkout: prefer the current build output over any temporary
        # package payload left by an interrupted wheel build.
        directories.append(package_dir.parents[2] / "build" / "engine")
    directories.append(package_dir)
    for directory in directories:
        for name in names:
            yield directory / name
        for configuration in ("Release", "Debug"):
            for name in names:
                yield directory / configuration / name
    located = ctypes.util.find_library("poecraft_engine")
    if located:
        yield located


def _load_library() -> ct.CDLL:
    attempted: list[str] = []
    for candidate in _library_candidates():
        attempted.append(str(candidate))
        if isinstance(candidate, Path) and not candidate.exists():
            continue
        try:
            if (
                isinstance(candidate, Path)
                and os.name == "nt"
                and hasattr(os, "add_dll_directory")
            ):
                # Keep the directory cookie alive with the loaded library so
                # bundled MinGW runtime dependencies remain resolvable.
                cookie = os.add_dll_directory(str(candidate.parent))
                library = ct.CDLL(str(candidate))
                library._poecraft_dll_directory = cookie
                return library
            return ct.CDLL(str(candidate))
        except OSError:
            continue
    raise RuntimeError(
        "poecraft native library was not found; run scripts/build.ps1 or set "
        f"POECRAFT_ENGINE_LIBRARY. Tried: {attempted}"
    )


_lib = _load_library()
_handle = ct.c_void_p

_lib.pc_abi_version.restype = ct.c_uint32
_lib.pc_error_info_init.argtypes = [ct.POINTER(_ErrorInfo)]
_lib.pc_data_load_file.argtypes = [
    ct.c_char_p,
    ct.POINTER(_handle),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_data_load_file.restype = ct.c_int32
_lib.pc_data_destroy.argtypes = [_handle]
_lib.pc_data_get_summary.argtypes = [
    _handle,
    ct.POINTER(_DataSummary),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_data_get_summary.restype = ct.c_int32
_lib.pc_session_create.argtypes = [
    _handle,
    ct.POINTER(_SessionOptions),
    ct.POINTER(_handle),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_session_create.restype = ct.c_int32
_lib.pc_session_destroy.argtypes = [_handle]
_lib.pc_session_get_mod_count.argtypes = [
    _handle,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_session_get_mod_count.restype = ct.c_int32
_lib.pc_session_get_mod_info.argtypes = [
    _handle,
    ct.c_uint32,
    ct.POINTER(_ModInfo),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_session_get_mod_info.restype = ct.c_int32
_lib.pc_action_context_create.argtypes = [
    _handle,
    ct.POINTER(_ContextOptions),
    ct.POINTER(_handle),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_action_context_create.restype = ct.c_int32
_lib.pc_action_context_destroy.argtypes = [_handle]
_lib.pc_item_init.argtypes = [
    _handle,
    ct.POINTER(_ItemInitOptions),
    ct.POINTER(_ItemState),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_item_init.restype = ct.c_int32
_lib.pc_item_add_mod.argtypes = [
    ct.POINTER(_ItemState),
    ct.c_int32,
    ct.c_uint32,
    ct.c_uint16,
    ct.c_uint8,
    ct.POINTER(ct.POINTER(_ModSlot)),
]
_lib.pc_item_add_mod.restype = ct.c_int32
_lib.pc_apply_action.argtypes = [
    _handle,
    ct.POINTER(_ItemState),
    ct.POINTER(_ActionRequest),
    ct.POINTER(_ActionResult),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_apply_action.restype = ct.c_int32
_lib.pc_apply_action_batch.argtypes = [
    _handle,
    ct.POINTER(_ItemState),
    ct.c_uint32,
    ct.POINTER(_ActionRequest),
    ct.POINTER(_ActionResult),
    ct.POINTER(_BatchSummary),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_apply_action_batch.restype = ct.c_int32
_lib.pc_debug_pool_query.argtypes = [
    _handle,
    ct.POINTER(_ItemState),
    ct.POINTER(_PoolQueryRequest),
    ct.POINTER(_PoolDebugEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_PoolDebugSummary),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_debug_pool_query.restype = ct.c_int32

if _lib.pc_abi_version() != ABI_VERSION:
    raise RuntimeError("poecraft native library has an incompatible ABI version")


def _error() -> _ErrorInfo:
    error = _ErrorInfo()
    _lib.pc_error_info_init(ct.byref(error))
    return error


def _check(result: int, error: _ErrorInfo) -> None:
    if result != RESULT_OK:
        raise EngineError(result, _decode(bytes(error.message).split(b"\0", 1)[0]))


def _action_request(action: str | Mapping[str, Any]) -> tuple[_ActionRequest, list[bytes]]:
    spec: Mapping[str, Any] = {"type": action} if isinstance(action, str) else action
    name = str(spec.get("type", "")).lower()
    if name not in _ACTION_TYPES:
        raise ValueError(f"unknown action type: {name!r}")
    request = _ActionRequest()
    request.struct_size = ct.sizeof(request)
    request.abi_version = ABI_VERSION
    request.action_type = _ACTION_TYPES[name]
    keepalive: list[bytes] = []
    if name == "essence":
        key = str(spec.get("essence") or spec.get("essence_key") or "")
        if not key:
            raise ValueError("essence action requires essence_key")
        encoded = key.encode()
        keepalive.append(encoded)
        request.essence_key = encoded
    if name == "fossil":
        raw = spec.get("fossils") or spec.get("fossil_keys")
        if isinstance(raw, str):
            raw = [raw]
        fossils = list(raw or [])
        if not 1 <= len(fossils) <= MAX_FOSSILS:
            raise ValueError("fossil action requires 1-4 fossil keys")
        request.fossil_count = len(fossils)
        for index, key in enumerate(fossils):
            encoded = str(key).encode()
            keepalive.append(encoded)
            request.fossil_keys[index] = encoded
    return request, keepalive


@dataclass(frozen=True)
class ActionResult:
    applied: bool
    added: int
    removed: int


@dataclass(frozen=True)
class BatchSummary:
    item_count: int
    applied_count: int
    total_added: int
    total_removed: int


@dataclass(frozen=True)
class BatchResult:
    items: tuple["Item", ...]
    results: tuple[ActionResult, ...]
    summary: BatchSummary


@dataclass(frozen=True)
class PoolDebugResult(Sequence[dict[str, Any]]):
    entries: tuple[dict[str, Any], ...]
    summary: dict[str, int | bool]

    def __getitem__(self, index: int | slice) -> Any:
        return self.entries[index]

    def __len__(self) -> int:
        return len(self.entries)


@dataclass(frozen=True)
class ModInfo:
    session_mod_id: int
    global_mod_id: int
    key: str
    generation_type: int
    reach_kind: int
    reach_influence: int
    reach_via: str
    primary_group_id: int
    required_level: int

    @property
    def side(self) -> str | None:
        return {0: "prefix", 1: "suffix"}.get(self.generation_type)


class _OwnedHandle:
    _destroy = None

    def __init__(self, handle: _handle):
        self._handle = handle

    @property
    def closed(self) -> bool:
        return not bool(self._handle)

    def close(self) -> None:
        if not self.closed:
            assert self._destroy is not None
            self._destroy(self._handle)
            self._handle = _handle()

    def __enter__(self):
        if self.closed:
            raise RuntimeError("native handle is closed")
        return self

    def __exit__(self, *_args):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class Data(_OwnedHandle):
    _destroy = _lib.pc_data_destroy

    @property
    def summary(self) -> dict[str, int | bool]:
        native = _DataSummary()
        error = _error()
        _check(_lib.pc_data_get_summary(self._handle, ct.byref(native), ct.byref(error)), error)
        return {
            "artifact_schema_version": native.artifact_schema_version,
            "complete_dataset": bool(native.complete_dataset),
            "string_count": native.string_count,
            "tag_count": native.tag_count,
            "item_class_count": native.item_class_count,
            "base_item_count": native.base_item_count,
            "ordinary_session_base_count": native.ordinary_session_base_count,
            "cluster_unsupported_base_count": native.cluster_unsupported_base_count,
            "unsupported_domain_base_count": native.unsupported_domain_base_count,
            "mod_count": native.mod_count,
            "group_count": native.group_count,
        }

    def create_session(self, base_key: str, item_level: int) -> "Session":
        encoded = base_key.encode()
        options = _SessionOptions(ct.sizeof(_SessionOptions), ABI_VERSION, encoded, item_level)
        handle = _handle()
        error = _error()
        _check(
            _lib.pc_session_create(
                self._handle, ct.byref(options), ct.byref(handle), ct.byref(error)
            ),
            error,
        )
        return Session(handle, self)


class Session(_OwnedHandle):
    _destroy = _lib.pc_session_destroy

    def __init__(self, handle: _handle, data: Data):
        super().__init__(handle)
        self._data = data
        self._mods_by_key: dict[str, ModInfo] | None = None

    def create_action_context(self, seed: int = 0) -> "ActionContext":
        options = _ContextOptions(ct.sizeof(_ContextOptions), ABI_VERSION, seed)
        handle = _handle()
        error = _error()
        _check(
            _lib.pc_action_context_create(
                self._handle, ct.byref(options), ct.byref(handle), ct.byref(error)
            ),
            error,
        )
        return ActionContext(handle, self)

    def create_item(
        self, rarity: str = "normal", *, with_implicits: bool = True
    ) -> "Item":
        try:
            rarity_code = _RARITIES[rarity.lower()]
        except KeyError as exc:
            raise ValueError(f"unknown rarity: {rarity!r}") from exc
        options = _ItemInitOptions(
            ct.sizeof(_ItemInitOptions), ABI_VERSION, rarity_code, int(with_implicits)
        )
        state = _ItemState()
        error = _error()
        _check(
            _lib.pc_item_init(
                self._handle, ct.byref(options), ct.byref(state), ct.byref(error)
            ),
            error,
        )
        return Item(self, state)

    @property
    def mod_count(self) -> int:
        count = ct.c_uint32()
        error = _error()
        _check(
            _lib.pc_session_get_mod_count(
                self._handle, ct.byref(count), ct.byref(error)
            ),
            error,
        )
        return count.value

    def mod_info(self, session_mod_id: int) -> ModInfo:
        native = _ModInfo()
        error = _error()
        _check(
            _lib.pc_session_get_mod_info(
                self._handle,
                session_mod_id,
                ct.byref(native),
                ct.byref(error),
            ),
            error,
        )
        return ModInfo(
            native.session_mod_id,
            native.global_mod_id,
            _decode(native.key),
            native.generation_type,
            native.reach_kind,
            native.reach_influence,
            _decode(native.reach_via),
            native.primary_group_id,
            native.required_level,
        )

    def find_mod(self, key: str) -> ModInfo:
        if self._mods_by_key is None:
            self._mods_by_key = {
                info.key: info
                for info in (self.mod_info(index) for index in range(self.mod_count))
            }
        try:
            return self._mods_by_key[key]
        except KeyError as exc:
            raise KeyError(f"mod is not in this session: {key}") from exc


class Item:
    def __init__(self, session: Session, state: _ItemState):
        self._session = session
        self._state = state

    @property
    def rarity(self) -> str:
        return ("normal", "magic", "rare")[self._state.rarity]

    @property
    def prefix_mod_ids(self) -> tuple[int, ...]:
        return tuple(self._state.prefixes[i].mod_id for i in range(self._state.prefix_count))

    @property
    def suffix_mod_ids(self) -> tuple[int, ...]:
        return tuple(self._state.suffixes[i].mod_id for i in range(self._state.suffix_count))

    @property
    def implicit_mod_ids(self) -> tuple[int, ...]:
        return tuple(self._state.implicits[i].mod_id for i in range(self._state.implicit_count))

    @property
    def explicit_count(self) -> int:
        return self._state.prefix_count + self._state.suffix_count

    def copy(self) -> "Item":
        state = _ItemState()
        ct.memmove(ct.byref(state), ct.byref(self._state), ct.sizeof(state))
        return Item(self._session, state)

    def add_mod(
        self,
        mod: str | ModInfo,
        *,
        side: str | None = None,
        fractured: bool = False,
    ) -> None:
        info = self._session.find_mod(mod) if isinstance(mod, str) else mod
        resolved_side = side or info.side
        if resolved_side not in ("prefix", "suffix"):
            raise ValueError("explicit mod side must be prefix or suffix")
        expected_side = {"prefix": 0, "suffix": 1}[resolved_side]
        if info.generation_type != expected_side:
            raise ValueError(
                f"{info.key} is a {info.side}, not a {resolved_side}"
            )
        result = _lib.pc_item_add_mod(
            ct.byref(self._state),
            expected_side,
            info.session_mod_id,
            info.primary_group_id,
            MOD_SLOT_FRACTURED if fractured else 0,
            None,
        )
        if result != RESULT_OK:
            raise EngineError(result, "could not add mod to item")

    @property
    def fractured_mod_ids(self) -> tuple[int, ...]:
        ids: list[int] = []
        for slots, count in (
            (self._state.prefixes, self._state.prefix_count),
            (self._state.suffixes, self._state.suffix_count),
        ):
            ids.extend(
                slots[index].mod_id
                for index in range(count)
                if slots[index].flags & MOD_SLOT_FRACTURED
            )
        return tuple(ids)


class ActionContext(_OwnedHandle):
    _destroy = _lib.pc_action_context_destroy

    def __init__(self, handle: _handle, session: Session):
        super().__init__(handle)
        self._session = session

    def _check_item(self, item: Item) -> None:
        if item._session is not self._session:
            raise ValueError("item belongs to a different session")

    def apply(
        self, item: Item, action: str | Mapping[str, Any]
    ) -> ActionResult:
        self._check_item(item)
        request, keepalive = _action_request(action)
        result = _ActionResult()
        error = _error()
        _check(
            _lib.pc_apply_action(
                self._handle,
                ct.byref(item._state),
                ct.byref(request),
                ct.byref(result),
                ct.byref(error),
            ),
            error,
        )
        del keepalive
        return ActionResult(bool(result.applied), result.added, result.removed)

    def apply_batch(
        self,
        items: Iterable[Item],
        action: str | Mapping[str, Any],
    ) -> BatchResult:
        item_list = list(items)
        for item in item_list:
            self._check_item(item)
        count = len(item_list)
        native_items = (_ItemState * max(1, count))()
        for index, item in enumerate(item_list):
            ct.memmove(
                ct.byref(native_items[index]),
                ct.byref(item._state),
                ct.sizeof(_ItemState),
            )
        native_results = (_ActionResult * max(1, count))()
        request, keepalive = _action_request(action)
        summary = _BatchSummary()
        error = _error()
        _check(
            _lib.pc_apply_action_batch(
                self._handle,
                native_items,
                count,
                ct.byref(request),
                native_results,
                ct.byref(summary),
                ct.byref(error),
            ),
            error,
        )
        del keepalive
        for index, item in enumerate(item_list):
            ct.memmove(
                ct.byref(item._state),
                ct.byref(native_items[index]),
                ct.sizeof(_ItemState),
            )
        return BatchResult(
            tuple(item_list),
            tuple(
                ActionResult(
                    bool(native_results[i].applied),
                    native_results[i].added,
                    native_results[i].removed,
                )
                for i in range(count)
            ),
            BatchSummary(
                summary.item_count,
                summary.applied_count,
                summary.total_added,
                summary.total_removed,
            ),
        )

    def run_batch(
        self,
        item: Item,
        action: str | Mapping[str, Any],
        count: int,
    ) -> BatchResult:
        if count < 0:
            raise ValueError("count must be non-negative")
        self._check_item(item)
        return self.apply_batch((item.copy() for _ in range(count)), action)

    def debug_pool(
        self,
        item: Item,
        action: str | Mapping[str, Any],
        *,
        side: str | None = None,
        include_rejected: bool = False,
    ) -> PoolDebugResult:
        self._check_item(item)
        side_filter = {None: -1, "prefix": 0, "suffix": 1}.get(side)
        if side_filter is None:
            raise ValueError("side must be None, 'prefix', or 'suffix'")
        action_request, keepalive = _action_request(action)
        query = _PoolQueryRequest(
            ct.sizeof(_PoolQueryRequest),
            ABI_VERSION,
            action_request,
            side_filter,
            int(include_rejected),
        )
        count = ct.c_uint32()
        summary = _PoolDebugSummary()
        error = _error()
        first = _lib.pc_debug_pool_query(
            self._handle,
            ct.byref(item._state),
            ct.byref(query),
            None,
            0,
            ct.byref(count),
            ct.byref(summary),
            ct.byref(error),
        )
        if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
            _check(first, error)
        entries = (_PoolDebugEntry * max(1, count.value))()
        error = _error()
        _check(
            _lib.pc_debug_pool_query(
                self._handle,
                ct.byref(item._state),
                ct.byref(query),
                entries,
                count.value,
                ct.byref(count),
                ct.byref(summary),
                ct.byref(error),
            ),
            error,
        )
        del keepalive
        rows = tuple(
            {
                "session_mod_id": row.session_mod_id,
                "global_mod_id": row.global_mod_id,
                "key": _decode(row.key),
                "generation_type": row.generation_type,
                "reach_kind": row.reach_kind,
                "reach_via": _decode(row.reach_via),
                "tag_signature_id": row.tag_signature_id,
                "accepted": row.first_failure == 0,
                "first_failure": row.first_failure,
                "blocking_group_id": (
                    None if row.blocking_group_id == MOD_NONE else row.blocking_group_id
                ),
                "spawn_tag": _decode(row.active_spawn_tag),
                "spawn_weight": row.active_spawn_weight,
                "generation_tag": _decode(row.active_generation_tag),
                "generation_multiplier_pct": row.active_generation_pct,
                "special_multiplier_pct": row.special_multiplier_pct,
                "final_weight": row.final_weight,
            }
            for row in entries[: count.value]
        )
        return PoolDebugResult(
            rows,
            {
                "tag_signature_id": summary.tag_signature_id,
                "cache_hit": bool(summary.cache_hit),
                "candidate_count": summary.candidate_count,
                "prefix_total_weight": summary.prefix_total_weight,
                "suffix_total_weight": summary.suffix_total_weight,
                "combined_total_weight": summary.combined_total_weight,
                "cache_hits": summary.cache_hits,
                "cache_misses": summary.cache_misses,
            },
        )


def load_data(path: str | os.PathLike[str]) -> Data:
    manifest = Path(path)
    if manifest.is_dir():
        manifest /= "manifest.json"
    handle = _handle()
    error = _error()
    _check(
        _lib.pc_data_load_file(
            os.fsencode(manifest), ct.byref(handle), ct.byref(error)
        ),
        error,
    )
    return Data(handle)
