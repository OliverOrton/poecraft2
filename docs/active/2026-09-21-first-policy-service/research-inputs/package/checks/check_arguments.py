#!/usr/bin/env python3
"""Abstract compiler/lifetime/latency examples; does NOT load or test poecraft2.

Run from any working directory:
    python checks/check_arguments.py --output evidence/abstract_checks.json

Tests explain proposed contracts. They are not a native compiler, evaluator,
profiling tool, execution supervisor or substitute for repository qualification.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from decimal import Decimal
import hashlib
import io
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class FrozenInput:
    target: str
    epoch: int
    operations: tuple[str, ...]

    def tokens(self) -> tuple[bytes, ...]:
        # Toy deterministic serialization, intentionally unrelated to PoE JSON.
        rows = (f"target={self.target};epoch={self.epoch}\n",) + tuple(
            f"{i}:{op}\n" for i, op in enumerate(self.operations)
        )
        return tuple(row.encode("utf-8") for row in rows)


@dataclass(frozen=True)
class Compiled:
    source: FrozenInput
    graph: bytes


class StagedEmitter:
    def __init__(self, source: FrozenInput, limit: int = 10_000) -> None:
        if limit < 0:
            raise ValueError("negative budget")
        self.source = source
        self.limit = limit
        self.cursor = 0
        self.debit = 0
        self.scratch: list[bytes] = []
        self.live = 0
        self.state = "pending"
        self._result: Compiled | None = None

    def step(self, quantum: int, current_epoch: int | None = None) -> bool:
        if quantum <= 0:
            raise ValueError("quantum must be positive")
        if self.state in {"cancelled", "failed", "stale"}:
            raise RuntimeError("terminal incomplete emitter")
        if self.state == "done":
            return True
        if current_epoch is not None and current_epoch != self.source.epoch:
            self.scratch.clear()
            self.live = 0
            self.state = "stale"
            raise RuntimeError("stale input")
        tokens = self.source.tokens()
        for _ in range(quantum):
            if self.cursor == len(tokens):
                break
            piece = tokens[self.cursor]
            if self.live + len(piece) > self.limit:
                self.state = "failed"
                raise MemoryError("toy output budget")
            self.scratch.append(piece)
            self.live += len(piece)
            self.cursor += 1
            self.debit += 1
        if self.cursor == len(tokens):
            self._result = Compiled(self.source, b"".join(self.scratch))
            self.state = "done"
        return self.state == "done"

    def observe(self) -> tuple[str, int, int, int]:
        return self.state, self.cursor, self.live, self.debit

    def take(self) -> Compiled:
        if self.state != "done" or self._result is None:
            raise RuntimeError("incomplete output is not a result")
        return self._result

    def cancel(self) -> None:
        if self.state == "done":
            return
        self.scratch.clear()
        self.live = 0
        self.state = "cancelled"


@dataclass(frozen=True)
class CheckToken:
    target: str
    graph_sha256: str


class Publisher:
    def __init__(self, existing: Compiled | None = None) -> None:
        self.current = existing

    def publish(self, graph: Compiled, token: CheckToken | None) -> None:
        if token is None or token.target != graph.source.target or (
            token.graph_sha256 != hashlib.sha256(graph.graph).hexdigest()
        ):
            raise ValueError("missing or mismatched abstract check token")
        self.current = graph


@dataclass(frozen=True)
class Edge:
    identity: str
    source: str
    target: str
    condition: str
    priority: int
    default: bool = False


def designated_pair(
    left: tuple[Edge, ...], right: tuple[Edge, ...],
    allowed: dict[str, tuple[str, str]],
    left_operations: tuple[str, ...] = (), right_operations: tuple[str, ...] = (),
) -> bool:
    """Toy specified relation, NOT the native paired-default checker."""
    if len(left) != len(right) or left_operations != right_operations:
        return False
    for a, b in zip(left, right):
        if (a.identity, a.source, a.condition, a.priority, a.default) != (
            b.identity, b.source, b.condition, b.priority, b.default
        ):
            return False
        if a.target != b.target:
            if not a.default or allowed.get(a.identity) != (a.target, b.target):
                return False
    return True


def duration(start: tuple[str, Decimal], end: tuple[str, Decimal]) -> Decimal:
    if start[0] != end[0]:
        raise ValueError("unrelated clocks")
    if end[1] < start[1]:
        raise ValueError("negative interval")
    return end[1] - start[1]


def exclusive_remainder(parent: Decimal, children: list[Decimal]) -> Decimal:
    """Caller must establish disjointness; this tests arithmetic, not tracing."""
    if parent < 0 or any(v < 0 for v in children) or sum(children) > parent:
        raise ValueError("invalid additive attribution")
    return parent - sum(children)


class ArgumentChecks(unittest.TestCase):
    def setUp(self) -> None:
        self.source = FrozenInput("target-A", 7, ("roll", "route", "finish"))

    def test_complete_emission_matches_all_quanta(self) -> None:
        for count in (0, 1, 4, 31):
            source = FrozenInput("target-A", 7, tuple(f"op-{i}" for i in range(count)))
            for quantum in (1, 2, 7, 4096):
                with self.subTest(count=count, quantum=quantum):
                    work = StagedEmitter(source)
                    while not work.step(quantum):
                        pass
                    self.assertEqual(work.take().graph, b"".join(source.tokens()))

    def test_every_incomplete_prefix_refuses_result(self) -> None:
        work = StagedEmitter(self.source)
        for _ in range(len(self.source.tokens())):
            with self.assertRaises(RuntimeError):
                work.take()
            work.step(1)
        self.assertEqual(work.state, "done")

    def test_cancel_at_each_prefix_preserves_debit(self) -> None:
        for prefix in range(len(self.source.tokens())):
            work = StagedEmitter(self.source)
            for _ in range(prefix):
                work.step(1)
            consumed = work.debit
            work.cancel()
            self.assertEqual(work.live, 0)
            self.assertEqual(work.debit, consumed)
            with self.assertRaises(RuntimeError):
                work.take()

    def test_observation_is_passive(self) -> None:
        work = StagedEmitter(self.source)
        work.step(1)
        before = work.observe()
        for _ in range(1_000):
            self.assertEqual(work.observe(), before)
        self.assertEqual(work.observe(), before)

    def test_stale_input_refuses_after_suspension(self) -> None:
        work = StagedEmitter(self.source)
        work.step(1, 7)
        with self.assertRaises(RuntimeError):
            work.step(1, 8)
        self.assertEqual((work.state, work.live, work.debit), ("stale", 0, 1))

    def test_limit_failure_cannot_create_partial_result(self) -> None:
        work = StagedEmitter(self.source, len(self.source.tokens()[0]))
        work.step(1)
        with self.assertRaises(MemoryError):
            work.step(1)
        with self.assertRaises(RuntimeError):
            work.take()

    def test_compile_completion_is_not_check_evidence(self) -> None:
        work = StagedEmitter(self.source)
        work.step(100)
        publisher = Publisher()
        with self.assertRaises(ValueError):
            publisher.publish(work.take(), None)
        self.assertIsNone(publisher.current)

    def test_old_artifact_survives_new_bad_evidence(self) -> None:
        old = Compiled(self.source, b"old graph")
        changed = Compiled(self.source, b"different graph")
        token = CheckToken(self.source.target, hashlib.sha256(old.graph).hexdigest())
        publisher = Publisher(old)
        with self.assertRaises(ValueError):
            publisher.publish(changed, token)
        self.assertEqual(publisher.current, old)

    def test_target_is_part_of_evidence(self) -> None:
        graph = Compiled(self.source, b"same bytes")
        token = CheckToken("target-B", hashlib.sha256(graph.graph).hexdigest())
        with self.assertRaises(ValueError):
            Publisher().publish(graph, token)

    def test_matching_abstract_bundle_can_commit(self) -> None:
        graph = Compiled(self.source, b"toy complete graph")
        token = CheckToken(self.source.target, hashlib.sha256(graph.graph).hexdigest())
        publisher = Publisher()
        publisher.publish(graph, token)
        self.assertEqual(publisher.current, graph)

    def test_only_designated_default_changes_allowed(self) -> None:
        a = (Edge("e1", "router", "offpolicy", "", 3, True),)
        b = (Edge("e1", "router", "restart", "", 3, True),)
        self.assertTrue(designated_pair(a, b, {"e1": ("offpolicy", "restart")}))
        self.assertFalse(designated_pair(a, b, {}))

    def test_pair_rejects_other_field_changes(self) -> None:
        a = (Edge("e1", "router", "offpolicy", "", 3, True),)
        allowed = {"e1": ("offpolicy", "restart")}
        bad = (
            Edge("e1", "router", "restart", "new test", 3, True),
            Edge("e1", "router", "restart", "", 4, True),
            Edge("e1", "router", "restart", "", 3, False),
        )
        for b in bad:
            self.assertFalse(designated_pair(a, (b,), allowed))
        self.assertFalse(designated_pair(a, a, {}, ("paid roll",), ("free roll",)))

    def test_post_move_fit_does_not_establish_overlap_fit(self) -> None:
        fixed, old, new, limit = 100, 60, 90, 220
        self.assertLessEqual(fixed + new, limit)
        self.assertGreater(fixed + old + new, limit)
        # Refusal leaves the original allocation accounted, not a fictitious refund.
        self.assertEqual(fixed + old, 160)

    def test_frozen_charge_matches_mutation_delta(self) -> None:
        fixed = 123
        capacities = [16, 32, 32, 64, 128]
        keys = [0, 1, 1, 3, 4]
        maintained = fixed
        previous_capacity = previous_keys = 0
        for capacity, key_count in zip(capacities, keys):
            maintained += capacity - previous_capacity + 24 * (key_count - previous_keys)
            full = fixed + capacity + 24 * key_count
            self.assertEqual(maintained, full)
            previous_capacity, previous_keys = capacity, key_count

    def test_mutation_of_frozen_domain_breaks_shortcut(self) -> None:
        frozen, growing, hidden_growth = 100, 20, 50
        self.assertNotEqual(frozen + growing, frozen + growing + hidden_growth)

    def test_small_quota_cannot_remove_single_long_unit(self) -> None:
        entry, body = 300, 5
        for quota in (1, 8, 32):
            first_call = entry + body  # one work unit, regardless of allowed quota
            self.assertGreater(first_call, 250)

    def test_body_yields_do_not_remove_runtime_entry(self) -> None:
        calls = [300 + 1, 1, 1, 1, 1]
        self.assertEqual(sum(calls), 305)
        self.assertGreater(max(calls), 250)

    def test_different_clock_subtraction_refuses(self) -> None:
        with self.assertRaises(ValueError):
            duration(("worker", Decimal("41")), ("ui", Decimal("44")))
        self.assertEqual(duration(("ui", Decimal("41")), ("ui", Decimal("44"))), Decimal("3"))

    def test_unassigned_remainder_is_not_forced_to_a_cause(self) -> None:
        self.assertEqual(exclusive_remainder(Decimal(373), [Decimal(50), Decimal(200)]), Decimal(123))
        with self.assertRaises(ValueError):
            exclusive_remainder(Decimal(100), [Decimal(80), Decimal(80)])

    def test_recorded_first_verification_and_delivery_are_separate(self) -> None:
        data = json.loads((ROOT / "evidence/baseline.json").read_text(encoding="utf-8"))
        runs = data["finish_runs"]
        for a, b in ((runs[0], runs[1]), (runs[3], runs[2])):
            self.assertGreater(Decimal(b["first_verified_worker_ms"]) - Decimal(a["first_verified_worker_ms"]), 0)
            self.assertLess(Decimal(b["delivery_ui_ms"]) - Decimal(a["delivery_ui_ms"]), 0)


    def test_safe_pending_lower_can_lose_one_shot_service(self) -> None:
        true_cost, completed_lower, pending_lower = 3, 3, 0
        self.assertLessEqual(pending_lower, true_cost)
        first_attempt_certified = pending_lower >= true_cost
        permanently_attempted = True
        retried = not permanently_attempted
        self.assertFalse(first_attempt_certified or (retried and completed_lower >= true_cost))
        self.assertTrue(completed_lower >= true_cost)

    def test_pending_debt_consumes_committed_generation_once(self) -> None:
        state, reads, decisions = "pending", 0, 0
        for _ in range(5):
            reads += 1
            self.assertEqual(state, "pending")
        state = "ready"
        if state == "ready":
            decisions += 1
            state = "handled"
        self.assertEqual((reads, decisions, state), (5, 1, "handled"))

    def test_clamping_to_policy_cost_does_not_validate_a_lower(self) -> None:
        optimum, policy_cost, proposed_lower = 4, 10, 12
        clamped = min(proposed_lower, policy_cost)
        self.assertLessEqual(clamped, policy_cost)
        self.assertGreater(clamped, optimum)  # bracket appearance does not prove admissibility


def derive_recorded() -> dict:
    data = json.loads((ROOT / "evidence/baseline.json").read_text(encoding="utf-8"))
    runs = data["finish_runs"]
    comparisons = []
    for name, baseline, candidate in (("AB", runs[0], runs[1]), ("BA", runs[3], runs[2])):
        comparisons.append({"order": name, "candidate_minus_baseline_ms": {
            key: str(Decimal(candidate[key]) - Decimal(baseline[key]))
            for key in ("begin_ms", "max_ordinary_ms", "finish_ui_ms", "delivery_ui_ms", "first_verified_worker_ms")
        }})
    return {"kind": "arithmetic on recorded projections, not new runtime measurements", "pairs": comparisons,
            "native_peak_owned_delta_bytes": data["native_c5"]["peak_owned_candidate"] - data["native_c5"]["peak_owned_baseline"],
            "native_live_owned_delta_bytes": data["native_c5"]["live_owned_candidate"] - data["native_c5"]["live_owned_baseline"],
            "statistical_claim": "none; two pairs do not establish universal effects or a causal allocation of the deltas"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "evidence/abstract_checks.json")
    args = parser.parse_args()
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(ArgumentChecks))
    report = {"test_kind": "abstract examples only; no repository native/WASM execution",
              "tests_run": result.testsRun, "failures": len(result.failures), "errors": len(result.errors),
              "skipped": len(result.skipped), "passed": result.wasSuccessful(),
              "details": stream.getvalue(), "derived_recorded_metrics": derive_recorded()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("tests_run", "failures", "errors", "passed")}))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
