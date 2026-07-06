from __future__ import annotations

import argparse
import json
import os
import statistics
import time
from pathlib import Path

from poecraft_engine import SimulationOptions, load_data


BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"
ITEM_LEVEL = 86


def elapsed_ms(start: float) -> float:
    return (time.perf_counter() - start) * 1000.0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Measure the current native poecraft engine."
    )
    parser.add_argument(
        "--artifact",
        type=Path,
        default=Path("data/compiled/current"),
    )
    parser.add_argument("--runs", type=int, default=100_000)
    parser.add_argument(
        "--action",
        choices=("alteration", "chaos"),
        default="chaos",
    )
    parser.add_argument("--actions-per-run", type=int, default=10)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    started = time.perf_counter()
    data = load_data(args.artifact)
    data_load_ms = elapsed_ms(started)
    try:
        session_samples: list[float] = []
        for _ in range(15):
            started = time.perf_counter()
            session = data.create_session(BASE, ITEM_LEVEL)
            session_samples.append(elapsed_ms(started))
            session.close()

        session = data.create_session(BASE, ITEM_LEVEL)
        try:
            with session.create_action_context(seed=17) as context:
                context.set_performance_timing(True)
                rare = session.create_item(rarity="rare", with_implicits=False)
                context.performance_stats(reset=True)
                context.debug_pool(rare, {"type": "chaos"})
                context.debug_pool(rare, {"type": "chaos"})
                pool_stats = context.performance_stats()

            profile_count = min(args.runs, 2_000)
            profile_items = [
                session.create_item(rarity="normal", with_implicits=False)
                for _ in range(profile_count)
            ]
            with session.create_action_context(seed=21) as context:
                context.set_performance_timing(True)
                context.performance_stats(reset=True)
                context.apply_batch(profile_items, {"type": "alchemy"})
                profile_stats = context.performance_stats()

            action_count = min(args.runs, 25_000)
            items = [
                session.create_item(rarity="normal", with_implicits=False)
                for _ in range(action_count)
            ]
            with session.create_action_context(seed=23) as context:
                context.performance_stats(reset=True)
                started = time.perf_counter()
                result = context.apply_batch(items, {"type": "alchemy"})
                action_ms = elapsed_ms(started)
                action_stats = context.performance_stats()
                if result.summary.applied_count != action_count:
                    raise RuntimeError("native action benchmark did not apply every action")

            actions_per_run = max(1, args.actions_per_run)
            strategy_rarity = "magic" if args.action == "alteration" else "rare"
            action_nodes = [
                {
                    "id": f"{args.action}_{index + 1}",
                    "kind": "operation",
                    "operation": {"type": args.action, "params": {}},
                }
                for index in range(actions_per_run)
            ]
            strategy_json = {
                "version": "v1",
                "name": "Phase 14 throughput benchmark",
                "start_node_id": "start",
                "base_state": {
                    "base_key": BASE,
                    "item_level": ITEM_LEVEL,
                    "rarity": strategy_rarity,
                },
                "nodes": [
                    {"id": "start", "kind": "start"},
                    *action_nodes,
                    {"id": "success", "kind": "terminal", "terminal": "success"},
                ],
                "edges": [
                    {
                        "id": "begin",
                        "from": "start",
                        "to": action_nodes[0]["id"],
                        "priority": 0,
                        "condition": {"type": "always"},
                    },
                    *[
                        {
                            "id": f"step_{index + 1}",
                            "from": node["id"],
                            "to": (
                                action_nodes[index + 1]["id"]
                                if index + 1 < len(action_nodes)
                                else "success"
                            ),
                            "priority": 0,
                            "condition": {"type": "always"},
                        }
                        for index, node in enumerate(action_nodes)
                    ],
                ],
            }
            with session.compile_strategy(strategy_json) as strategy:
                with strategy.create_simulator() as simulator:
                    started = time.perf_counter()
                    result = simulator.run(
                        SimulationOptions(
                            target_runs=args.runs,
                            seed=29,
                            retained_trace_count=0,
                            retained_success_count=0,
                            retained_failure_count=0,
                        ),
                        chunk_size=10_000,
                    )
                    strategy_ms = elapsed_ms(started)
                    if result.summary["completed_runs"] != args.runs:
                        raise RuntimeError("native strategy benchmark was incomplete")

            pool_requests = int(action_stats["pool_requests"])
            report = {
                "schema_version": 1,
                "engine": "native",
                "artifact": str(args.artifact.resolve()),
                "base": BASE,
                "item_level": ITEM_LEVEL,
                "data_load_ms": round(data_load_ms, 3),
                "session_build_ms": {
                    "median": round(statistics.median(session_samples), 3),
                    "min": round(min(session_samples), 3),
                    "max": round(max(session_samples), 3),
                },
                "pool": {
                    "requests": int(pool_stats["pool_requests"]),
                    "cache_hits": int(pool_stats["cache_hits"]),
                    "cache_misses": int(pool_stats["cache_misses"]),
                    "cache_hit_rate": round(float(pool_stats["cache_hit_rate"]), 6),
                    "candidate_build_ns_per_request": round(
                        int(pool_stats["candidate_build_ns"])
                        / max(1, int(pool_stats["pool_requests"])),
                        1,
                    ),
                    "weighted_pool_build_ns_per_miss": round(
                        int(pool_stats["weighted_pool_build_ns"])
                        / max(1, int(pool_stats["cache_misses"])),
                        1,
                    ),
                },
                "actions": {
                    "count": action_count,
                    "elapsed_ms": round(action_ms, 3),
                    "actions_per_second": round(action_count * 1000.0 / action_ms),
                    "cache_hit_rate": round(
                        int(action_stats["cache_hits"]) / max(1, pool_requests), 6
                    ),
                    "candidate_build_ns_per_request": round(
                        int(profile_stats["candidate_build_ns"])
                        / max(1, int(profile_stats["pool_requests"])),
                        1,
                    ),
                    "weighted_pool_build_ns_per_miss": round(
                        int(profile_stats["weighted_pool_build_ns"])
                        / max(1, int(profile_stats["cache_misses"])),
                        1,
                    ),
                    "sampling_ns_per_call": round(
                        int(profile_stats["sampling_ns"])
                        / max(1, int(profile_stats["sampling_calls"])),
                        1,
                    ),
                    "profiled_action_count": profile_count,
                },
                "strategy": {
                    "runs": args.runs,
                    "elapsed_ms": round(strategy_ms, 3),
                    "runs_per_second": round(args.runs * 1000.0 / strategy_ms),
                    "actions_per_second": round(
                        result.summary["total_actions"] * 1000.0 / strategy_ms
                    ),
                    "actions_per_run": actions_per_run,
                    "action_type": args.action,
                    "total_actions": result.summary["total_actions"],
                },
                "process_id": os.getpid(),
            }
        finally:
            session.close()
    finally:
        data.close()

    payload = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    print(payload, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
