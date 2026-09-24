#!/usr/bin/env python3
"""Independent finite specification checks; NOT the native PoE implementation.

Uses only the standard library. Run:
  python checks/check_contracts.py --output checks/results.json
The examples check conditional sharing/ownership arguments, not native performance.
"""
from __future__ import annotations
import argparse
import json
import random
import unittest
from dataclasses import dataclass, replace
from fractions import Fraction as F
from pathlib import Path
from typing import Callable, Iterable

Features = frozenset[int]

@dataclass(frozen=True)
class Transfer:
    keep: Features
    add: Features = frozenset()

    def __call__(self, value: Features) -> Features:
        return (value & self.keep) | self.add

@dataclass(frozen=True)
class Node:
    direct: Features
    transfer: Transfer
    successors: tuple[int, ...]

@dataclass(frozen=True)
class Result:
    requirements: tuple[Features, ...]
    rounds: int
    complete: bool
    evaluations: int


def propagate(nodes: tuple[Node, ...], cap: int, *, grouped: bool = False,
              unsafe_key: Callable[[Node], object] | None = None) -> Result:
    """Frozen-round finite set analysis; grouped equality is full input equality."""
    if not nodes or cap < 0:
        raise ValueError('nonempty graph and nonnegative cap required')
    if any(j < 0 or j >= len(nodes) for n in nodes for j in n.successors):
        raise ValueError('unknown successor')
    groups: dict[object, list[int]] = {}
    for i, n in enumerate(nodes):
        key = unsafe_key(n) if unsafe_key is not None else (n if grouped else i)
        groups.setdefault(key, []).append(i)
    old = tuple(n.direct for n in nodes)
    evaluations = 0
    for epoch in range(1, cap + 1):
        new = list(old)
        for members in groups.values():
            i = members[0]
            value = old[i]
            for j in nodes[i].successors:
                value = value | nodes[i].transfer(old[j])
                evaluations += 1
            for member in members:
                new[member] = value
        nxt = tuple(new)
        if nxt == old:
            return Result(nxt, epoch, True, evaluations)
        old = nxt
    return Result(old, cap, False, evaluations)


def decode_one_level_sources(payloads: tuple[object | None, ...],
                             sources: tuple[int | None, ...]) -> tuple[object, ...]:
    if len(payloads) != len(sources):
        raise ValueError('size mismatch')
    out = []
    for i, (payload, source) in enumerate(zip(payloads, sources)):
        if source is None:
            if payload is None:
                raise ValueError('missing owner payload')
            out.append(payload)
        else:
            if source == i or source < 0 or source >= len(payloads):
                raise ValueError('invalid source')
            if payload is not None or sources[source] is not None or payloads[source] is None:
                raise ValueError('chained or conflicting source')
            out.append(payloads[source])
    return tuple(out)


def observation_key(parent: str, required: Features,
                    features: dict[int, int]) -> tuple[object, ...]:
    if not required.issubset(features):
        raise ValueError('missing observed field')
    return (parent, tuple(sorted(required)), tuple((k, features[k]) for k in sorted(required)))


class Inventory:
    """Toy owned vector: net charge and separately exposed reallocation peak."""
    def __init__(self, element_bytes: int = 16):
        self.element_bytes = element_bytes
        self.capacity = 0
        self.payloads: list[int] = []
        self.total = 0
        self.logical_work = 0

    def append(self, nested: int) -> int:
        if nested < 0:
            raise ValueError('negative size')
        new_capacity = max(1, 2 * self.capacity) if len(self.payloads) == self.capacity else self.capacity
        peak = self.total + nested
        if new_capacity != self.capacity:
            peak += new_capacity * self.element_bytes
        self.total += (new_capacity - self.capacity) * self.element_bytes + nested
        self.capacity = new_capacity
        self.payloads.append(nested)
        self.logical_work += 1
        return peak

    def replace(self, index: int, nested: int) -> None:
        self.total += nested - self.payloads[index]
        self.payloads[index] = nested
        self.logical_work += 1

    def take_payload(self, index: int) -> int:
        value = self.payloads[index]
        self.payloads[index] = 0
        self.total -= value
        return value

    def audit(self) -> int:
        return self.capacity * self.element_bytes + sum(self.payloads)

    def release(self) -> None:
        self.payloads.clear()
        self.capacity = 0
        self.total = 0


class StagedResult:
    def __init__(self, committed: object | None):
        self.committed = committed
        self.pending: object | None = None
        self.work = 0

    def stage(self, value: object) -> None:
        self.pending = value
        self.work += 1

    def commit(self, complete: bool) -> None:
        if not complete or self.pending is None:
            raise ValueError('cannot commit incomplete output')
        self.committed, self.pending = self.pending, None

    def cancel(self) -> None:
        self.pending = None


class ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keep = Transfer(frozenset(range(8)))

    def test_grouped_equals_cold_generated_frozen_rounds(self):
        rng = random.Random(9032)
        for _ in range(64):
            nodes = []
            for i in range(7):
                nodes.append(Node(frozenset(k for k in range(4) if rng.randrange(4) == 0),
                                  Transfer(frozenset(k for k in range(4) if rng.randrange(4) != 0)),
                                  tuple(j for j in range(7) if rng.randrange(5) == 0)))
            nodes[1] = nodes[0]
            ns = tuple(nodes)
            for cap in range(8):
                cold, shared = propagate(ns, cap), propagate(ns, cap, grouped=True)
                self.assertEqual((cold.requirements, cold.rounds, cold.complete),
                                 (shared.requirements, shared.rounds, shared.complete))
                self.assertLessEqual(shared.evaluations, cold.evaluations)

    def test_valid_group_actually_skips_operations(self):
        n = Node(frozenset(), self.keep, (2,))
        nodes = (n, n, Node(frozenset({1}), self.keep, ()))
        cold, shared = propagate(nodes, 8), propagate(nodes, 8, grouped=True)
        self.assertEqual(shared.requirements, cold.requirements)
        self.assertLess(shared.evaluations, cold.evaluations)

    def test_same_transfer_different_direct_is_not_group(self):
        a = Node(frozenset({1}), self.keep, ())
        b = Node(frozenset({2}), self.keep, ())
        nodes = (a, b)
        cold = propagate(nodes, 1)
        bad = propagate(nodes, 1, unsafe_key=lambda n: n.transfer)
        self.assertNotEqual(cold.requirements, bad.requirements)
        self.assertEqual(propagate(nodes, 1, grouped=True), cold)

    def test_same_direct_different_successor_is_not_group(self):
        nodes = (Node(frozenset(), self.keep, (2,)), Node(frozenset(), self.keep, (3,)),
                 Node(frozenset({1}), self.keep, ()), Node(frozenset({2}), self.keep, ()))
        cold = propagate(nodes, 4)
        bad = propagate(nodes, 4, unsafe_key=lambda n: (n.direct, n.transfer))
        self.assertNotEqual(cold.requirements, bad.requirements)

    def test_missing_successor_refuses(self):
        with self.assertRaises(ValueError):
            propagate((Node(frozenset(), self.keep, (7,)),), 2)

    def test_complete_confirmation_round_is_preserved(self):
        nodes = (Node(frozenset(), self.keep, (1,)), Node(frozenset({1}), self.keep, ()))
        self.assertFalse(propagate(nodes, 1, grouped=True).complete)
        self.assertTrue(propagate(nodes, 2, grouped=True).complete)
        self.assertEqual(propagate(nodes, 2, grouped=True).rounds, 2)

    def test_in_place_sweep_changes_capped_prefix(self):
        nodes = (Node(frozenset(), self.keep, (1,)), Node(frozenset(), self.keep, (2,)),
                 Node(frozenset({1}), self.keep, ()))
        sync = propagate(nodes, 1).requirements
        immediate = [n.direct for n in nodes]
        for i in (2, 1, 0):
            for j in nodes[i].successors:
                immediate[i] = immediate[i] | nodes[i].transfer(immediate[j])
        self.assertNotEqual(sync, tuple(immediate))
        self.assertEqual(propagate(nodes, 4).requirements, tuple(immediate))

    def test_transfer_memo_needs_complete_key(self):
        t1, t2 = Transfer(frozenset({1})), Transfer(frozenset({2}))
        x = frozenset({1, 2})
        self.assertNotEqual(t1(x), t2(x))
        memo = {(t1, x): t1(x), (t2, x): t2(x)}
        self.assertEqual(memo[(t1, x)], t1(x))
        self.assertEqual(memo[(t2, x)], t2(x))

    def test_same_requirement_different_native_values(self):
        p1, p2 = F(1, 5), F(1, 100)
        self.assertEqual(F(1)/p1, 5)
        self.assertEqual(F(1)/p2, 100)
        direct = 20
        self.assertLess(F(1)/p1, direct)
        self.assertGreater(F(1)/p2, direct)

    def test_shared_input_sources_decode(self):
        values = decode_one_level_sources((('real_contract', 1), None, ('other', 2)), (None, 0, None))
        self.assertEqual(values[0], values[1])
        self.assertNotEqual(values[1], values[2])

    def test_missing_chained_self_sources_refuse(self):
        for payloads, sources in [((None,), (3,)), ((None,), (0,)),
                                  (('x', None, None), (None, 0, 1)),
                                  (('x', 'y'), (None, 0))]:
            with self.subTest(sources=sources), self.assertRaises(ValueError):
                decode_one_level_sources(payloads, sources)

    def test_required_fields_not_feature_values(self):
        required = frozenset({1})
        a = observation_key('parent', required, {1: 2, 3: 7})
        b = observation_key('parent', required, {1: 0, 3: 7})
        self.assertNotEqual(a, b)
        self.assertEqual(a, observation_key('parent', required, {1: 2, 3: 999}))

    def test_parent_identity_is_part_of_observation(self):
        self.assertNotEqual(observation_key('p', frozenset({1}), {1: 2}),
                            observation_key('q', frozenset({1}), {1: 2}))

    def test_missing_required_feature_refuses(self):
        with self.assertRaises(ValueError):
            observation_key('p', frozenset({1, 2}), {1: 7})

    def test_requirement_generation_change_invalidates(self):
        features = {1: 8, 2: 9}
        self.assertNotEqual(observation_key('p', frozenset({1}), features),
                            observation_key('p', frozenset({1, 2}), features))

    def test_inventory_updates_match_full_audit(self):
        inv = Inventory()
        for nested in (10, 40, 0, 300, 80, 5):
            inv.append(nested)
            self.assertEqual(inv.total, inv.audit())
        for i, nested in ((0, 50), (3, 0), (2, 71)):
            inv.replace(i, nested)
            self.assertEqual(inv.total, inv.audit())

    def test_growth_peak_not_net_delta(self):
        inv = Inventory()
        inv.append(9)
        before = inv.total
        peak = inv.append(11)
        self.assertEqual(peak, before + 2*inv.element_bytes + 11)
        self.assertGreater(peak, inv.total)

    def test_move_preserves_live_sum_and_work_debit(self):
        inv = Inventory()
        inv.append(50)
        before = inv.total
        moved = inv.take_payload(0)
        self.assertEqual(inv.total + moved, before)
        spent = inv.logical_work
        inv.release()
        self.assertEqual(inv.total, 0)
        self.assertEqual(inv.logical_work, spent)

    def test_saturated_subtraction_is_not_exact(self):
        limit = 255
        actual = 200 + 100
        saturated = min(limit, actual)
        self.assertNotEqual(saturated - 100, actual - 100)

    def test_prefix_scan_operation_count(self):
        for n in (0, 1, 10, 1000):
            self.assertEqual(sum(range(1, n+1)), n*(n+1)//2)
            if n > 1:
                self.assertGreater(sum(range(1, n+1)), n)

    def test_cancel_preserves_committed_witness(self):
        s = StagedResult(('graph1', 'certificate1'))
        s.stage(('incomplete_graph', None))
        spent = s.work
        s.cancel()
        self.assertEqual(s.committed, ('graph1', 'certificate1'))
        self.assertIsNone(s.pending)
        self.assertEqual(s.work, spent)

    def test_partial_output_cannot_commit(self):
        s = StagedResult(None)
        s.stage('partial')
        with self.assertRaises(ValueError):
            s.commit(False)
        self.assertIsNone(s.committed)

    def test_complete_bundle_commits_together(self):
        s = StagedResult(('old_graph', 'old_certificate'))
        s.stage(('new_graph', 'new_certificate'))
        s.commit(True)
        self.assertEqual(s.committed, ('new_graph', 'new_certificate'))
        self.assertIsNone(s.pending)

    def test_baseline_arithmetic_is_not_a_savings_claim(self):
        baseline = json.loads((Path(__file__).resolve().parents[1]/'evidence/baseline.json').read_text())
        b, a = baseline['B'], baseline['A']
        delta = F(b['carrier_discovery_prefix_ns'] - b['locator_loop_ns'], 10**9)
        self.assertEqual(delta, F('50.6508783'))
        self.assertLess(a['protected_kernel_ns'], a['automatic_admission_ns'])
        self.assertLess(b['selected_row_build_ns'], b['locator_loop_ns'])
        self.assertEqual(a['rows'], b['rows'])
        self.assertEqual(a['cost'], b['cost'])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests)
    names = [test.id().split('.')[-1] for test in suite]
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {
        'schema': 'strict_preparation_abstract_checks_v1',
        'classification': 'Independent finite specification checks, not native game rules or performance',
        'tests_run': result.testsRun,
        'failures': len(result.failures), 'errors': len(result.errors),
        'passed': result.wasSuccessful(), 'test_names': names,
        'generated_frozen_graphs': 64,
        'compared_round_caps_per_graph': 8,
        'cold_grouped_prefix_comparisons': 512,
        'native_solver_executed': False,
        'derived_D1_unseparated_interval_s': '50.6508783',
        'unseparated_interval_is_not_measured_redundant_work': True,
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if result.wasSuccessful() else 1

if __name__ == '__main__':
    raise SystemExit(main())
