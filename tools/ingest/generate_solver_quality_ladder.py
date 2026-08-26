"""Generate the current Calculator-profile solver quality ladder fixtures."""

from __future__ import annotations

import copy
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "fixtures" / "solver-quality-ladder" / "v1"
PROFILE = "calculator-product-quality-ladder-v1"
QUALIFICATION_PROFILE = "calculator-product-quality-ladder-qualification1024-v1"
ALLFLAME_SHA = "de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0"

ECONOMY = {
    "version": "v1",
    "id": f"economy:allflame:{ALLFLAME_SHA}",
    "snapshot_path": f"apps/web/public/economy/snapshots/{ALLFLAME_SHA}.json",
    "content_sha256": ALLFLAME_SHA,
    "source_cutoff_at_utc": "2026-08-09T18:34:48Z",
    "league_key": "allflame",
    "league_name": "Allflame",
    "manual_overrides": {"base": 1.0},
    "fallback_price": None,
    "missing_price_decisions": {
        "base": "quality_ladder_fixture_input_1_chaos"
    },
}

COMMON_CAPS = {
    "max_states": 200000,
    "max_sweeps": 100000,
    # The Calculator does not supply chunkSize. The worker therefore starts at
    # four and adapts only up to its default requested maximum of eight.
    "solve_step_work_items": 8,
    "max_discovered_states": 200000,
    "max_expanded_states": 200000,
    "max_state_action_rows": 1215000,
    "max_transitions": 10000000,
    "max_reforge_work": 50000000,
    "max_solver_owned_bytes": 1073741824,
    "max_diagnostic_samples": 64,
    "max_telemetry_json_bytes": 2097152,
    "max_policy_refinement_states": 200000,
    "max_compiled_nodes": 100000,
    "max_compiled_edges": 400000,
    "max_strategy_json_bytes": 67108864,
    "worker_step_ms": 250,
    "cancel_ack_ms": 250,
    "goal_progress_gated_reforges": True,
    "allow_economic_restart": False,
    "high_impact_executable_uppers": True,
    "consider_imprint_programs": False,
    "full_evidence": True,
}

VERIFICATION = {
    "runs": 0,
    "seed": 20260826,
    "max_actions_per_run": 100000,
    "max_graph_steps_per_run": 4000000,
    "exact_evaluation": True,
    "exact_max_states": 1000000,
    "exact_max_pairs": 5000000,
    "exact_max_transitions": 20000000,
    "exact_max_owned_bytes": 1073741824,
    "exact_cost_absolute_tolerance": 1e-7,
    "exact_cost_relative_tolerance": 1e-9,
}

CONQUEST_SESSION = {
    "base_metadata_path": "Metadata/Items/Armours/BodyArmours/BodyStrDex20",
    "base_name": "Conquest Lamellar",
    "item_class_key": "Body Armour",
    "item_level": 86,
    "minimum_item_level": 86,
}
CONQUEST_SLOTS = [
    "LocalIncreasedArmourAndEvasion8",
    "LocalIncreasedArmourAndEvasionAndStunRecovery6",
    "LocalBaseArmourAndEvasionRating8",
    "ChanceToSuppressSpellsHigh5___",
    "AdditionalPhysicalDamageReduction5_",
]

BOW_SESSION = {
    "base_metadata_path": "Metadata/Items/Weapons/TwoHandWeapons/Bows/Bow20",
    "base_name": "Spine Bow",
    "item_class_key": "Bow",
    "item_level": 86,
    "minimum_item_level": 86,
}
BOW_SLOTS = [
    "LocalIncreaseSocketedGemLevel1",
    "LocalAddedPhysicalDamageTwoHand9",
    "LocalAddedColdDamageTwoHand10",
    "ManaGainedFromEnemyDeath6",
    "ProjectileSpeed5",
]

