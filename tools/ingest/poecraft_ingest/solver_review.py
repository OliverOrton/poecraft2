"""Deterministic, display-only review projections for ordinary strategies.

The raw ordinary strategy is always the sole execution authority.  This module
reads and verifies that document, derives descriptive sections and entries, and
emits a separate document conforming to the S8.0 review-projection contract.
It deliberately has no solver, compiler, simulator, legality, or evaluation
integration.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import gzip
import hashlib
import heapq
import json
from pathlib import Path
import re
from typing import Any, Iterable, Mapping, Sequence


SCHEMA_VERSION = "solver_review_projection_v1"
REPRESENTATIVE_CASES: tuple[tuple[str, str], ...] = (
    (
        "oracle-real-one-mod",
        "Small exact oracle policy with a single rolling retry SCC.",
    ),
    (
        "ordinary-es-bench",
        "Ordinary Alchemy/Scour rolling, deterministic goal bench finishing, and recovery actions.",
    ),
    (
        "s8-fracture-prepare",
        "Existing exact fracture_prepare expansion with preparation, Fracture routing, and retry behavior.",
    ),
    (
        "endgame-fractured-es",
        "Archived fractured starting carrier with Fossil rolling, deterministic bench finishing, and recovery actions.",
    ),
)

_ROLE_ORDER = {
    role: index
    for index, role in enumerate(
        (
            "setup",
            "rolling",
            "temporary_blocking",
            "protection",
            "fracture",
            "finishing",
            "cleanup",
            "recovery",
            "restart",
        )
    )
}

_ROLLING_OPERATIONS = {
    "alchemy",
    "alteration",
    "chaos",
    "essence",
    "fossil",
    "harvest_reforge",
    "transmute",
}
_SETUP_OPERATIONS = {
    "augment",
    "bestiary:imprint",
    "exalt",
    "influence_exalt",
    "regal",
    "veiled_exalt",
}
_RECOVERY_OPERATIONS = {"annul", "bestiary:restore_imprint", "restore_imprint"}
_CLEANUP_OPERATIONS = {"remove_crafted_mod"}
_RESTART_OPERATIONS = {"restart", "scour"}
_PROTECTION_FLAGS = {"prefixes_locked", "suffixes_locked"}
_BLOCKING_FLAGS = {"no_attack", "no_caster"}


class ReviewProjectionError(ValueError):
    """A projection or its raw strategy identity is invalid or stale."""


def _sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _natural_key(value: str) -> tuple[tuple[int, object], ...]:
    return tuple(
        (0, int(part)) if part.isdigit() else (1, part)
        for part in re.split(r"(\d+)", value)
        if part
    )


def _slug(value: str) -> str:
    result = re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")
    return result or "entry"


def _repo_relative(root: Path, path: Path) -> str:
    root = root.resolve()
    path = path.resolve()
    try:
        return path.relative_to(root).as_posix()
    except ValueError as exc:
        raise ReviewProjectionError(
            f"raw strategy path is outside repository root: {path}"
        ) from exc


def _resolve_inside(root: Path, relative: str) -> Path:
    root = root.resolve()
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ReviewProjectionError(
            f"raw strategy path escapes repository root: {relative}"
        ) from exc
    return candidate


def _load_strategy(path: Path) -> tuple[bytes, bytes, dict[str, Any]]:
    try:
        source_bytes = path.read_bytes()
    except OSError as exc:
        raise ReviewProjectionError(f"cannot read raw strategy: {path}") from exc
    try:
        json_bytes = gzip.decompress(source_bytes) if path.suffix == ".gz" else source_bytes
    except (OSError, EOFError) as exc:
        raise ReviewProjectionError(f"cannot decompress raw strategy: {path}") from exc
    try:
        strategy = json.loads(json_bytes)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ReviewProjectionError(f"raw strategy is not valid JSON: {path}") from exc
    if not isinstance(strategy, dict):
        raise ReviewProjectionError("raw strategy root must be an object")
    return source_bytes, json_bytes, strategy


def _strategy_indexes(
    strategy: Mapping[str, Any],
) -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    nodes: dict[str, dict[str, Any]] = {}
    for node in strategy.get("nodes", []):
        if not isinstance(node, dict) or not isinstance(node.get("id"), str):
            raise ReviewProjectionError("every raw strategy node requires a string id")
        node_id = node["id"]
        if node_id in nodes:
            raise ReviewProjectionError(f"duplicate raw node id: {node_id}")
        nodes[node_id] = node

    edges: dict[str, dict[str, Any]] = {}
    for edge in strategy.get("edges", []):
        if not isinstance(edge, dict) or not isinstance(edge.get("id"), str):
            raise ReviewProjectionError("every raw strategy edge requires a string id")
        edge_id = edge["id"]
        if edge_id in edges:
            raise ReviewProjectionError(f"duplicate raw edge id: {edge_id}")
        if edge.get("from") not in nodes or edge.get("to") not in nodes:
            raise ReviewProjectionError(
                f"raw edge {edge_id!r} has an unresolved endpoint"
            )
        edges[edge_id] = edge

    if not nodes:
        raise ReviewProjectionError("raw strategy requires at least one node")
    if strategy.get("start_node_id") not in nodes:
        raise ReviewProjectionError("raw strategy start_node_id does not resolve")
    return nodes, edges


def _condition_leaves(
    condition: Any, *, positive: bool = True
) -> Iterable[tuple[dict[str, Any], bool]]:
    """Yield facts that are conjunctively required by a compiled condition.

    Compiler-produced policy routes use nested ``all`` and single-child
    ``not`` conditions.  Unknown disjunctions are ignored conservatively, so a
    label is never based on a fact that is true in only one alternative.
    """

    if not isinstance(condition, dict):
        return
    kind = condition.get("type")
    children = condition.get("conditions")
    if kind == "all" and isinstance(children, list):
        for child in children:
            yield from _condition_leaves(child, positive=positive)
        return
    if kind == "not" and isinstance(children, list):
        for child in children:
            yield from _condition_leaves(child, positive=not positive)
        return
    if kind in {"any", "one_of"} and isinstance(children, list):
        if len(children) == 1:
            yield from _condition_leaves(children[0], positive=positive)
        return
    yield condition, positive


def _goal_requirements(
    nodes: Mapping[str, Mapping[str, Any]],
    edges: Mapping[str, Mapping[str, Any]],
) -> tuple[dict[str, int], list[Mapping[str, Any]]]:
    success_ids = {
        node_id
        for node_id, node in nodes.items()
        if node.get("kind") == "terminal" and node.get("terminal") == "success"
    }
    success_routes = [edge for edge in edges.values() if edge.get("to") in success_ids]
    route_requirements: list[dict[str, int]] = []
    for edge in success_routes:
        requirements: dict[str, int] = {}
        for fact, positive in _condition_leaves(edge.get("condition")):
            if not positive or fact.get("type") != "has_mod_family":
                continue
            family = fact.get("family_mod_key")
            if isinstance(family, str):
                requirements[family] = max(
                    requirements.get(family, 0), int(fact.get("min_tier", 0))
                )
        route_requirements.append(requirements)

    if not route_requirements:
        return {}, success_routes
    common = set(route_requirements[0])
    for requirements in route_requirements[1:]:
        common.intersection_update(requirements)
    return {
        family: min(requirements[family] for requirements in route_requirements)
        for family in sorted(common, key=_natural_key)
    }, success_routes


def _base_goal_facts(
    strategy: Mapping[str, Any], goal_requirements: Mapping[str, int]
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    base = strategy.get("base_state")
    if not isinstance(base, dict):
        return result
    for side_key, side in (("prefixes", "prefix"), ("suffixes", "suffix")):
        for affix in base.get(side_key, []):
            if not isinstance(affix, dict):
                continue
            key = affix.get("mod_key")
            if key in goal_requirements:
                result[key] = {
                    "side": side,
                    "crafted": bool(affix.get("crafted", False)),
                    "fractured": bool(affix.get("fractured", False)),
                }
    return result


def _route_facts(
    condition: Any, goal_requirements: Mapping[str, int]
) -> dict[str, set[str]]:
    result = {
        "goals": set(),
        "crafted_goals": set(),
        "fractured_goals": set(),
        "flags": set(),
        "crafted_non_goals": set(),
    }
    for fact, positive in _condition_leaves(condition):
        if not positive:
            continue
        kind = fact.get("type")
        if kind == "item_flag" and isinstance(fact.get("flag"), str):
            result["flags"].add(fact["flag"])
            continue
        if kind == "has_mod_family":
            family = fact.get("family_mod_key")
            if not isinstance(family, str):
                continue
            required = goal_requirements.get(family)
            if required is not None:
                if int(fact.get("min_tier", 0)) >= required:
                    result["goals"].add(family)
                if fact.get("crafted") is True:
                    result["crafted_goals"].add(family)
                if fact.get("fractured") is True:
                    result["fractured_goals"].add(family)
            elif fact.get("crafted") is True:
                result["crafted_non_goals"].add(family)
            continue
        if kind == "mod_family_count" and fact.get("crafted") is True:
            if int(fact.get("min", 0)) <= 0:
                continue
            for family in fact.get("family_mod_keys", []):
                if isinstance(family, str) and family not in goal_requirements:
                    result["crafted_non_goals"].add(family)
    result["crafted_goals"].intersection_update(result["goals"])
    result["fractured_goals"].intersection_update(result["goals"])
    return result


def _combined_route_facts(
    incoming: Sequence[Mapping[str, Any]], goal_requirements: Mapping[str, int]
) -> dict[str, tuple[str, ...]]:
    routed = [
        _route_facts(edge.get("condition"), goal_requirements)
        for edge in incoming
        if isinstance(edge.get("condition"), dict)
    ]
    names = (
        "goals",
        "crafted_goals",
        "fractured_goals",
        "flags",
        "crafted_non_goals",
    )
    if not routed:
        return {
            **{f"common_{name}": () for name in names},
            **{f"possible_{name}": () for name in names},
        }
    result: dict[str, tuple[str, ...]] = {}
    for name in names:
        common = set(routed[0][name])
        possible: set[str] = set()
        for facts in routed:
            common.intersection_update(facts[name])
            possible.update(facts[name])
        result[f"common_{name}"] = tuple(sorted(common, key=_natural_key))
        result[f"possible_{name}"] = tuple(sorted(possible, key=_natural_key))
    return result


def _bench_role(mod_key: str, goal_requirements: Mapping[str, int]) -> str:
    if "ItemGenerationCannotChange" in mod_key:
        return "protection"
    if "ItemGenerationCannotRoll" in mod_key:
        return "temporary_blocking"
    if "ItemGenerationCanHaveMultipleCraftedMods" in mod_key:
        return "setup"
    if mod_key in goal_requirements:
        return "finishing"
    return "temporary_blocking"


def _operation_role(
    operation: Mapping[str, Any], goal_requirements: Mapping[str, int]
) -> str:
    kind = str(operation.get("type", ""))
    if kind == "bench":
        return _bench_role(str(operation.get("mod_key", "")), goal_requirements)
    if kind == "fracture":
        return "fracture"
    if kind in _RESTART_OPERATIONS:
        return "restart"
    if kind in _RECOVERY_OPERATIONS:
        return "recovery"
    if kind in _CLEANUP_OPERATIONS:
        return "cleanup"
    if kind in _ROLLING_OPERATIONS:
        return "rolling"
    if kind in _SETUP_OPERATIONS:
        return "setup"
    return "setup"


def _operation_label(operation: Mapping[str, Any]) -> str:
    kind = str(operation.get("type", "operation"))
    if kind == "bench":
        return f"Bench {operation.get('mod_key', '<missing mod key>')}"
    if kind == "fossil":
        fossils = operation.get("fossils", [])
        return "Fossil " + ", ".join(str(value) for value in fossils)
    if kind == "harvest_reforge":
        target = operation.get("target_tag")
        if target is None and isinstance(operation.get("params"), dict):
            target = operation["params"].get("target_tag")
        return f"Harvest reforge {target}" if target else "Harvest reforge"
    return kind.replace("_", " ").title()


def _goal_subset_label(
    goals: Sequence[str], base_goal_facts: Mapping[str, Mapping[str, Any]]
) -> str:
    if not goals:
        return ""
    sides = (
        {base_goal_facts[goal]["side"] for goal in goals}
        if all(goal in base_goal_facts for goal in goals)
        else set()
    )
    side = f"{next(iter(sides))} " if len(sides) == 1 else ""
    return f"{side}goal subset [{', '.join(goals)}]"


def _node_review(
    node: Mapping[str, Any],
    incoming: Sequence[Mapping[str, Any]],
    goal_requirements: Mapping[str, int],
    base_goal_facts: Mapping[str, Mapping[str, Any]],
) -> tuple[str, tuple[Any, ...], str]:
    operation = node.get("operation")
    if not isinstance(operation, dict):
        kind = node.get("kind")
        if kind == "start":
            return "setup", ("start",), "Enter the exact raw policy"
        if kind == "terminal" and node.get("terminal") == "success":
            return "finishing", ("terminal", "success"), "Satisfied goal carrier"
        if kind == "terminal":
            terminal = str(node.get("terminal", "failure"))
            return "recovery", ("terminal", terminal), f"{terminal.title()} policy exit"
        return "rolling", (str(kind),), "Exact state router"

    role = _operation_role(operation, goal_requirements)
    facts = _combined_route_facts(incoming, goal_requirements)
    operation_key = json.dumps(operation, sort_keys=True, separators=(",", ":"))
    target = str(operation.get("mod_key", "")) if operation.get("type") == "bench" else ""
    other_goals = set(goal_requirements) - ({target} if target else set())
    finishing_ready = (
        role == "finishing"
        and target in goal_requirements
        and other_goals.issubset(facts["common_goals"])
        and target not in facts["possible_goals"]
    )
    key = (
        operation_key,
        facts["common_goals"],
        facts["possible_goals"],
        facts["common_crafted_goals"],
        facts["common_fractured_goals"],
        facts["common_flags"],
        facts["common_crafted_non_goals"],
        finishing_ready,
    )

    details: list[str] = []
    if finishing_ready:
        details.append("deterministic finishing ready")
    common_goals = facts["common_goals"]
    possible_goals = facts["possible_goals"]
    if common_goals:
        active_flags = set(facts["common_flags"])
        preserved_goals = set(facts["common_fractured_goals"])
        for goal in common_goals:
            side = base_goal_facts.get(goal, {}).get("side")
            if (
                side == "prefix" and "prefixes_locked" in active_flags
            ) or (
                side == "suffix" and "suffixes_locked" in active_flags
            ):
                preserved_goals.add(goal)
        if preserved_goals:
            details.append(
                "preserves "
                + _goal_subset_label(
                    sorted(preserved_goals, key=_natural_key), base_goal_facts
                )
            )
        remaining_goals = sorted(set(common_goals) - preserved_goals, key=_natural_key)
        if remaining_goals:
            details.append(
                "carrier has " + _goal_subset_label(remaining_goals, base_goal_facts)
            )
    elif not possible_goals and role == "rolling":
        details.append("disposable carrier with no satisfied goal subset on routed states")
    elif possible_goals:
        details.append(
            "routed carrier goal subset varies within ["
            + ", ".join(possible_goals)
            + "]"
        )
    if facts["common_crafted_goals"]:
        details.append("crafted goals [" + ", ".join(facts["common_crafted_goals"]) + "]")
    if facts["common_fractured_goals"]:
        details.append(
            "fractured goals [" + ", ".join(facts["common_fractured_goals"]) + "]"
        )
    active_protection = sorted(
        set(facts["common_flags"]) & _PROTECTION_FLAGS, key=_natural_key
    )
    active_blocking = sorted(
        set(facts["common_flags"]) & _BLOCKING_FLAGS, key=_natural_key
    )
    if active_protection:
        details.append("active protection " + ", ".join(active_protection))
    if active_blocking:
        details.append("active blocking " + ", ".join(active_blocking))
    if facts["common_crafted_non_goals"]:
        details.append(
            "temporary crafted blockers ["
            + ", ".join(facts["common_crafted_non_goals"])
            + "]"
        )
    if role in {"recovery", "restart", "cleanup"}:
        details.append("returns through exact raw routing")

    label = _operation_label(operation)
    if details:
        label += " — " + "; ".join(details)
    return role, key, label


def _strong_components(
    nodes: Mapping[str, Any], edges: Mapping[str, Mapping[str, Any]]
) -> list[list[str]]:
    adjacency = {node_id: [] for node_id in nodes}
    reverse = {node_id: [] for node_id in nodes}
    for edge in edges.values():
        source = edge["from"]
        target = edge["to"]
        adjacency[source].append(target)
        reverse[target].append(source)
    for routes in adjacency.values():
        routes.sort(key=_natural_key)
    for routes in reverse.values():
        routes.sort(key=_natural_key)

    visited: set[str] = set()
    finish_order: list[str] = []
    for root in sorted(nodes, key=_natural_key):
        if root in visited:
            continue
        visited.add(root)
        stack: list[tuple[str, bool]] = [(root, False)]
        while stack:
            node_id, expanded = stack.pop()
            if expanded:
                finish_order.append(node_id)
                continue
            stack.append((node_id, True))
            for target in reversed(adjacency[node_id]):
                if target not in visited:
                    visited.add(target)
                    stack.append((target, False))

    components: list[list[str]] = []
    assigned: set[str] = set()
    for root in reversed(finish_order):
        if root in assigned:
            continue
        component: list[str] = []
        stack = [root]
        assigned.add(root)
        while stack:
            node_id = stack.pop()
            component.append(node_id)
            for target in reversed(reverse[node_id]):
                if target not in assigned:
                    assigned.add(target)
                    stack.append(target)
        components.append(sorted(component, key=_natural_key))
    return components


def _ordered_components(
    strategy: Mapping[str, Any],
    nodes: Mapping[str, Mapping[str, Any]],
    edges: Mapping[str, Mapping[str, Any]],
) -> list[list[str]]:
    components = _strong_components(nodes, edges)
    component_by_node = {
        node_id: component_index
        for component_index, component in enumerate(components)
        for node_id in component
    }
    outgoing: dict[int, set[int]] = defaultdict(set)
    indegree = [0] * len(components)
    for edge in edges.values():
        source = component_by_node[edge["from"]]
        target = component_by_node[edge["to"]]
        if source != target and target not in outgoing[source]:
            outgoing[source].add(target)
            indegree[target] += 1

    start_component = component_by_node[strategy["start_node_id"]]

    def queue_key(component_index: int) -> tuple[Any, ...]:
        return (
            0 if component_index == start_component else 1,
            _natural_key(components[component_index][0]),
            component_index,
        )

    queue = [queue_key(index) for index, value in enumerate(indegree) if value == 0]
    heapq.heapify(queue)
    ordered: list[list[str]] = []
    while queue:
        _, _, component_index = heapq.heappop(queue)
        ordered.append(components[component_index])
        for target in sorted(outgoing[component_index], key=lambda item: queue_key(item)):
            indegree[target] -= 1
            if indegree[target] == 0:
                heapq.heappush(queue, queue_key(target))
    if len(ordered) != len(components):
        raise ReviewProjectionError("raw strategy SCC condensation was not acyclic")
    return ordered


def _section_role(
    component: Sequence[str],
    nodes: Mapping[str, Mapping[str, Any]],
    goal_requirements: Mapping[str, int],
) -> str:
    node_roles: list[str] = []
    operation_roles: list[str] = []
    for node_id in component:
        node = nodes[node_id]
        operation = node.get("operation")
        if isinstance(operation, dict):
            role = _operation_role(operation, goal_requirements)
            operation_roles.append(role)
            node_roles.append(role)
        elif node.get("kind") == "start":
            node_roles.append("setup")
        elif node.get("kind") == "terminal" and node.get("terminal") == "success":
            node_roles.append("finishing")
        elif node.get("kind") == "terminal":
            node_roles.append("recovery")

    if "fracture" in operation_roles:
        return "fracture"
    if "rolling" in operation_roles:
        return "rolling"
    for role in (
        "protection",
        "temporary_blocking",
        "finishing",
        "cleanup",
        "recovery",
        "restart",
        "setup",
    ):
        if role in node_roles:
            return role
    return "rolling"


def _goal_status_label(
    goal_requirements: Mapping[str, int],
    success_routes: Sequence[Mapping[str, Any]],
    base_goal_facts: Mapping[str, Mapping[str, Any]],
) -> str:
    crafted_required: set[str] = set()
    fractured_required: set[str] = set()
    if success_routes:
        route_crafted: list[set[str]] = []
        route_fractured: list[set[str]] = []
        for edge in success_routes:
            facts = _route_facts(edge.get("condition"), goal_requirements)
            route_crafted.append(facts["crafted_goals"])
            route_fractured.append(facts["fractured_goals"])
        crafted_required = set.intersection(*route_crafted) if route_crafted else set()
        fractured_required = (
            set.intersection(*route_fractured) if route_fractured else set()
        )

    parts = [f"Satisfied goal carriers ({len(goal_requirements)} goal families)"]
    parts.append(
        "crafted required [" + ", ".join(sorted(crafted_required, key=_natural_key)) + "]"
        if crafted_required
        else "crafted status unconstrained by the success route"
    )
    parts.append(
        "fractured required ["
        + ", ".join(sorted(fractured_required, key=_natural_key))
        + "]"
        if fractured_required
        else "fractured status unconstrained by the success route"
    )
    base_fractured = [
        f"{facts['side']} {family}"
        for family, facts in sorted(base_goal_facts.items(), key=lambda item: _natural_key(item[0]))
        if facts["fractured"]
    ]
    if base_fractured:
        parts.append("base carrier pins fractured " + ", ".join(base_fractured))
    return "; ".join(parts)


def _section_label(
    role: str,
    component: Sequence[str],
    nodes: Mapping[str, Mapping[str, Any]],
    goal_requirements: Mapping[str, int],
    success_routes: Sequence[Mapping[str, Any]],
    base_goal_facts: Mapping[str, Mapping[str, Any]],
) -> str:
    if any(
        nodes[node_id].get("kind") == "terminal"
        and nodes[node_id].get("terminal") == "success"
        for node_id in component
    ):
        return _goal_status_label(goal_requirements, success_routes, base_goal_facts)
    labels = {
        "setup": "Enter the exact raw policy",
        "rolling": "Rolling and carrier progression retry policy",
        "temporary_blocking": "Temporary blocking and follow-up routing",
        "protection": "Protection setup and protected routing",
        "fracture": "Fracture preparation, attempt, and retry policy",
        "finishing": "Deterministic finishing",
        "cleanup": "Crafted-mod cleanup",
        "recovery": "Recovery or failure exit",
        "restart": "Restart-equivalent carrier reset",
    }
    return labels[role]


def _references(
    *, nodes: Iterable[str] = (), edges: Iterable[str] = ()
) -> list[dict[str, str]]:
    result = [
        {"node_id": node_id} for node_id in sorted(set(nodes), key=_natural_key)
    ]
    result.extend(
        {"edge_id": edge_id} for edge_id in sorted(set(edges), key=_natural_key)
    )
    return result


def _exact_visit_suffix(
    node_ids: Iterable[str], evaluator_result: Mapping[str, Any] | None
) -> str:
    if not evaluator_result:
        return ""
    visits = {
        entry.get("id"): entry.get("expected_visits")
        for entry in evaluator_result.get("nodes", [])
        if isinstance(entry, dict)
    }
    values = [visits.get(node_id) for node_id in node_ids]
    numeric = [float(value) for value in values if isinstance(value, (int, float))]
    if not numeric:
        return ""
    return f"; exact expected visits {sum(numeric):.12g}"


def generate_projection(
    root: Path,
    strategy_path: Path,
    *,
    projection_id: str,
    expected_sha256: str | None = None,
    evaluator_result: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
    """Generate a deterministic projection after verifying raw file identity."""

    root = root.resolve()
    strategy_path = strategy_path.resolve()
    relative_path = _repo_relative(root, strategy_path)
    source_bytes, _, strategy = _load_strategy(strategy_path)
    actual_sha256 = _sha256(source_bytes)
    if expected_sha256 is not None and actual_sha256 != expected_sha256:
        raise ReviewProjectionError(
            "raw strategy sha256 mismatch before projection: "
            f"expected {expected_sha256}, got {actual_sha256}"
        )
    nodes, edges = _strategy_indexes(strategy)
    goal_requirements, success_routes = _goal_requirements(nodes, edges)
    base_goal_facts = _base_goal_facts(strategy, goal_requirements)

    incoming: dict[str, list[dict[str, Any]]] = defaultdict(list)
    outgoing: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for edge in edges.values():
        incoming[edge["to"]].append(edge)
        outgoing[edge["from"]].append(edge)
    for routes in incoming.values():
        routes.sort(key=lambda edge: _natural_key(edge["id"]))
    for routes in outgoing.values():
        routes.sort(key=lambda edge: _natural_key(edge["id"]))

    components = _ordered_components(strategy, nodes, edges)
    section_by_node: dict[str, int] = {}
    for section_index, component in enumerate(components):
        for node_id in component:
            section_by_node[node_id] = section_index

    section_roles = [
        _section_role(component, nodes, goal_requirements) for component in components
    ]
    section_labels = [
        _section_label(
            role,
            component,
            nodes,
            goal_requirements,
            success_routes,
            base_goal_facts,
        )
        for role, component in zip(section_roles, components)
    ]

    sections: list[dict[str, Any]] = []
    for section_index, component in enumerate(components):
        role = section_roles[section_index]
        section_id = f"section-{section_index + 1:03d}-{role}"
        component_set = set(component)
        section_edge_ids = [
            edge_id for edge_id, edge in edges.items() if edge["from"] in component_set
        ]

        grouped: dict[tuple[Any, ...], dict[str, Any]] = {}
        for node_id in component:
            node = nodes[node_id]
            node_role, _fact_key, label = _node_review(
                node, incoming[node_id], goal_requirements, base_goal_facts
            )
            if (
                node.get("kind") == "terminal"
                and node.get("terminal") == "success"
            ):
                label = section_labels[section_index]

            # A router dedicated to one operation role inherits that descriptive
            # role.  The central exact-state router remains the section role.
            if node.get("kind") == "router":
                target_roles = {
                    _operation_role(nodes[edge["to"]]["operation"], goal_requirements)
                    for edge in outgoing[node_id]
                    if isinstance(nodes[edge["to"]].get("operation"), dict)
                }
                if len(target_roles) == 1:
                    node_role = next(iter(target_roles))
                    label = f"{node_role.replace('_', ' ').title()} outcome routing"

            operation = node.get("operation")
            reference_edges: list[str] = []
            if isinstance(operation, dict):
                reference_edges.extend(edge["id"] for edge in incoming[node_id])
                reference_edges.extend(edge["id"] for edge in outgoing[node_id])
            # Entries are review groups, so nodes with the same fully derived
            # display role/label share one entry even when irrelevant exact
            # route details differ. Their individual raw references remain.
            key = (node_role, label)
            group = grouped.setdefault(
                key,
                {"role": node_role, "label": label, "nodes": [], "edges": []},
            )
            group["nodes"].append(node_id)
            group["edges"].extend(reference_edges)

        entries: list[dict[str, Any]] = []
        ordered_groups = sorted(
            grouped.values(),
            key=lambda group: (
                _ROLE_ORDER[group["role"]],
                group["label"],
                _natural_key(min(group["nodes"], key=_natural_key)),
            ),
        )
        for entry_index, group in enumerate(ordered_groups, start=1):
            entry_label = group["label"] + _exact_visit_suffix(
                group["nodes"], evaluator_result
            )
            entries.append(
                {
                    "id": (
                        f"{section_id}-entry-{entry_index:03d}-"
                        f"{_slug(group['role'])}"
                    ),
                    "label": entry_label,
                    "role": group["role"],
                    "raw_references": _references(
                        nodes=group["nodes"], edges=group["edges"]
                    ),
                }
            )

        boundary_edges = [
            edge
            for edge in outgoing_edge_values(edges, component_set)
            if section_by_node[edge["to"]] != section_index
        ]
        for edge in sorted(boundary_edges, key=lambda item: _natural_key(item["id"])):
            target_index = section_by_node[edge["to"]]
            target_role = section_roles[target_index]
            route_role = target_role
            if target_index < section_index and route_role not in {
                "recovery",
                "restart",
                "cleanup",
            }:
                route_role = "recovery"
            entries.append(
                {
                    "id": (
                        f"{section_id}-entry-{len(entries) + 1:03d}-"
                        f"route-{_slug(edge['id'])}"
                    ),
                    "label": (
                        f"Raw route {edge['id']} to {section_labels[target_index]}"
                    ),
                    "role": route_role,
                    "raw_references": [{"edge_id": edge["id"]}],
                }
            )

        sections.append(
            {
                "id": section_id,
                "label": section_labels[section_index],
                "role": role,
                "derived": True,
                "raw_references": _references(
                    nodes=component, edges=section_edge_ids
                ),
                "entries": entries,
            }
        )

    return {
        "schema_version": SCHEMA_VERSION,
        "projection_id": projection_id,
        "raw_strategy": {
            "path": relative_path,
            "sha256": actual_sha256,
            "execution_authority": "raw_strategy_only",
        },
        "semantics": {
            "executable": False,
            "can_alter_execution": False,
            "can_alter_routing": False,
            "can_alter_legality": False,
            "can_alter_evaluation": False,
        },
        "sections": sections,
    }


def outgoing_edge_values(
    edges: Mapping[str, Mapping[str, Any]], sources: set[str]
) -> Iterable[Mapping[str, Any]]:
    for edge in edges.values():
        if edge["from"] in sources:
            yield edge


def projection_bytes(projection: Mapping[str, Any]) -> bytes:
    return (
        json.dumps(projection, indent=2, ensure_ascii=False, sort_keys=False) + "\n"
    ).encode("utf-8")


def _require_object_keys(
    value: Any,
    *,
    required: set[str],
    allowed: set[str],
    context: str,
) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ReviewProjectionError(f"{context} must be an object")
    missing = required - set(value)
    extra = set(value) - allowed
    if missing:
        raise ReviewProjectionError(f"{context} is missing {sorted(missing)}")
    if extra:
        raise ReviewProjectionError(f"{context} has unknown fields {sorted(extra)}")
    return value


def validate_projection_document(
    projection: Any, schema: Mapping[str, Any]
) -> dict[str, Any]:
    """Validate the closed v1 contract without adding a runtime dependency."""

    top = _require_object_keys(
        projection,
        required={"schema_version", "projection_id", "raw_strategy", "semantics", "sections"},
        allowed={"schema_version", "projection_id", "raw_strategy", "semantics", "sections"},
        context="projection",
    )
    if top["schema_version"] != schema["properties"]["schema_version"]["const"]:
        raise ReviewProjectionError("projection schema_version is unsupported")
    if not isinstance(top["projection_id"], str) or not top["projection_id"]:
        raise ReviewProjectionError("projection_id must be a non-empty string")

    raw = _require_object_keys(
        top["raw_strategy"],
        required={"path", "sha256", "execution_authority"},
        allowed={"path", "sha256", "execution_authority"},
        context="raw_strategy",
    )
    if not isinstance(raw["path"], str) or not raw["path"]:
        raise ReviewProjectionError("raw_strategy.path must be a non-empty string")
    if not isinstance(raw["sha256"], str) or re.fullmatch(r"[0-9a-f]{64}", raw["sha256"]) is None:
        raise ReviewProjectionError("raw_strategy.sha256 must be lowercase SHA-256")
    if raw["execution_authority"] != "raw_strategy_only":
        raise ReviewProjectionError("projection cannot become execution authority")

    semantics = _require_object_keys(
        top["semantics"],
        required={
            "executable",
            "can_alter_execution",
            "can_alter_routing",
            "can_alter_legality",
            "can_alter_evaluation",
        },
        allowed={
            "executable",
            "can_alter_execution",
            "can_alter_routing",
            "can_alter_legality",
            "can_alter_evaluation",
        },
        context="semantics",
    )
    if any(value is not False for value in semantics.values()):
        raise ReviewProjectionError("all projection semantic capabilities must be false")

    roles = set(schema["$defs"]["role"]["enum"])
    sections = top["sections"]
    if not isinstance(sections, list):
        raise ReviewProjectionError("sections must be an array")
    section_ids: set[str] = set()
    entry_ids: set[str] = set()

    def validate_references(value: Any, context: str) -> None:
        if not isinstance(value, list) or not value:
            raise ReviewProjectionError(f"{context} must contain at least one raw reference")
        for index, reference in enumerate(value):
            reference = _require_object_keys(
                reference,
                required=set(),
                allowed={"node_id", "edge_id"},
                context=f"{context}[{index}]",
            )
            if len(reference) != 1:
                raise ReviewProjectionError(
                    f"{context}[{index}] must name exactly one raw node or edge"
                )
            raw_id = next(iter(reference.values()))
            if not isinstance(raw_id, str) or not raw_id:
                raise ReviewProjectionError(f"{context}[{index}] id must be non-empty")

    for section_index, section_value in enumerate(sections):
        section = _require_object_keys(
            section_value,
            required={"id", "label", "role", "derived", "raw_references", "entries"},
            allowed={"id", "label", "role", "derived", "raw_references", "entries"},
            context=f"sections[{section_index}]",
        )
        if not isinstance(section["id"], str) or not section["id"]:
            raise ReviewProjectionError("section id must be non-empty")
        if section["id"] in section_ids:
            raise ReviewProjectionError(f"duplicate section id: {section['id']}")
        section_ids.add(section["id"])
        if not isinstance(section["label"], str) or not section["label"]:
            raise ReviewProjectionError("section label must be non-empty")
        if section["role"] not in roles or section["derived"] is not True:
            raise ReviewProjectionError("section role/derived marker violates the contract")
        validate_references(section["raw_references"], f"sections[{section_index}].raw_references")
        if not isinstance(section["entries"], list):
            raise ReviewProjectionError("section entries must be an array")
        for entry_index, entry_value in enumerate(section["entries"]):
            entry = _require_object_keys(
                entry_value,
                required={"id", "label", "role", "raw_references"},
                allowed={"id", "label", "role", "raw_references"},
                context=f"sections[{section_index}].entries[{entry_index}]",
            )
            if not isinstance(entry["id"], str) or not entry["id"]:
                raise ReviewProjectionError("entry id must be non-empty")
            if entry["id"] in entry_ids:
                raise ReviewProjectionError(f"duplicate entry id: {entry['id']}")
            entry_ids.add(entry["id"])
            if not isinstance(entry["label"], str) or not entry["label"]:
                raise ReviewProjectionError("entry label must be non-empty")
            if entry["role"] not in roles:
                raise ReviewProjectionError(f"unknown entry role: {entry['role']}")
            validate_references(
                entry["raw_references"],
                f"sections[{section_index}].entries[{entry_index}].raw_references",
            )
    return top


def validate_projection(
    projection: Any,
    *,
    root: Path,
    schema: Mapping[str, Any],
    require_complete_coverage: bool = True,
) -> dict[str, Any]:
    """Validate schema, raw identity, references, coverage, SCCs, and boundaries."""

    projection = validate_projection_document(projection, schema)
    raw = projection["raw_strategy"]
    strategy_path = _resolve_inside(root, raw["path"])
    source_bytes, json_bytes, strategy = _load_strategy(strategy_path)
    actual_sha256 = _sha256(source_bytes)
    if actual_sha256 != raw["sha256"]:
        raise ReviewProjectionError(
            "stale projection: raw strategy sha256 mismatch "
            f"(expected {raw['sha256']}, got {actual_sha256})"
        )
    nodes, edges = _strategy_indexes(strategy)

    section_by_node: dict[str, int] = {}
    section_by_edge: dict[str, int] = {}
    boundary_entry_roles: dict[str, set[str]] = defaultdict(set)
    for section_index, section in enumerate(projection["sections"]):
        for reference in section["raw_references"]:
            if "node_id" in reference:
                node_id = reference["node_id"]
                if node_id not in nodes:
                    raise ReviewProjectionError(f"unresolved raw node reference: {node_id}")
                if require_complete_coverage and node_id in section_by_node:
                    raise ReviewProjectionError(f"raw node appears in multiple sections: {node_id}")
                section_by_node[node_id] = section_index
            else:
                edge_id = reference["edge_id"]
                if edge_id not in edges:
                    raise ReviewProjectionError(f"unresolved raw edge reference: {edge_id}")
                if require_complete_coverage and edge_id in section_by_edge:
                    raise ReviewProjectionError(f"raw edge appears in multiple sections: {edge_id}")
                section_by_edge[edge_id] = section_index
        for entry in section["entries"]:
            for reference in entry["raw_references"]:
                if "node_id" in reference:
                    if reference["node_id"] not in nodes:
                        raise ReviewProjectionError(
                            f"unresolved raw node reference: {reference['node_id']}"
                        )
                else:
                    edge_id = reference["edge_id"]
                    if edge_id not in edges:
                        raise ReviewProjectionError(
                            f"unresolved raw edge reference: {edge_id}"
                        )
                    boundary_entry_roles[edge_id].add(entry["role"])

    if require_complete_coverage:
        missing_nodes = set(nodes) - set(section_by_node)
        missing_edges = set(edges) - set(section_by_edge)
        if missing_nodes or missing_edges:
            raise ReviewProjectionError(
                "projection does not cover the complete raw graph: "
                f"{len(missing_nodes)} nodes and {len(missing_edges)} edges missing"
            )
        for edge_id, edge in edges.items():
            if section_by_edge[edge_id] != section_by_node[edge["from"]]:
                raise ReviewProjectionError(
                    f"raw edge {edge_id} is not owned by its source node section"
                )
            source_section = section_by_node[edge["from"]]
            target_section = section_by_node[edge["to"]]
            if source_section != target_section and edge_id not in boundary_entry_roles:
                raise ReviewProjectionError(
                    f"cross-section route {edge_id} lacks a projected route entry"
                )
            if target_section < source_section and not (
                boundary_entry_roles[edge_id] & {"recovery", "restart", "cleanup"}
            ):
                raise ReviewProjectionError(
                    f"backward route {edge_id} is not labelled recovery/restart/cleanup"
                )

        for component in _strong_components(nodes, edges):
            component_sections = {section_by_node[node_id] for node_id in component}
            if len(component_sections) != 1:
                raise ReviewProjectionError(
                    "retry strongly connected component was split across sections: "
                    + ", ".join(component)
                )

    return {
        "raw_source_bytes": source_bytes,
        "raw_strategy_json_bytes": json_bytes,
        "raw_strategy": strategy,
        "nodes": nodes,
        "edges": edges,
    }


def validate_projection_path(
    path: Path,
    *,
    root: Path,
    schema_path: Path,
    require_complete_coverage: bool = True,
) -> dict[str, Any]:
    try:
        projection = json.loads(path.read_text(encoding="utf-8"))
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ReviewProjectionError(f"cannot reload projection or schema: {path}") from exc
    return validate_projection(
        projection,
        root=root,
        schema=schema,
        require_complete_coverage=require_complete_coverage,
    )


def generate_baseline_projections(
    *,
    root: Path,
    baseline_root: Path,
    output_dir: Path,
    case_ids: Sequence[str],
    check: bool = False,
) -> dict[str, Any]:
    root = root.resolve()
    baseline_root = baseline_root.resolve()
    output_dir = output_dir.resolve()
    manifest = json.loads((baseline_root / "manifest.json").read_text(encoding="utf-8"))
    manifest_cases = {entry["id"]: entry for entry in manifest["cases"]}
    purposes = dict(REPRESENTATIVE_CASES)
    schema_path = baseline_root / "contracts" / "review-projection.schema.json"
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    records: list[dict[str, Any]] = []

    for case_id in case_ids:
        if case_id not in manifest_cases:
            raise ReviewProjectionError(f"unknown S8.0 baseline case: {case_id}")
        baseline_path = _resolve_inside(root, manifest_cases[case_id]["path"])
        baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
        identity = baseline.get("strategy")
        if not isinstance(identity, dict):
            raise ReviewProjectionError(f"baseline case has no raw strategy: {case_id}")
        strategy_path = _resolve_inside(root, identity["path"])
        evaluator = baseline.get("evaluator", {})
        evaluator_result = evaluator.get("result") if evaluator.get("status") == "completed" else None
        projection = generate_projection(
            root,
            strategy_path,
            projection_id=f"s8.1-{case_id}-review-v1",
            expected_sha256=identity["gzip_sha256"],
            evaluator_result=evaluator_result,
        )
        validate_projection(
            projection,
            root=root,
            schema=schema,
            require_complete_coverage=True,
        )
        payload = projection_bytes(projection)
        destination = output_dir / f"{case_id}.review.json"
        if check:
            if not destination.exists() or destination.read_bytes() != payload:
                raise ReviewProjectionError(
                    f"committed projection is stale or nondeterministic: {destination}"
                )
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(payload)
        records.append(
            {
                "case_id": case_id,
                "purpose": purposes.get(case_id, "Requested S8.0 representative case."),
                "projection_path": _repo_relative(root, destination),
                "projection_sha256": _sha256(payload),
                "raw_strategy_path": identity["path"],
                "raw_strategy_sha256": identity["gzip_sha256"],
                "evaluator_status": evaluator.get("status"),
            }
        )

    projection_manifest = {
        "schema_version": "s8_review_projection_manifest_v1",
        "projection_set_id": "poecraft2-s8.1-derived-review-v1",
        "scope": "Display-only S8.1 projections over frozen S8.0 ordinary strategies",
        "contract": {
            "path": _repo_relative(root, schema_path),
            "sha256": _sha256(schema_path.read_bytes()),
        },
        "execution_authority": "raw_strategy_only",
        "representatives": records,
        "documented_mismatches_preserved": [
            "Current compiled graphs contain compiler-only mod_count routing that Calculator exact evaluation refuses.",
            "The archived endgame Simulator sample remains 0.9942 against its historical 0.995 threshold.",
            "The temporary-blocker solve did not converge and therefore has no raw strategy to project.",
            "Protected-reforge captures were abandoned because they ran too long and therefore have no raw strategy to project.",
            "The archived S7 artifact pins predate the additive B1.4 artifact.",
        ],
    }
    manifest_payload = projection_bytes(projection_manifest)
    manifest_path = output_dir / "manifest.json"
    if check:
        if not manifest_path.exists() or manifest_path.read_bytes() != manifest_payload:
            raise ReviewProjectionError(
                f"committed projection manifest is stale or nondeterministic: {manifest_path}"
            )
    else:
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_bytes(manifest_payload)
    return projection_manifest


def main(argv: Sequence[str] | None = None) -> int:
    default_root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(
        description="Generate deterministic non-executable S8.1 review projections."
    )
    parser.add_argument("--root", type=Path, default=default_root)
    parser.add_argument(
        "--baseline-root",
        type=Path,
        default=default_root / "fixtures" / "solver-baselines" / "s8.0",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=default_root
        / "fixtures"
        / "solver-baselines"
        / "s8.0"
        / "review-projections",
    )
    parser.add_argument(
        "--case",
        action="append",
        dest="case_ids",
        help="S8.0 case id; repeat to select multiple cases (defaults to representatives).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Refuse stale or byte-different committed projections instead of writing.",
    )
    args = parser.parse_args(argv)
    cases = args.case_ids or [case_id for case_id, _ in REPRESENTATIVE_CASES]
    projection_manifest = generate_baseline_projections(
        root=args.root,
        baseline_root=args.baseline_root,
        output_dir=args.output_dir,
        case_ids=cases,
        check=args.check,
    )
    print(
        f"validated {len(projection_manifest['representatives'])} "
        f"display-only review projections"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
