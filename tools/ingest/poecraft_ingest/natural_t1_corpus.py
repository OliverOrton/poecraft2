"""Deterministic natural-T1 benchmark corpus generation.

Python selects candidate families and strata. The native engine remains the
authority for natural weight, T1 eligibility, exclusion groups, affix
capacity, goal resolution, and goal-relevant action admission.
"""

from __future__ import annotations

import hashlib
import json
import math
import random
import subprocess
import time
from collections import Counter, defaultdict
from dataclasses import asdict
from pathlib import Path
from typing import Any, Iterable, Mapping

from poecraft_engine import (
    BaseInfo,
    GoalFeasibility,
    ModInfo,
    engine_abi_version,
    engine_compiler,
    engine_library_path,
    load_data,
)


GENERATOR_VERSION = "natural-t1-corpus-v1"
CONFIG_SCHEMA = "natural_t1_generator_config_v1"
MANIFEST_SCHEMA = "natural_t1_benchmark_corpus_v1"
REPORT_SCHEMA = "natural_t1_generation_report_v1"
CASE_SCHEMA = "solver_benchmark_case_v1"


def _json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _git(root: Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", *args],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return completed.stdout.strip()


def _relative(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def _load_base_list(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    if path.suffix.lower() == ".json":
        value = json.loads(text)
        if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
            raise ValueError("base_selection.list JSON must be an array of paths")
        return list(value)
    return [line.strip() for line in text.splitlines() if line.strip()]


def _select_bases(
    config: Mapping[str, Any],
    config_path: Path,
    bases: Iterable[BaseInfo],
) -> tuple[list[BaseInfo], list[str]]:
    selection = dict(config.get("base_selection") or {})
    wanted_paths = set(str(value) for value in selection.get("paths", []))
    list_path = selection.get("list")
    if list_path:
        source = Path(str(list_path))
        if not source.is_absolute():
            source = config_path.parent / source
        wanted_paths.update(_load_base_list(source))
    pool_name = selection.get("pool")
    if pool_name:
        pools = config.get("base_pools") or {}
        if pool_name not in pools:
            raise ValueError(f"unknown base pool: {pool_name}")
        wanted_paths.update(str(value) for value in pools[pool_name])
    wanted_classes = set(str(value) for value in selection.get("classes", []))

    all_bases = list(bases)
    by_path = {base.metadata_path: base for base in all_bases}
    missing = sorted(path for path in wanted_paths if path not in by_path)
    chosen = {
        base.metadata_path: base
        for base in all_bases
        if base.metadata_path in wanted_paths or base.item_class_key in wanted_classes
    }
    if not wanted_paths and not wanted_classes:
        chosen = {base.metadata_path: base for base in all_bases}
    return [chosen[key] for key in sorted(chosen)], missing


def _candidate_families(
    session: Any,
    filters: Mapping[str, Any],
    item_level: int,
) -> dict[str, list[ModInfo]]:
    include_families = set(str(v) for v in filters.get("families", []))
    required_tags = set(str(v) for v in filters.get("required_tags", []))
    excluded_tags = set(str(v) for v in filters.get("excluded_tags", []))
    tag_mode = str(filters.get("tag_mode", "any"))
    if tag_mode not in {"any", "all"}:
        raise ValueError("natural_filters.tag_mode must be any or all")

    by_side: dict[str, dict[int, ModInfo]] = {
        "prefix": {},
        "suffix": {},
    }
    for mod_id in range(session.mod_count):
        info = session.mod_info(mod_id)
        # Candidate enumeration is deliberately narrower than proof: this
        # initial corpus uses uninfluenced starts, so base-reach T1 families
        # are the useful proposal set. The native feasibility query still
        # rechecks every accepted combination's actual masks and weights.
        if (
            info.side not in by_side
            or info.family_tier_index != 1
            or info.reach_kind != 0
            or info.required_level > item_level
        ):
            continue
        if include_families and info.key not in include_families:
            continue
        tags = set(info.classification_tags)
        if excluded_tags.intersection(tags):
            continue
        if required_tags:
            matches = required_tags.issubset(tags) if tag_mode == "all" else bool(required_tags.intersection(tags))
            if not matches:
                continue
        current = by_side[info.side].get(info.family_id)
        if current is None or info.key < current.key:
            by_side[info.side][info.family_id] = info
    return {
        side: sorted(values.values(), key=lambda info: (info.family_id, info.key))
        for side, values in by_side.items()
    }


def _start_item(session: Any, spec: Mapping[str, Any]) -> Any:
    item = session.create_item(
        rarity=str(spec.get("rarity", "rare")),
        with_implicits=bool(spec.get("with_implicits", False)),
    )
    for entry in spec.get("mods", []):
        flags = set(str(value) for value in entry.get("flags", []))
        unknown = flags.difference({"fractured"})
        if unknown:
            raise ValueError(f"unsupported start mod flags: {sorted(unknown)}")
        item.add_mod(
            str(entry["key"]),
            fractured="fractured" in flags,
        )
    return item


def _goal(keys: list[str]) -> dict[str, Any]:
    return {
        "version": "v1",
        "rarity": "rare",
        "action_mode": "goal_relevant",
        "min_satisfied_slots": len(keys),
        "slots": [
            {"family_mod_key": key, "min_tier": 1}
            for key in keys
        ],
    }


def _side_code(value: str, count: int) -> str:
    code = value.strip().upper()
    if len(code) != count or any(char not in "PS" for char in code):
        raise ValueError(f"side composition {value!r} does not match goal_count={count}")
    return code


def _resolved_goals(session: Any, feasibility: GoalFeasibility) -> list[dict[str, Any]]:
    resolved: list[dict[str, Any]] = []
    for slot, mod_id in enumerate(feasibility.witness_mod_ids):
        if mod_id is None:
            raise ValueError("feasible all-slot goal did not return a complete witness")
        info = session.mod_info(mod_id)
        resolved.append(
            {
                "slot": slot,
                "family_mod_key": info.key,
                "family_id": info.family_id,
                "session_mod_id": info.session_mod_id,
                "global_mod_id": info.global_mod_id,
                "side": info.side,
                "required_level": info.required_level,
                "family_tier_index": info.family_tier_index,
                "reach_kind": info.reach_kind,
                "reach_via": info.reach_via,
                "classification_tags": list(info.classification_tags),
                "natural_mod_count": feasibility.slot_natural_mod_counts[slot],
                "natural_weight": feasibility.slot_natural_weights[slot],
                "single_draw_probability": feasibility.slot_single_draw_probabilities[slot],
            }
        )
    return resolved


def _economy(root: Path, value: Mapping[str, Any]) -> dict[str, Any]:
    snapshot_path = root / str(value["snapshot_path"])
    payload = json.loads(snapshot_path.read_text(encoding="utf-8"))
    metadata = payload.get("metadata") or {}
    return {
        "version": "v1",
        "id": payload["id"],
        "snapshot_path": _relative(root, snapshot_path),
        "content_sha256": _sha256(snapshot_path),
        "source_cutoff_at_utc": metadata.get("created_at_utc"),
        "league_key": metadata.get("league_key"),
        "league_name": metadata.get("league_name"),
        "manual_overrides": dict(value.get("manual_overrides") or {}),
        "fallback_price": value.get("fallback_price"),
    }


def _artifact_provenance(root: Path, artifact_dir: Path) -> dict[str, Any]:
    manifest_path = artifact_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    files = manifest.get("files") or {}
    source = manifest.get("source") or {}
    return {
        "manifest_relative_path": _relative(root, manifest_path),
        "manifest_sha256": _sha256(manifest_path),
        "artifact_schema_version": manifest.get("artifact_schema_version"),
        "source_version": source.get("source_version"),
        "source_data_hash": source.get("data_hash"),
        "game_data_sha256": (files.get("game-data.json") or {}).get("sha256"),
        "strings_sha256": (files.get("strings.json") or {}).get("sha256"),
    }


def _generation_provenance(
    root: Path,
    output_dir: Path,
    seed: int,
) -> dict[str, Any]:
    status_lines = _git(root, "status", "--porcelain").splitlines()
    output_relative = _relative(root, output_dir).rstrip("/") + "/"
    relevant_status = [
        line for line in status_lines
        if not line[3:].replace("\\", "/").startswith(output_relative)
    ]
    binary = engine_library_path()
    return {
        "generator_version": GENERATOR_VERSION,
        "seed": seed,
        "engine_commit": _git(root, "rev-parse", "HEAD"),
        "engine_dirty": bool(relevant_status),
        "engine_dirty_paths": relevant_status,
        "engine_binary_path": _relative(root, binary),
        "engine_binary_sha256": _sha256(binary),
        "engine_abi_version": engine_abi_version(),
        "compiler": engine_compiler(),
    }


def _feasibility_dict(value: GoalFeasibility) -> dict[str, Any]:
    result = asdict(value)
    result["witness_mod_ids"] = list(value.witness_mod_ids)
    result["slot_natural_mod_counts"] = list(value.slot_natural_mod_counts)
    result["slot_natural_weights"] = list(value.slot_natural_weights)
    result["slot_single_draw_probabilities"] = list(value.slot_single_draw_probabilities)
    return result


def generate_corpus(
    config_path: Path,
    *,
    output_override: Path | None = None,
) -> dict[str, Any]:
    config_path = config_path.resolve()
    config = json.loads(config_path.read_text(encoding="utf-8"))
    if config.get("schema_version") != CONFIG_SCHEMA:
        raise ValueError(f"config schema_version must be {CONFIG_SCHEMA}")
    root = Path(__file__).resolve().parents[3]
    artifact_dir = root / str(config.get("artifact", "data/compiled/current"))
    output_dir = (output_override or (root / str(config["output_directory"]))).resolve()
    seed = int(config["seed"])
    rng = random.Random(seed)
    watchdog_seconds = float(config.get("watchdog_seconds", 60.0))
    if watchdog_seconds <= 0 or watchdog_seconds > 900:
        raise ValueError("watchdog_seconds must be in (0, 900]")
    started = time.monotonic()

    economy = _economy(root, config["economy"])
    artifact = _artifact_provenance(root, artifact_dir)
    generation = _generation_provenance(root, output_dir, seed)
    caps = dict(config["resource_caps"])
    start_spec = dict(config.get("start") or {"rarity": "rare", "with_implicits": False, "mods": []})
    item_level = int(config.get("item_level", 86))
    filters = dict(config.get("natural_filters") or {})

    counters: Counter[str] = Counter()
    counters.update(
        {
            "attempts": 0,
            "accepts": 0,
            "duplicates": 0,
            "ineligible_bases": 0,
            "zero_weight_families": 0,
            "group_slot_conflicts": 0,
            "infeasible": 0,
            "unknown": 0,
            "exhausted_strata": 0,
        }
    )
    reason_counts: Counter[str] = Counter()
    rejection_examples: dict[str, list[dict[str, Any]]] = defaultdict(list)
    probe_results: list[dict[str, Any]] = []
    exhausted: list[str] = []
    cases: list[dict[str, Any]] = []
    case_signatures: set[str] = set()
    attempt_signatures: set[str] = set()
    accepted_by_stratum: Counter[str] = Counter()

    data = load_data(artifact_dir)
    try:
        selected_bases, missing_paths = _select_bases(config, config_path, data.bases)
        counters["ineligible_bases"] += len(missing_paths)
        for path in missing_paths[:16]:
            rejection_examples["ineligible_bases"].append({"base_metadata_path": path, "reason": "not_found"})
        eligible_bases = []
        for base in selected_bases:
            if base.session_support != 0 or (base.drop_level >= 0 and base.drop_level > item_level):
                counters["ineligible_bases"] += 1
                if len(rejection_examples["ineligible_bases"]) < 16:
                    rejection_examples["ineligible_bases"].append(
                        {"base_metadata_path": base.metadata_path, "reason": "unsupported_or_drop_level"}
                    )
                continue
            eligible_bases.append(base)
        if not eligible_bases:
            raise ValueError("base selection produced no eligible bases")
        by_path = {base.metadata_path: base for base in eligible_bases}
        session_cache: dict[str, tuple[Any, dict[str, list[ModInfo]]]] = {}

        def session_for(base: BaseInfo) -> tuple[Any, dict[str, list[ModInfo]]]:
            cached = session_cache.get(base.metadata_path)
            if cached is None:
                session = data.create_session(base.metadata_path, item_level)
                cached = (
                    session,
                    _candidate_families(session, filters, item_level),
                )
                session_cache[base.metadata_path] = cached
            return cached

        def record_rejection(reason: str, detail: Mapping[str, Any]) -> None:
            reason_counts[reason] += 1
            if len(rejection_examples[reason]) < 16:
                rejection_examples[reason].append(dict(detail))

        strata = [dict(value) for value in config["strata"]]
        stratum_by_id = {str(value["id"]): value for value in strata}

        for probe_value in config.get("feasibility_probes", []):
            probe = dict(probe_value)
            counters["attempts"] += 1
            base_path = str(probe["base_metadata_path"])
            base = by_path.get(base_path)
            if base is None:
                counters["ineligible_bases"] += 1
                actual_status = "ineligible"
                actual_reason = "ineligible_base"
                record_rejection(actual_reason, {"probe": probe["id"], "base_metadata_path": base_path})
            else:
                session, _ = session_for(base)
                feasibility = session.goal_feasibility(
                    _goal([str(value) for value in probe["goal_family_mod_keys"]]),
                    _start_item(session, dict(probe.get("start") or start_spec)),
                )
                actual_status = feasibility.status
                actual_reason = feasibility.reason
                reason_counts[actual_reason] += 1
                if actual_status != "feasible":
                    counters[actual_status] += 1
                    if actual_reason == "no_natural_t1":
                        counters["zero_weight_families"] += sum(
                            count == 0
                            for count in feasibility.slot_natural_mod_counts
                        )
                    if actual_reason in {"slot_capacity", "group_conflict"}:
                        counters["group_slot_conflicts"] += 1
                    if len(rejection_examples[actual_reason]) < 16:
                        rejection_examples[actual_reason].append(
                            {"probe": probe["id"], "base_metadata_path": base_path}
                        )
            expected_status = str(probe["expected_status"])
            expected_reason = str(probe["expected_reason"])
            if (actual_status, actual_reason) != (expected_status, expected_reason):
                raise RuntimeError(
                    f"feasibility probe {probe['id']} expected "
                    f"{expected_status}/{expected_reason}, got "
                    f"{actual_status}/{actual_reason}"
                )
            probe_results.append(
                {
                    "id": probe["id"],
                    "status": actual_status,
                    "reason": actual_reason,
                }
            )

        def attempt_case(
            *,
            base: BaseInfo,
            requested_keys: list[str],
            stratum: Mapping[str, Any],
            explicit_id: str | None = None,
        ) -> bool:
            if time.monotonic() - started >= watchdog_seconds:
                raise TimeoutError("natural-T1 generator watchdog expired")
            counters["attempts"] += 1
            session, _ = session_for(base)
            requested_signature = json.dumps(
                [base.metadata_path, sorted(requested_keys)], separators=(",", ":")
            )
            if requested_signature in attempt_signatures:
                counters["duplicates"] += 1
                return False
            attempt_signatures.add(requested_signature)
            feasibility = session.goal_feasibility(
                _goal(requested_keys),
                _start_item(session, start_spec),
            )
            if feasibility.status != "feasible":
                counters[feasibility.status] += 1
                if feasibility.reason == "no_natural_t1":
                    counters["zero_weight_families"] += sum(
                        count == 0 for count in feasibility.slot_natural_mod_counts
                    )
                if feasibility.reason in {"slot_capacity", "group_conflict"}:
                    counters["group_slot_conflicts"] += 1
                record_rejection(
                    feasibility.reason,
                    {"base_metadata_path": base.metadata_path, "goal_keys": requested_keys},
                )
                return False
            reason_counts[feasibility.reason] += 1

            resolved = _resolved_goals(session, feasibility)
            resolved_keys = [entry["family_mod_key"] for entry in resolved]
            signature = json.dumps(
                [base.metadata_path, sorted(resolved_keys)], separators=(",", ":")
            )
            if signature in case_signatures:
                counters["duplicates"] += 1
                return False
            minimum_probability = min(
                entry["single_draw_probability"] for entry in resolved
            )
            maximum_probability = max(
                entry["single_draw_probability"] for entry in resolved
            )
            min_probability = stratum.get("min_spawn_probability")
            max_probability = stratum.get("max_spawn_probability")
            min_density = stratum.get("min_pool_mod_count")
            max_density = stratum.get("max_pool_mod_count")
            if min_probability is not None and minimum_probability < float(min_probability):
                counters["spawn_probability_rejections"] += 1
                return False
            if max_probability is not None and minimum_probability > float(max_probability):
                counters["spawn_probability_rejections"] += 1
                return False
            if min_density is not None and feasibility.natural_pool_mod_count < int(min_density):
                counters["pool_density_rejections"] += 1
                return False
            if max_density is not None and feasibility.natural_pool_mod_count > int(max_density):
                counters["pool_density_rejections"] += 1
                return False

            case_signatures.add(signature)
            case_hash = hashlib.sha256(signature.encode("utf-8")).hexdigest()[:12]
            case_id = explicit_id or f"natural-t1-{stratum['id']}-{case_hash}"
            prefix_count = sum(entry["side"] == "prefix" for entry in resolved)
            suffix_count = len(resolved) - prefix_count
            goal = _goal(resolved_keys)
            case = {
                "schema_version": CASE_SCHEMA,
                "id": case_id,
                "category": "seeded_natural_t1",
                "approval_status": "generated_b3_2026_07_22",
                "benchmark_enabled": True,
                "comparison_profile": "bounded-policy-natural-t1-v1",
                "corpus": {
                    "tier": stratum["corpus"],
                    "stratum": stratum["id"],
                    "goal_modifier_count": len(resolved),
                    "prefix_goal_count": prefix_count,
                    "suffix_goal_count": suffix_count,
                    "side_mix": "P" * prefix_count + "S" * suffix_count,
                    "natural_pool_mod_count": feasibility.natural_pool_mod_count,
                    "natural_pool_weight": feasibility.natural_pool_weight,
                    "minimum_single_draw_probability": minimum_probability,
                    "maximum_single_draw_probability": maximum_probability,
                    "geometric_mean_single_draw_probability": math.prod(
                        entry["single_draw_probability"] for entry in resolved
                    ) ** (1.0 / len(resolved)),
                },
                "session": {
                    "base_metadata_path": base.metadata_path,
                    "base_name": base.name,
                    "item_class_key": base.item_class_key,
                    "item_level": item_level,
                },
                "start": start_spec,
                "goal": goal,
                "resolved_natural_t1_goals": resolved,
                "product_action_envelope": {
                    "mode": "calculator_goal_relevant_priced_v1",
                    "envelope_goal": {
                        **goal,
                        "fossil_mode": "goal_relevant",
                        "requested_fossil_actions": [],
                    },
                    "pricing_filter": "all_declared_cost_keys_present",
                    "bench_goal_slots_forbidden": True,
                },
                "allowed_mechanic_families": ["calculator_goal_relevant_product_envelope"],
                "economy": economy,
                "caps": dict(stratum.get("resource_caps") or caps),
                "watchdog_seconds": float(stratum.get("case_watchdog_seconds", config.get("case_watchdog_seconds", 60))),
                "verification": {
                    "runs": 10000,
                    "seed": seed + len(cases) + 1,
                    "max_actions_per_run": 100000,
                },
                "feasibility": _feasibility_dict(feasibility),
                "generation": {
                    "generator_version": GENERATOR_VERSION,
                    "generator_seed": seed,
                    "engine_commit": generation["engine_commit"],
                    "engine_binary_sha256": generation["engine_binary_sha256"],
                    "artifact_manifest_sha256": artifact["manifest_sha256"],
                },
            }
            cases.append(case)
            accepted_by_stratum[str(stratum["id"])] += 1
            counters["accepts"] += 1
            return True

        for explicit in config.get("explicit_cases", []):
            explicit = dict(explicit)
            stratum = stratum_by_id[str(explicit["stratum"])]
            base_path = str(explicit["base_metadata_path"])
            base = by_path.get(base_path)
            if base is None:
                counters["ineligible_bases"] += 1
                record_rejection("explicit_ineligible_base", {"base_metadata_path": base_path})
                continue
            attempt_case(
                base=base,
                requested_keys=[str(value) for value in explicit["goal_family_mod_keys"]],
                stratum=stratum,
                explicit_id=str(explicit["id"]),
            )

        for stratum in strata:
            stratum_id = str(stratum["id"])
            target = int(stratum["cases"])
            max_attempts = int(stratum.get("max_attempts", max(100, target * 100)))
            base_pool = [
                by_path[path]
                for path in stratum.get("base_paths", by_path.keys())
                if path in by_path
            ]
            if not base_pool:
                raise ValueError(f"stratum {stratum_id} has no eligible bases")
            goal_count = int(stratum["goal_count"])
            compositions = [
                _side_code(str(value), goal_count)
                for value in stratum["side_compositions"]
            ]
            attempts_before = counters["attempts"]
            while accepted_by_stratum[stratum_id] < target:
                if counters["attempts"] - attempts_before >= max_attempts:
                    exhausted.append(stratum_id)
                    break
                base = rng.choice(base_pool)
                session, families = session_for(base)
                composition = rng.choice(compositions)
                prefix_count = composition.count("P")
                suffix_count = composition.count("S")
                if len(families["prefix"]) < prefix_count or len(families["suffix"]) < suffix_count:
                    counters["ineligible_bases"] += 1
                    continue
                chosen = rng.sample(families["prefix"], prefix_count) + rng.sample(
                    families["suffix"], suffix_count
                )
                rng.shuffle(chosen)
                attempt_case(
                    base=base,
                    requested_keys=[info.key for info in chosen],
                    stratum=stratum,
                )

        for session, _ in session_cache.values():
            session.close()
    finally:
        data.close()

    counters["exhausted_strata"] = len(exhausted)
    if exhausted:
        raise RuntimeError(f"generator exhausted strata: {', '.join(exhausted)}")

    cases.sort(key=lambda case: case["id"])
    output_dir.mkdir(parents=True, exist_ok=True)
    cases_dir = output_dir / "cases"
    cases_dir.mkdir(parents=True, exist_ok=True)
    for old_case in cases_dir.glob("*.json"):
        old_case.unlink()
    relative_cases: list[str] = []
    for case in cases:
        path = cases_dir / f"{case['id']}.json"
        path.write_bytes(_json_bytes(case))
        relative_cases.append(path.relative_to(output_dir).as_posix())

    manifest = {
        "schema_version": MANIFEST_SCHEMA,
        "corpus_id": str(config["corpus_id"]),
        "generator": generation,
        "artifact": artifact,
        "economy": economy,
        "configuration": {
            "config_path": _relative(root, config_path),
            "config_sha256": _sha256(config_path),
            "item_level": item_level,
            "start": start_spec,
            "natural_filters": filters,
            "watchdog_seconds": watchdog_seconds,
        },
        "case_counts": dict(sorted(Counter(case["corpus"]["tier"] for case in cases).items())),
        "stratum_counts": dict(sorted(accepted_by_stratum.items())),
        "cases": relative_cases,
    }
    report = {
        "schema_version": REPORT_SCHEMA,
        "corpus_id": str(config["corpus_id"]),
        "generator": generation,
        "artifact": artifact,
        "counters": dict(sorted(counters.items())),
        "reason_counts": dict(sorted(reason_counts.items())),
        "rejection_examples": dict(sorted(rejection_examples.items())),
        "feasibility_probes": probe_results,
        "stratum_targets": {stratum["id"]: stratum["cases"] for stratum in strata},
        "stratum_accepts": dict(sorted(accepted_by_stratum.items())),
        "exhausted_strata": exhausted,
    }
    (output_dir / "manifest.json").write_bytes(_json_bytes(manifest))
    (output_dir / "generation-report.json").write_bytes(_json_bytes(report))
    return {"manifest": manifest, "report": report, "cases": cases}