RING_SESSION = {
    "base_metadata_path": "Metadata/Items/Rings/Ring10",
    "base_name": "Amethyst Ring",
    "item_class_key": "Ring",
    "item_level": 86,
    "minimum_item_level": 86,
}
RING_SLOTS = [
    "IncreasedEvasionRating7",
    "FireResist8",
    "AddedColdDamage9",
    "AllAttributes4",
]


def goal(slots: list[str]) -> dict[str, object]:
    rows = [
        {"family_mod_key": family, "min_tier": 1}
        for family in slots
    ]
    return {
        "version": "v1",
        "rarity": "rare",
        "action_mode": "goal_relevant",
        "min_satisfied_slots": len(rows),
        "slots": rows,
    }


def start(mods: list[tuple[str, list[str]]] | None = None) -> dict[str, object]:
    return {
        "rarity": "rare",
        "with_implicits": False,
        "mods": [
            {"key": key, "flags": flags}
            for key, flags in (mods or [])
        ],
    }


def write_json(path: Path, value: object) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(value, indent=2, sort_keys=False) + "\n")


def case(
    *,
    case_id: str,
    role: str,
    session: dict[str, object],
    goal_slots: list[str],
    start_mods: list[tuple[str, list[str]]] | None,
    bounded_finish_seconds: int,
    tier: str,
) -> dict[str, object]:
    requested_goal = goal(goal_slots)
    return {
        "schema_version": "solver_benchmark_case_v1",
        "id": case_id,
        "category": "solver_quality_ladder",
        "approval_status": "active_gate0_2026_08_26",
        "benchmark_enabled": True,
        "description": role,
        "comparison_profile": PROFILE,
        "watchdog_seconds": bounded_finish_seconds + 30,
        "requested_bounded_finish_seconds": bounded_finish_seconds,
        "session": copy.deepcopy(session),
        "start": start(start_mods),
        "goal": requested_goal,
        "product_action_envelope": {
            "mode": "calculator_goal_relevant_priced_v1",
            "envelope_goal": {
                **copy.deepcopy(requested_goal),
                "fossil_mode": "goal_relevant",
                "requested_fossil_actions": [],
            },
            "pricing_filter": "all_declared_cost_keys_present",
            "bench_goal_slots_forbidden": True,
        },
        "allowed_mechanic_families": [
            "calculator_goal_relevant_product_envelope"
        ],
        "economy": copy.deepcopy(ECONOMY),
        "expected": {
            "solve_status": "reliability_classified",
            "optimality_status": "classified",
            "compile_status": "compiled",
            "verification_status": "not_required",
        },
        "bounded_best_policy_contract": {
            "allow_exact": True,
            "bounded_requires_named_stop": True,
            "bounded_requires_strict_gap": True,
            "bounded_requires_open_obligations": True,
            "require_evaluated_upper": True,
            "require_cheapest_independently_evaluated_incumbent": True,
        },
        "caps": copy.deepcopy(COMMON_CAPS),
        "verification": copy.deepcopy(VERIFICATION),
        "corpus": {
            "tier": tier,
            "stratum": role,
            "goal_modifier_count": len(goal_slots),
            "start_goal_modifier_count": len(start_mods or []),
            "product_worker_profile": "calculator_default_adaptive_max_8",
        },
    }


