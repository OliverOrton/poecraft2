from __future__ import annotations

import ctypes as ct
import ctypes.util
import json
import os
from collections.abc import Iterable, Iterator, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

MAX_BESTIARY_COST_KEYS = 4
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
    "bench": 11,
    "veiled_chaos": 12,
    "veiled_exalt": 13,
    "unveil": 14,
    "harvest_reforge": 15,
    "harvest_augment": 16,
    "harvest_resist": 17,
    "eldritch_ember": 18,
    "eldritch_ichor": 19,
    "eldritch_exalt": 20,
    "eldritch_chaos": 21,
    "eldritch_annul": 22,
    "influence_exalt": 23,
    "fracture": 24,
    "remove_crafted_modifiers": 25,
}
_RARITIES = {"normal": 0, "magic": 1, "rare": 2}
_TERMINAL_KINDS = {"success": 0, "failure": 1, "stop": 2}
_TERMINAL_NAMES = {value: key for key, value in _TERMINAL_KINDS.items()}
_COST_STATUS = {0: "disabled", 1: "complete", 2: "incomplete"}


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
        ("mod_key", ct.c_char_p),
        ("target_tag", ct.c_char_p),
        ("source_tag", ct.c_char_p),
        ("influence", ct.c_char_p),
        ("tier", ct.c_uint32),
    ]


class _ActionResult(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("applied", ct.c_int32),
        ("added", ct.c_int32),
        ("removed", ct.c_int32),
    ]


class _BestiaryCraftState(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("item", _ItemState),
        ("live_item_identity", ct.c_uint64),
        ("checkpoint_present", ct.c_int32),
        ("checkpoint_bound_identity", ct.c_uint64),
        ("checkpoint", _ItemState),
    ]


class _BestiaryActionInfo(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("action_index", ct.c_uint32),
        ("global_action_id", ct.c_uint32),
        ("global_recipe_id", ct.c_uint32),
        ("action_id", ct.c_char_p),
        ("recipe_id", ct.c_char_p),
        ("family_display_name", ct.c_char_p),
        ("display_name", ct.c_char_p),
        ("operation", ct.c_int32),
        ("transition_kind", ct.c_int32),
        ("emulator_available", ct.c_int32),
        ("calculator_available", ct.c_int32),
        ("strategy_builder_available", ct.c_int32),
        ("solver_available", ct.c_int32),
        ("checkpoint_requirement", ct.c_int32),
        ("checkpoint_effect", ct.c_int32),
        ("identity_requirement", ct.c_int32),
        ("cost_key_count", ct.c_uint32),
        ("cost_keys", ct.c_char_p * MAX_BESTIARY_COST_KEYS),
    ]


class _BestiaryActionRequest(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("action_id", ct.c_char_p),
    ]


class _BestiaryActionResult(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("action_id", ct.c_char_p),
        ("applied", ct.c_int32),
        ("refusal", ct.c_int32),
        ("refusal_key", ct.c_char_p),
        ("refusal_reason", ct.c_char_p),
        ("cost_key_count", ct.c_uint32),
        ("cost_keys", ct.c_char_p * MAX_BESTIARY_COST_KEYS),
        ("consumed_price_key_count", ct.c_uint32),
        ("consumed_price_keys", ct.c_char_p * MAX_BESTIARY_COST_KEYS),
        ("output_item_count", ct.c_uint32),
        ("output_checkpoint_count", ct.c_uint32),
        ("consumed_checkpoint_count", ct.c_uint32),
        ("checkpoint_present", ct.c_int32),
    ]


class _BestiaryCalculation(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("outcome_count", ct.c_uint32),
        ("probability", ct.c_double),
        ("successor", _BestiaryCraftState),
        ("result", _BestiaryActionResult),
    ]