def build_cases() -> list[dict[str, object]]:
    cases: list[dict[str, object]] = []
    finish_by_count = {1: 15, 2: 20, 3: 30, 4: 60, 5: 60}
    tier_by_count = {
        1: "clean-small",
        2: "clean-small",
        3: "clean-three",
        4: "clean-four",
        5: "clean-five",
    }
    for stem, session, slots in (
        ("conquest-lamellar", CONQUEST_SESSION, CONQUEST_SLOTS),
        ("spine-bow", BOW_SESSION, BOW_SLOTS),
    ):
        for count in range(1, 6):
            cases.append(case(
                case_id=f"{stem}-allflame-clean-{count}-goal-product8",
                role=f"{stem}_clean_zero_to_{count}",
                session=session,
                goal_slots=slots[:count],
                start_mods=None,
                bounded_finish_seconds=finish_by_count[count],
                tier=tier_by_count[count],
            ))

    for count in range(1, 5):
        cases.append(case(
            case_id=f"amethyst-ring-allflame-clean-{count}-goal-product8",
            role=f"amethyst_ring_clean_zero_to_{count}",
            session=RING_SESSION,
            goal_slots=RING_SLOTS[:count],
            start_mods=None,
            bounded_finish_seconds=finish_by_count[count],
            tier="diversity-" + tier_by_count[count],
        ))

    three_prefixes = [(family, []) for family in CONQUEST_SLOTS[:3]]
    four_goals = three_prefixes + [(CONQUEST_SLOTS[3], [])]
    fractured_four = three_prefixes + [(CONQUEST_SLOTS[3], ["fractured"])]
    cases.extend([
        case(
            case_id="conquest-lamellar-allflame-partial-3-to-4-product8",
            role="conquest_lamellar_partial_three_to_four",
            session=CONQUEST_SESSION,
            goal_slots=CONQUEST_SLOTS[:4],
            start_mods=three_prefixes,
            bounded_finish_seconds=30,
            tier="partial",
        ),
        case(
            case_id="conquest-lamellar-allflame-partial-3-to-5-product8",
            role="conquest_lamellar_partial_three_to_five",
            session=CONQUEST_SESSION,
            goal_slots=CONQUEST_SLOTS,
            start_mods=three_prefixes,
            bounded_finish_seconds=45,
            tier="partial",
        ),
        case(
            case_id="conquest-lamellar-allflame-partial-4-to-5-product8",
            role="conquest_lamellar_partial_four_to_five",
            session=CONQUEST_SESSION,
            goal_slots=CONQUEST_SLOTS,
            start_mods=four_goals,
            bounded_finish_seconds=45,
            tier="partial",
        ),
        case(
            case_id="conquest-lamellar-allflame-fractured-4-to-5-product8",
            role="conquest_lamellar_fractured_four_to_five_owner",
            session=CONQUEST_SESSION,
            goal_slots=CONQUEST_SLOTS,
            start_mods=fractured_four,
            bounded_finish_seconds=30,
            tier="partial-owner",
        ),
    ])
    return cases


def main() -> None:
    cases = build_cases()
    case_directory = OUTPUT / "cases"
    case_directory.mkdir(parents=True, exist_ok=True)
    for specification in cases:
        path = case_directory / f"{specification['id']}.json"
        write_json(path, specification)

    roles = {
        str(specification["id"]): specification["corpus"]["stratum"]
        for specification in cases
    }
    manifest = {
        "schema_version": "solver_benchmark_corpus_v1",
        "corpus_id": "poecraft2-solver-quality-ladder-v1",
        "artifact": {
            "manifest_relative_path": "data/compiled/current/manifest.json",
            "canonical_source": "data/sqlite/poecraft.db",
            "engine_abi_version": 2,
            "artifact_schema_version": 4,
            "source_version": "repoe-e4eaf06c20e1ddb4",
            "source_data_hash": "76375e02fc21b0bc0d5709ab589aede8b1967b9a2d53b25aaf517a206f592000",
            "game_data_sha256": "af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5",
            "strings_sha256": "ba2110894e94b533d42e0440b83fab468d848e438ff5e6d6ed976108ac0d507f",
        },
        "source_checkpoint": {
            "engine_product_commit": "ec3fbd3",
            "plan_commit": "b5f3cfb",
            "mechanics_authority": "unchanged_current_implemented_contract",
        },
        "benchmark_identity_contract": {
            "id": PROFILE,
            "trajectory_required": True,
            "native_repetitions": 1,
            "fixed_work_identity_required": False,
            "compiled_exact_evaluation_required": True,
            "general_product_scope": {
                "action_mode": "goal_relevant",
                "solve_step_work_items": 8,
                "goal_progress_gated_reforges": True,
                "allow_economic_restart": False,
                "consider_imprint_programs": False,
            },
            "explicit_imprint_scope": {
                "consider_imprint_programs": False,
                "disposition": "not_present_in_quality_ladder",
            },
            "required_disclosures": [
                "artifact",
                "source_checkpoint",
                "economy",
                "action_scope",
                "terminal_goal",
                "start_carrier",
                "work_step",
                "requested_finish",
                "verification",
            ],
        },
        "case_roles": roles,
        "comparison_profile": {
            "id": PROFILE,
            "product_scope": "calculator_goal_relevant_exact_terminal_imprint_off_restart_off_product8",
            "exact_required": False,
            "maximum_wall_seconds": 120,
            "compiled_exact_evaluation_required": True,
            "verification_runs": 0,
        },
        "owner_decisions": [
            "The ladder reproduces the actual Calculator worker's default adaptive path capped at eight work items; 1024-item qualification runs are separate controls.",
            "Every case uses exact terminal semantics, generated Imprint programs off, no voluntary economic Restart, and the Allflame price snapshot with an explicit one-Chaos base input.",
            "Goal-count ladders are nested observations, not a fixed-order restriction on solver policy.",
            "Gate 0 skips sampled verification but requires independent exact evaluation whenever a finite strategy is published.",
        ],
        "cases": [f"cases/{specification['id']}.json" for specification in cases],
    }
    write_json(OUTPUT / "manifest.json", manifest)

    qualification_cases = [
        copy.deepcopy(specification)
        for specification in cases
        if specification["corpus"]["goal_modifier_count"] >= 4
        or specification["corpus"]["tier"].startswith("partial")
    ]
    qualification_output = OUTPUT / "qualification-1024"
    qualification_case_directory = qualification_output / "cases"
    qualification_case_directory.mkdir(parents=True, exist_ok=True)
    for specification in qualification_cases:
        specification["comparison_profile"] = QUALIFICATION_PROFILE
        specification["caps"]["solve_step_work_items"] = 1024
        specification["caps"]["worker_step_ms"] = 20000
        specification["corpus"]["product_worker_profile"] = (
            "qualification_fixed_1024"
        )
        path = qualification_case_directory / f"{specification['id']}.json"
        write_json(path, specification)

    qualification_manifest = copy.deepcopy(manifest)
    qualification_manifest["corpus_id"] = (
        "poecraft2-solver-quality-ladder-qualification1024-v1"
    )
    qualification_manifest["benchmark_identity_contract"]["id"] = (
        QUALIFICATION_PROFILE
    )
    qualification_manifest["benchmark_identity_contract"][
        "general_product_scope"
    ]["solve_step_work_items"] = 1024
    qualification_manifest["comparison_profile"]["id"] = (
        QUALIFICATION_PROFILE
    )
    qualification_manifest["comparison_profile"]["product_scope"] = (
        "calculator_goal_relevant_exact_terminal_imprint_off_restart_off_qualification1024"
    )
    qualification_manifest["case_roles"] = {
        str(specification["id"]): specification["corpus"]["stratum"]
        for specification in qualification_cases
    }
    qualification_manifest["owner_decisions"] = [
        "This corpus changes only solve-step work from the Calculator's adaptive maximum of eight to the retained 1024-item native qualification path.",
        "It is an intentional scheduling treatment, not a claim about browser responsiveness or product identity.",
    ]
    qualification_manifest["cases"] = [
        f"cases/{specification['id']}.json"
        for specification in qualification_cases
    ]
    write_json(qualification_output / "manifest.json", qualification_manifest)
    print(
        f"Generated {len(cases)} product cases and "
        f"{len(qualification_cases)} qualification controls in "
        f"{OUTPUT.relative_to(ROOT)}"
    )


if __name__ == "__main__":
    main()