class _BestiarySolverOptionInfo(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("option_index", ct.c_uint32),
        ("option_id", ct.c_char_p),
        ("display_name", ct.c_char_p),
        ("goal_restriction_key", ct.c_char_p),
        ("goal_restriction", ct.c_char_p),
        ("goal_rarity", ct.c_int32),
        ("requires_complete_goal", ct.c_int32),
        ("min_program_actions", ct.c_uint32),
        ("max_program_actions", ct.c_uint32),
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

class _ActionPerfStats(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("pool_requests", ct.c_uint64),
        ("cache_hits", ct.c_uint64),
        ("cache_misses", ct.c_uint64),
        ("candidate_build_ns", ct.c_uint64),
        ("weighted_pool_build_ns", ct.c_uint64),
        ("sampling_calls", ct.c_uint64),
        ("sampling_ns", ct.c_uint64),
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
        ("family_id", ct.c_uint32),
        ("required_level", ct.c_uint32),
        ("group_display_name", ct.c_char_p),
        ("family_tier_index", ct.c_uint32),
        ("text_line_count", ct.c_uint32),
        ("text_lines", ct.POINTER(ct.c_char_p)),
        ("classification_tag_count", ct.c_uint32),
        ("classification_tags", ct.POINTER(ct.c_char_p)),
    ]


class _SimulationOptions(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("target_runs", ct.c_uint64),
        ("seed", ct.c_uint64),
        ("max_actions_per_run", ct.c_uint32),
        ("max_graph_steps_per_run", ct.c_uint32),
        ("max_cost_per_run", ct.c_double),
        ("retained_trace_count", ct.c_uint32),
        ("max_trace_entries", ct.c_uint32),
        ("retained_success_count", ct.c_uint32),
        ("retained_failure_count", ct.c_uint32),
    ]


class _StrategyEvalOptions(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("epsilon", ct.c_double),
        ("max_sweeps", ct.c_uint32),
        ("max_states", ct.c_uint32),
        ("max_pairs", ct.c_uint32),
        ("max_transitions", ct.c_uint32),
        ("top_classes_per_node", ct.c_uint32),
        ("economy", ct.c_void_p),
        ("review_projection_json", ct.c_char_p),
        ("review_projection_json_size", ct.c_size_t),
        ("include_success_normalized", ct.c_int32),
    ]


class _SimulationProgress(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("completed_runs", ct.c_uint64),
        ("target_runs", ct.c_uint64),
        ("finished", ct.c_int32),
    ]


class _SimulationSummary(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("completed_runs", ct.c_uint64),
        ("success_count", ct.c_uint64),
        ("failure_count", ct.c_uint64),
        ("stop_count", ct.c_uint64),
        ("total_actions", ct.c_uint64),
        ("action_limit_count", ct.c_uint64),
        ("cost_limit_count", ct.c_uint64),
        ("step_limit_count", ct.c_uint64),
        ("no_matching_edge_count", ct.c_uint64),
        ("action_not_applied_count", ct.c_uint64),
        ("missing_price_run_count", ct.c_uint64),
        ("costed_action_count", ct.c_uint64),
        ("missing_price_action_count", ct.c_uint64),
        ("known_total_cost", ct.c_double),
        ("cost_status", ct.c_int32),
        ("seed", ct.c_uint64),
        ("target_runs", ct.c_uint64),
    ]


class _TraceEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("step_index", ct.c_uint32),
        ("node_id", ct.c_char_p),
        ("node_kind", ct.c_int32),
        ("action_type", ct.c_int32),
        ("action_applied", ct.c_int32),
        ("matched_edge_id", ct.c_char_p),
        ("cumulative_actions", ct.c_uint64),
        ("known_cumulative_cost", ct.c_double),
        ("cost_complete", ct.c_int32),
        ("terminal_kind", ct.c_int32),
        ("failure_reason", ct.c_int32),
        ("item", _ItemState),
    ]


class _SimulationExample(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("terminal_kind", ct.c_int32),
        ("failure_reason", ct.c_int32),
        ("terminal_node_id", ct.c_char_p),
        ("action_count", ct.c_uint64),
        ("known_total_cost", ct.c_double),
        ("cost_complete", ct.c_int32),
        ("item", _ItemState),
    ]


class _FailureSummaryEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("failure_reason", ct.c_int32),
        ("node_id", ct.c_char_p),
        ("detail", ct.c_char_p),
        ("count", ct.c_uint64),
    ]


class _ActionDistributionEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("node_id", ct.c_char_p),
        ("action_type", ct.c_int32),
        ("count", ct.c_uint64),
    ]


class _PriceKeyEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("key", ct.c_char_p),
        ("missing_count", ct.c_uint64),
    ]


class _ActionDescriptorSampleEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("action_id", ct.c_char_p),
        ("count", ct.c_uint64),
    ]


class _MaterialSampleEntry(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("abi_version", ct.c_uint32),
        ("price_key", ct.c_char_p),
        ("count", ct.c_uint64),
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
_lib.pc_action_context_perf_stats_query.argtypes = [
    _handle,
    ct.POINTER(_ActionPerfStats),
    ct.c_int32,
    ct.POINTER(_ErrorInfo),
]
_lib.pc_action_context_perf_stats_query.restype = ct.c_int32
_lib.pc_action_context_perf_timing_set.argtypes = [
    _handle,
    ct.c_int32,
    ct.POINTER(_ErrorInfo),
]
_lib.pc_action_context_perf_timing_set.restype = ct.c_int32
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
_lib.pc_bestiary_state_init.argtypes = [
    ct.POINTER(_ItemState),
    ct.c_uint64,
    ct.POINTER(_BestiaryCraftState),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_state_init.restype = ct.c_int32
_lib.pc_bestiary_get_action_count.argtypes = [
    _handle,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_get_action_count.restype = ct.c_int32
_lib.pc_bestiary_get_action_info.argtypes = [
    _handle,
    ct.c_uint32,
    ct.POINTER(_BestiaryActionInfo),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_get_action_info.restype = ct.c_int32
_lib.pc_bestiary_find_action.argtypes = [
    _handle,
    ct.c_char_p,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_find_action.restype = ct.c_int32
_lib.pc_bestiary_apply_action.argtypes = [
    _handle,
    ct.POINTER(_BestiaryCraftState),
    ct.POINTER(_BestiaryActionRequest),
    ct.POINTER(_BestiaryActionResult),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_apply_action.restype = ct.c_int32
_lib.pc_bestiary_calculate_action.argtypes = [
    _handle,
    ct.POINTER(_BestiaryCraftState),
    ct.POINTER(_BestiaryActionRequest),
    ct.POINTER(_BestiaryCalculation),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_calculate_action.restype = ct.c_int32
_lib.pc_bestiary_get_solver_option_count.argtypes = [
    _handle,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_get_solver_option_count.restype = ct.c_int32
_lib.pc_bestiary_get_solver_option_info.argtypes = [
    _handle,
    ct.c_uint32,
    ct.POINTER(_BestiarySolverOptionInfo),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_bestiary_get_solver_option_info.restype = ct.c_int32
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
_lib.pc_strategy_compile_json.argtypes = [
    _handle,
    ct.c_char_p,
    ct.c_size_t,
    ct.POINTER(_handle),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_strategy_compile_json.restype = ct.c_int32
_lib.pc_strategy_destroy.argtypes = [_handle]
_lib.pc_strategy_evaluate.argtypes = [
    _handle,
    ct.POINTER(_StrategyEvalOptions),
    ct.c_char_p,
    ct.c_size_t,
    ct.POINTER(ct.c_size_t),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_strategy_evaluate.restype = ct.c_int32
_lib.pc_economy_load_json.argtypes = [
    ct.c_char_p,
    ct.c_size_t,
    ct.POINTER(_handle),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_economy_load_json.restype = ct.c_int32
_lib.pc_economy_destroy.argtypes = [_handle]
_lib.pc_simulator_create.argtypes = [
    _handle,
    _handle,
    _handle,
    ct.POINTER(_handle),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_create.restype = ct.c_int32
_lib.pc_simulator_run_chunk.argtypes = [
    _handle,
    ct.POINTER(_SimulationOptions),
    ct.c_uint32,
    ct.POINTER(_SimulationProgress),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_run_chunk.restype = ct.c_int32
_lib.pc_simulator_get_summary.argtypes = [
    _handle,
    ct.POINTER(_SimulationSummary),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_get_summary.restype = ct.c_int32
_lib.pc_simulator_missing_price_query.argtypes = [
    _handle,
    ct.POINTER(_PriceKeyEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_missing_price_query.restype = ct.c_int32
_lib.pc_simulator_get_trace_count.argtypes = [
    _handle,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_get_trace_count.restype = ct.c_int32
_lib.pc_simulator_trace_query.argtypes = [
    _handle,
    ct.c_uint32,
    ct.POINTER(_TraceEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_trace_query.restype = ct.c_int32
_lib.pc_simulator_get_example_count.argtypes = [
    _handle,
    ct.c_int32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_get_example_count.restype = ct.c_int32
_lib.pc_simulator_example_query.argtypes = [
    _handle,
    ct.c_int32,
    ct.c_uint32,
    ct.POINTER(_SimulationExample),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_example_query.restype = ct.c_int32
_lib.pc_simulator_failure_summary_query.argtypes = [
    _handle,
    ct.POINTER(_FailureSummaryEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_failure_summary_query.restype = ct.c_int32
_lib.pc_simulator_action_distribution_query.argtypes = [
    _handle,
    ct.POINTER(_ActionDistributionEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_action_distribution_query.restype = ct.c_int32
_lib.pc_simulator_action_descriptor_distribution_query.argtypes = [
    _handle,
    ct.POINTER(_ActionDescriptorSampleEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_action_descriptor_distribution_query.restype = ct.c_int32
_lib.pc_simulator_material_distribution_query.argtypes = [
    _handle,
    ct.POINTER(_MaterialSampleEntry),
    ct.c_uint32,
    ct.POINTER(ct.c_uint32),
    ct.POINTER(_ErrorInfo),
]
_lib.pc_simulator_material_distribution_query.restype = ct.c_int32
_lib.pc_simulator_destroy.argtypes = [_handle]

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
    if name in {"bench", "unveil"}:
        key = str(spec.get("mod_key") or "")
        if not key:
            raise ValueError(f"{name} action requires mod_key")
        encoded = key.encode()
        keepalive.append(encoded)
        request.mod_key = encoded
    if name in {"harvest_reforge", "harvest_augment"}:
        tag = str(spec.get("target_tag") or spec.get("tag") or "")
        if not tag:
            raise ValueError(f"{name} action requires target_tag")
        encoded = tag.encode()
        keepalive.append(encoded)
        request.target_tag = encoded
    if name == "harvest_resist":
        source = str(spec.get("source_tag") or "")
        target = str(spec.get("target_tag") or "")
        if not source or not target:
            raise ValueError("harvest_resist requires source_tag and target_tag")
        source_bytes = source.encode()
        target_bytes = target.encode()
        keepalive.extend((source_bytes, target_bytes))
        request.source_tag = source_bytes
        request.target_tag = target_bytes
    if name == "influence_exalt":
        influence = str(spec.get("influence") or "")
        if not influence:
            raise ValueError("influence_exalt requires influence")
        encoded = influence.encode()
        keepalive.append(encoded)
        request.influence = encoded
    if name in {"eldritch_ember", "eldritch_ichor"}:
        tier = int(spec.get("tier") or 0)
        if not 1 <= tier <= 4:
            raise ValueError(f"{name} requires tier 1-4")
        request.tier = tier
    return request, keepalive


def _json_bytes(value: str | Mapping[str, Any]) -> bytes:
    if isinstance(value, str):
        return value.encode()
    return json.dumps(value, separators=(",", ":")).encode()


@dataclass(frozen=True)
class ActionResult:
    applied: bool
    added: int
    removed: int


@dataclass(frozen=True)
class BestiaryActionInfo:
    index: int
    global_action_id: int
    global_recipe_id: int
    id: str
    recipe_id: str
    family_display_name: str
    display_name: str
    operation: str
    transition_kind: str
    emulator_available: bool
    calculator_available: bool
    strategy_builder_available: bool
    solver_available: bool
    checkpoint_requirement: str
    checkpoint_effect: str
    identity_requirement: str
    cost_keys: tuple[str, ...]


@dataclass(frozen=True)
class BestiarySolverOptionInfo:
    index: int
    id: str
    display_name: str
    goal_restriction_key: str
    goal_restriction: str
    goal_rarity: str
    requires_complete_goal: bool
    min_program_actions: int
    max_program_actions: int


@dataclass(frozen=True)
class BestiaryActionResult:
    action_id: str
    applied: bool
    refusal_code: int
    refusal_key: str
    refusal_reason: str
    cost_keys: tuple[str, ...]
    consumed_price_keys: tuple[str, ...]
    output_item_count: int
    output_checkpoint_count: int
    consumed_checkpoint_count: int
    checkpoint_present: bool


@dataclass(frozen=True)
class BestiaryCalculation:
    deterministic: bool
    probability: float
    successor: "BestiaryCraftState"
    result: BestiaryActionResult


def _bestiary_result(native: _BestiaryActionResult) -> BestiaryActionResult:
    return BestiaryActionResult(
        _decode(native.action_id),
        bool(native.applied),
        native.refusal,
        _decode(native.refusal_key),
        _decode(native.refusal_reason),
        tuple(
            _decode(native.cost_keys[index])
            for index in range(native.cost_key_count)
        ),
        tuple(
            _decode(native.consumed_price_keys[index])
            for index in range(native.consumed_price_key_count)
        ),
        native.output_item_count,
        native.output_checkpoint_count,
        native.consumed_checkpoint_count,
        bool(native.checkpoint_present),
    )


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
    family_id: int
    required_level: int
    group_display_name: str
    family_tier_index: int
    text_lines: tuple[str, ...]
    classification_tags: tuple[str, ...]

    @property
    def side(self) -> str | None:
        return {0: "prefix", 1: "suffix"}.get(self.generation_type)


@dataclass(frozen=True)
class SimulationOptions:
    target_runs: int
    seed: int = 0
    max_actions_per_run: int = 100_000
    max_graph_steps_per_run: int = 0
    max_cost_per_run: float = 0.0
    retained_trace_count: int = 10
    max_trace_entries: int = 512
    retained_success_count: int = 5
    retained_failure_count: int = 5

    def _native(self) -> _SimulationOptions:
        if self.target_runs <= 0:
            raise ValueError("target_runs must be positive")
        if self.max_actions_per_run <= 0:
            raise ValueError("max_actions_per_run must be positive")
        return _SimulationOptions(
            ct.sizeof(_SimulationOptions),
            ABI_VERSION,
            self.target_runs,
            self.seed,
            self.max_actions_per_run,
            self.max_graph_steps_per_run,
            self.max_cost_per_run,
            self.retained_trace_count,
            self.max_trace_entries,
            self.retained_success_count,
            self.retained_failure_count,
        )


@dataclass(frozen=True)
class SimulationProgress:
    completed_runs: int
    target_runs: int
    finished: bool


@dataclass(frozen=True)
class TraceEntry:
    step_index: int
    node_id: str
    node_kind: int
    action_type: int
    action_applied: bool
    matched_edge_id: str
    cumulative_actions: int
    known_cumulative_cost: float
    cost_complete: bool
    terminal_kind: str | None
    failure_reason: int
    item: "Item"


@dataclass(frozen=True)
class StrategyTrace:
    entries: tuple[TraceEntry, ...]


@dataclass(frozen=True)
class SimulationExample:
    terminal_kind: str
    failure_reason: int
    terminal_node_id: str
    action_count: int
    known_total_cost: float
    cost_complete: bool
    item: "Item"


@dataclass(frozen=True)
class SimulationResult:
    summary: dict[str, int | float | str]
    action_distribution: tuple[dict[str, int | str], ...]
    traces: tuple[StrategyTrace, ...]
    success_examples: tuple[SimulationExample, ...]
    failure_examples: tuple[SimulationExample, ...]
    stop_examples: tuple[SimulationExample, ...]
    failure_summaries: tuple[dict[str, int | str], ...]
    missing_prices: dict[str, int]
    sampled_accounting: dict[str, Any]


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

    @property
    def bestiary_actions(self) -> tuple[BestiaryActionInfo, ...]:
        count = ct.c_uint32()
        error = _error()
        _check(
            _lib.pc_bestiary_get_action_count(
                self._handle, ct.byref(count), ct.byref(error)
            ),
            error,
        )
        actions: list[BestiaryActionInfo] = []
        for index in range(count.value):
            native = _BestiaryActionInfo()
            error = _error()
            _check(
                _lib.pc_bestiary_get_action_info(
                    self._handle, index, ct.byref(native), ct.byref(error)
                ),
                error,
            )
            actions.append(
                BestiaryActionInfo(
                    native.action_index,
                    native.global_action_id,
                    native.global_recipe_id,
                    _decode(native.action_id),
                    _decode(native.recipe_id),
                    _decode(native.family_display_name),
                    _decode(native.display_name),
                    ("create", "restore")[native.operation],
                    "deterministic" if native.transition_kind == 0 else "unknown",
                    bool(native.emulator_available),
                    bool(native.calculator_available),
                    bool(native.strategy_builder_available),
                    bool(native.solver_available),
                    ("absent", "present")[native.checkpoint_requirement],
                    ("create", "consume")[native.checkpoint_effect],
                    ("current_item", "same_item")[native.identity_requirement],
                    tuple(
                        _decode(native.cost_keys[key_index])
                        for key_index in range(native.cost_key_count)
                    ),
                )
            )
        return tuple(actions)

    @property
    def bestiary_solver_options(self) -> tuple[BestiarySolverOptionInfo, ...]:
        count = ct.c_uint32()
        error = _error()
        _check(
            _lib.pc_bestiary_get_solver_option_count(
                self._handle, ct.byref(count), ct.byref(error)
            ),
            error,
        )
        options: list[BestiarySolverOptionInfo] = []
        for index in range(count.value):
            native = _BestiarySolverOptionInfo()
            error = _error()
            _check(
                _lib.pc_bestiary_get_solver_option_info(
                    self._handle, index, ct.byref(native), ct.byref(error)
                ),
                error,
            )
            options.append(
                BestiarySolverOptionInfo(
                    native.option_index,
                    _decode(native.option_id),
                    _decode(native.display_name),
                    _decode(native.goal_restriction_key),
                    _decode(native.goal_restriction),
                    ("normal", "magic", "rare")[native.goal_rarity],
                    bool(native.requires_complete_goal),
                    native.min_program_actions,
                    native.max_program_actions,
                )
            )
        return tuple(options)

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
        self._next_bestiary_identity = 1

    def create_bestiary_state(self, item: "Item") -> "BestiaryCraftState":
        if item._session is not self:
            raise ValueError("item belongs to a different session")
        native = _BestiaryCraftState()
        error = _error()
        identity = self._next_bestiary_identity
        self._next_bestiary_identity += 1
        _check(
            _lib.pc_bestiary_state_init(
                ct.byref(item._state),
                identity,
                ct.byref(native),
                ct.byref(error),
            ),
            error,
        )
        return BestiaryCraftState(self, native, item)

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

    def compile_strategy(
        self, strategy: str | Mapping[str, Any]
    ) -> "Strategy":
        encoded = _json_bytes(strategy)
        handle = _handle()
        error = _error()
        _check(
            _lib.pc_strategy_compile_json(
                self._handle,
                encoded,
                len(encoded),
                ct.byref(handle),
                ct.byref(error),
            ),
            error,
        )
        return Strategy(handle, self)

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
        lines: list[str] = []
        for index in range(native.text_line_count):
            ptr = native.text_lines[index] if native.text_lines else None
            if ptr is None:
                continue
            lines.append(_decode(ptr))
        tags: list[str] = []
        for index in range(native.classification_tag_count):
            ptr = (
                native.classification_tags[index]
                if native.classification_tags
                else None
            )
            if ptr is None:
                continue
            tags.append(_decode(ptr))
        return ModInfo(
            native.session_mod_id,
            native.global_mod_id,
            _decode(native.key),
            native.generation_type,
            native.reach_kind,
            native.reach_influence,
            _decode(native.reach_via),
            native.primary_group_id,
            native.family_id,
            native.required_level,
            _decode(native.group_display_name),
            native.family_tier_index,
            tuple(lines),
            tuple(tags),
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
    def item_flags(self) -> int:
        return int(self._state.item_flags)

    @property
    def generic_influence_bits(self) -> int:
        return int(self._state.generic_influence_bits)

    @property
    def searing_exarch_tier(self) -> int:
        return int(self._state.searing_exarch_tier)

    @property
    def eater_of_worlds_tier(self) -> int:
        return int(self._state.eater_of_worlds_tier)

    @property
    def veiled_option_mod_ids(self) -> tuple[int, ...]:
        for slots, count in (
            (self._state.prefixes, self._state.prefix_count),
            (self._state.suffixes, self._state.suffix_count),
        ):
            for index in range(count):
                slot = slots[index]
                if slot.flags & (1 << 2):
                    return tuple(
                        slot.veiled_option_mod_ids[i]
                        for i in range(slot.veiled_option_count)
                    )
        return ()

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


class BestiaryCraftState:
    """One live item plus its optional engine-authoritative Imprint checkpoint."""

    def __init__(
        self,
        session: Session,
        state: _BestiaryCraftState,
        item: Item | None = None,
    ):
        self._session = session
        self._state = state
        self._item = item or Item(session, self._state.item)
        self._item._state = self._state.item

    @property
    def item(self) -> Item:
        return self._item

    @property
    def live_item_identity(self) -> int:
        return self._state.live_item_identity

    @property
    def checkpoint_present(self) -> bool:
        return bool(self._state.checkpoint_present)

    @property
    def checkpoint_bound_identity(self) -> int | None:
        return (
            self._state.checkpoint_bound_identity
            if self.checkpoint_present
            else None
        )

    def apply(self, action_id: str) -> BestiaryActionResult:
        encoded = action_id.encode()
        request = _BestiaryActionRequest(
            ct.sizeof(_BestiaryActionRequest), ABI_VERSION, encoded
        )
        native = _BestiaryActionResult()
        error = _error()
        _check(
            _lib.pc_bestiary_apply_action(
                self._session._data._handle,
                ct.byref(self._state),
                ct.byref(request),
                ct.byref(native),
                ct.byref(error),
            ),
            error,
        )
        return _bestiary_result(native)

    def calculate(self, action_id: str) -> BestiaryCalculation:
        encoded = action_id.encode()
        request = _BestiaryActionRequest(
            ct.sizeof(_BestiaryActionRequest), ABI_VERSION, encoded
        )
        native = _BestiaryCalculation()
        error = _error()
        _check(
            _lib.pc_bestiary_calculate_action(
                self._session._data._handle,
                ct.byref(self._state),
                ct.byref(request),
                ct.byref(native),
                ct.byref(error),
            ),
            error,
        )
        successor = _BestiaryCraftState()
        ct.memmove(
            ct.byref(successor),
            ct.byref(native.successor),
            ct.sizeof(_BestiaryCraftState),
        )
        return BestiaryCalculation(
            native.outcome_count == 1,
            native.probability,
            BestiaryCraftState(self._session, successor),
            _bestiary_result(native.result),
        )


def _copy_item_state(source: _ItemState) -> _ItemState:
    state = _ItemState()
    ct.memmove(ct.byref(state), ct.byref(source), ct.sizeof(state))
    return state


class Strategy(_OwnedHandle):
    _destroy = _lib.pc_strategy_destroy

    def __init__(self, handle: _handle, session: Session):
        super().__init__(handle)
        self._session = session

    def create_simulator(self, economy: "Economy | None" = None) -> "Simulator":
        handle = _handle()
        error = _error()
        economy_handle = economy._handle if economy is not None else _handle()
        _check(
            _lib.pc_simulator_create(
                self._session._handle,
                self._handle,
                economy_handle,
                ct.byref(handle),
                ct.byref(error),
            ),
            error,
        )
        return Simulator(handle, self._session, self, economy)

    def evaluate(
        self,
        *,
        economy: "Economy | None" = None,
        review_projection: str | Mapping[str, Any] | None = None,
        include_success_normalized: bool = False,
        epsilon: float = 0.0,
        max_sweeps: int = 0,
        max_states: int = 0,
        max_pairs: int = 0,
        max_transitions: int = 0,
        top_classes_per_node: int = 0,
    ) -> dict[str, Any]:
        review = (
            b""
            if review_projection is None
            else _json_bytes(review_projection)
        )
        options = _StrategyEvalOptions(
            ct.sizeof(_StrategyEvalOptions),
            ABI_VERSION,
            epsilon,
            max_sweeps,
            max_states,
            max_pairs,
            max_transitions,
            top_classes_per_node,
            economy._handle if economy is not None else None,
            review if review else None,
            len(review),
            int(include_success_normalized),
        )
        length = ct.c_size_t()
        error = _error()
        _check(
            _lib.pc_strategy_evaluate(
                self._handle,
                ct.byref(options),
                None,
                0,
                ct.byref(length),
                ct.byref(error),
            ),
            error,
        )
        buffer = ct.create_string_buffer(length.value + 1)
        error = _error()
        _check(
            _lib.pc_strategy_evaluate(
                self._handle,
                ct.byref(options),
                buffer,
                len(buffer),
                ct.byref(length),
                ct.byref(error),
            ),
            error,
        )
        return json.loads(buffer.raw[: length.value].decode("utf-8"))


class Economy(_OwnedHandle):
    _destroy = _lib.pc_economy_destroy


class Simulator(_OwnedHandle):
    _destroy = _lib.pc_simulator_destroy

    def __init__(
        self,
        handle: _handle,
        session: Session,
        strategy: Strategy,
        economy: Economy | None,
    ):
        super().__init__(handle)
        self._session = session
        self._strategy = strategy
        self._economy = economy

    def run_chunk(
        self,
        options: SimulationOptions,
        max_completed_runs: int,
    ) -> SimulationProgress:
        if max_completed_runs < 0:
            raise ValueError("max_completed_runs must be non-negative")
        native_options = options._native()
        progress = _SimulationProgress()
        error = _error()
        _check(
            _lib.pc_simulator_run_chunk(
                self._handle,
                ct.byref(native_options),
                max_completed_runs,
                ct.byref(progress),
                ct.byref(error),
            ),
            error,
        )
        return SimulationProgress(
            progress.completed_runs,
            progress.target_runs,
            bool(progress.finished),
        )

    @property
    def summary(self) -> dict[str, int | float | str]:
        native = _SimulationSummary()
        error = _error()
        _check(
            _lib.pc_simulator_get_summary(
                self._handle, ct.byref(native), ct.byref(error)
            ),
            error,
        )
        return {
            "completed_runs": native.completed_runs,
            "success_count": native.success_count,
            "failure_count": native.failure_count,
            "stop_count": native.stop_count,
            "total_actions": native.total_actions,
            "action_limit_count": native.action_limit_count,
            "cost_limit_count": native.cost_limit_count,
            "step_limit_count": native.step_limit_count,
            "no_matching_edge_count": native.no_matching_edge_count,
            "action_not_applied_count": native.action_not_applied_count,
            "missing_price_run_count": native.missing_price_run_count,
            "costed_action_count": native.costed_action_count,
            "missing_price_action_count": native.missing_price_action_count,
            "known_total_cost": native.known_total_cost,
            "cost_status": _COST_STATUS.get(native.cost_status, "unknown"),
            "seed": native.seed,
            "target_runs": native.target_runs,
        }

    @property
    def traces(self) -> tuple[StrategyTrace, ...]:
        count = ct.c_uint32()
        error = _error()
        _check(
            _lib.pc_simulator_get_trace_count(
                self._handle, ct.byref(count), ct.byref(error)
            ),
            error,
        )
        traces: list[StrategyTrace] = []
        for trace_index in range(count.value):
            entry_count = ct.c_uint32()
            error = _error()
            first = _lib.pc_simulator_trace_query(
                self._handle,
                trace_index,
                None,
                0,
                ct.byref(entry_count),
                ct.byref(error),
            )
            if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
                _check(first, error)
            entries = (_TraceEntry * max(1, entry_count.value))()
            error = _error()
            _check(
                _lib.pc_simulator_trace_query(
                    self._handle,
                    trace_index,
                    entries,
                    entry_count.value,
                    ct.byref(entry_count),
                    ct.byref(error),
                ),
                error,
            )
            traces.append(
                StrategyTrace(
                    tuple(
                        TraceEntry(
                            row.step_index,
                            _decode(row.node_id),
                            row.node_kind,
                            row.action_type,
                            bool(row.action_applied),
                            _decode(row.matched_edge_id),
                            row.cumulative_actions,
                            row.known_cumulative_cost,
                            bool(row.cost_complete),
                            _TERMINAL_NAMES.get(row.terminal_kind),
                            row.failure_reason,
                            Item(self._session, _copy_item_state(row.item)),
                        )
                        for row in entries[: entry_count.value]
                    )
                )
            )
        return tuple(traces)

    def examples(self, terminal_kind: str) -> tuple[SimulationExample, ...]:
        try:
            kind = _TERMINAL_KINDS[terminal_kind]
        except KeyError as exc:
            raise ValueError(f"unknown terminal kind: {terminal_kind!r}") from exc
        count = ct.c_uint32()
        error = _error()
        _check(
            _lib.pc_simulator_get_example_count(
                self._handle, kind, ct.byref(count), ct.byref(error)
            ),
            error,
        )
        examples: list[SimulationExample] = []
        for index in range(count.value):
            native = _SimulationExample()
            error = _error()
            _check(
                _lib.pc_simulator_example_query(
                    self._handle,
                    kind,
                    index,
                    ct.byref(native),
                    ct.byref(error),
                ),
                error,
            )
            examples.append(
                SimulationExample(
                    _TERMINAL_NAMES[native.terminal_kind],
                    native.failure_reason,
                    _decode(native.terminal_node_id),
                    native.action_count,
                    native.known_total_cost,
                    bool(native.cost_complete),
                    Item(self._session, _copy_item_state(native.item)),
                )
            )
        return tuple(examples)

    @property
    def failure_summaries(self) -> tuple[dict[str, int | str], ...]:
        count = ct.c_uint32()
        error = _error()
        first = _lib.pc_simulator_failure_summary_query(
            self._handle, None, 0, ct.byref(count), ct.byref(error)
        )
        if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
            _check(first, error)
        rows = (_FailureSummaryEntry * max(1, count.value))()
        error = _error()
        _check(
            _lib.pc_simulator_failure_summary_query(
                self._handle,
                rows,
                count.value,
                ct.byref(count),
                ct.byref(error),
            ),
            error,
        )
        return tuple(
            {
                "failure_reason": row.failure_reason,
                "node_id": _decode(row.node_id),
                "detail": _decode(row.detail),
                "count": row.count,
            }
            for row in rows[: count.value]
        )

    @property
    def action_distribution(self) -> tuple[dict[str, int | str], ...]:
        count = ct.c_uint32()
        error = _error()
        first = _lib.pc_simulator_action_distribution_query(
            self._handle, None, 0, ct.byref(count), ct.byref(error)
        )
        if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
            _check(first, error)
        rows = (_ActionDistributionEntry * max(1, count.value))()
        error = _error()
        _check(
            _lib.pc_simulator_action_distribution_query(
                self._handle,
                rows,
                count.value,
                ct.byref(count),
                ct.byref(error),
            ),
            error,
        )
        return tuple(
            {
                "node_id": _decode(row.node_id),
                "action_type": row.action_type,
                "count": row.count,
            }
            for row in rows[: count.value]
        )

    @property
    def missing_prices(self) -> dict[str, int]:
        count = ct.c_uint32()
        error = _error()
        first = _lib.pc_simulator_missing_price_query(
            self._handle, None, 0, ct.byref(count), ct.byref(error)
        )
        if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
            _check(first, error)
        rows = (_PriceKeyEntry * max(1, count.value))()
        error = _error()
        _check(
            _lib.pc_simulator_missing_price_query(
                self._handle,
                rows,
                count.value,
                ct.byref(count),
                ct.byref(error),
            ),
            error,
        )
        return {
            _decode(row.key): row.missing_count for row in rows[: count.value]
        }

    @property
    def sampled_accounting(self) -> dict[str, Any]:
        summary = self.summary
        action_count = ct.c_uint32()
        error = _error()
        first = _lib.pc_simulator_action_descriptor_distribution_query(
            self._handle, None, 0, ct.byref(action_count), ct.byref(error)
        )
        if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
            _check(first, error)
        actions = (_ActionDescriptorSampleEntry * max(1, action_count.value))()
        error = _error()
        _check(
            _lib.pc_simulator_action_descriptor_distribution_query(
                self._handle,
                actions,
                action_count.value,
                ct.byref(action_count),
                ct.byref(error),
            ),
            error,
        )
        material_count = ct.c_uint32()
        error = _error()
        first = _lib.pc_simulator_material_distribution_query(
            self._handle, None, 0, ct.byref(material_count), ct.byref(error)
        )
        if first not in (RESULT_OK, RESULT_BUFFER_TOO_SMALL):
            _check(first, error)
        materials = (_MaterialSampleEntry * max(1, material_count.value))()
        error = _error()
        _check(
            _lib.pc_simulator_material_distribution_query(
                self._handle,
                materials,
                material_count.value,
                ct.byref(material_count),
                ct.byref(error),
            ),
            error,
        )
        runs = int(summary["completed_runs"])
        return {
            "evidence_source": "simulator_sample",
            "sample_count": runs,
            "seed": int(summary["seed"]),
            "actions": tuple(
                {
                    "action_id": _decode(row.action_id),
                    "count": row.count,
                    "average_per_invocation": row.count / runs if runs else 0.0,
                }
                for row in actions[: action_count.value]
            ),
            "materials": tuple(
                {
                    "price_key": _decode(row.price_key),
                    "count": row.count,
                    "average_per_invocation": row.count / runs if runs else 0.0,
                }
                for row in materials[: material_count.value]
            ),
        }

    def run(
        self,
        options: SimulationOptions,
        *,
        chunk_size: int = 10_000,
    ) -> SimulationResult:
        if chunk_size <= 0:
            raise ValueError("chunk_size must be positive")
        while True:
            progress = self.run_chunk(options, chunk_size)
            if progress.finished:
                break
        return SimulationResult(
            self.summary,
            self.action_distribution,
            self.traces,
            self.examples("success"),
            self.examples("failure"),
            self.examples("stop"),
            self.failure_summaries,
            self.missing_prices,
            self.sampled_accounting,
        )


class ActionContext(_OwnedHandle):
    _destroy = _lib.pc_action_context_destroy

    def __init__(self, handle: _handle, session: Session):
        super().__init__(handle)
        self._session = session

    def _check_item(self, item: Item) -> None:
        if item._session is not self._session:
            raise ValueError("item belongs to a different session")

    def performance_stats(self, *, reset: bool = False) -> dict[str, int | float]:
        stats = _ActionPerfStats()
        error = _error()
        _check(
            _lib.pc_action_context_perf_stats_query(
                self._handle,
                ct.byref(stats),
                int(reset),
                ct.byref(error),
            ),
            error,
        )
        requests = stats.pool_requests
        return {
            "pool_requests": requests,
            "cache_hits": stats.cache_hits,
            "cache_misses": stats.cache_misses,
            "cache_hit_rate": stats.cache_hits / requests if requests else 0.0,
            "candidate_build_ns": stats.candidate_build_ns,
            "weighted_pool_build_ns": stats.weighted_pool_build_ns,
            "sampling_calls": stats.sampling_calls,
            "sampling_ns": stats.sampling_ns,
        }

    def set_performance_timing(self, enabled: bool) -> None:
        error = _error()
        _check(
            _lib.pc_action_context_perf_timing_set(
                self._handle,
                int(enabled),
                ct.byref(error),
            ),
            error,
        )

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


def load_economy(economy: str | Mapping[str, Any]) -> Economy:
    encoded = _json_bytes(economy)
    handle = _handle()
    error = _error()
    _check(
        _lib.pc_economy_load_json(
            encoded, len(encoded), ct.byref(handle), ct.byref(error)
        ),
        error,
    )
    return Economy(handle)
