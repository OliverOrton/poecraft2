"""Exact arithmetic/specification checks, NOT native PoE qualification.

Run from any directory: python checks/abstract_checks.py
No repository modules, external dependencies, simulated crafting, or network.
"""
from __future__ import annotations
from dataclasses import dataclass
from fractions import Fraction as Q
from pathlib import Path
import json
import unittest

@dataclass(frozen=True)
class Affix:
    name: str
    side: int
    fractured: bool = False
    crafted: bool = False
    requested: bool = False


def one_lock_survivors(affixes: tuple[Affix, ...], locked_side: int) -> tuple[Affix, ...]:
    """Specification of the recorded owner rule; does not call native Scour."""
    if locked_side not in (0, 1):
        raise ValueError('exactly one valid locked side required')
    return tuple(a for a in affixes if a.side == locked_side or a.fractured)


def remove_crafted_spec(affixes: tuple[Affix, ...]) -> tuple[Affix, ...]:
    return tuple(a for a in affixes if not a.crafted or a.fractured)


def terminal(affixes: tuple[Affix, ...], required: int, extras_allowed: bool) -> bool:
    held = sum(a.requested for a in affixes)
    return held >= required and (extras_allowed or len(affixes) == held)


def complete_value(cost: Q, exits: dict[str, Q], tails: dict[str, Q]) -> Q:
    if any(p < 0 for p in exits.values()) or sum(exits.values(), Q(0)) != 1:
        raise ValueError('incomplete probability law')
    if any(p > 0 and s not in tails for s, p in exits.items()):
        raise ValueError('unknown positive continuation')
    return cost + sum((p * tails[s] for s, p in exits.items() if p), Q(0))


class Contracts(unittest.TestCase):
    def test_one_lock_keeps_opposite_fracture(self):
        a = (Affix('goal', 0, requested=True), Affix('lock', 1, crafted=True),
             Affix('fracture', 1, fractured=True), Affix('junk', 1))
        self.assertEqual([x.name for x in one_lock_survivors(a, 0)], ['goal', 'fracture'])

    def test_mirror_and_no_double_count(self):
        a = (Affix('held-fracture', 1, True), Affix('held', 1),
             Affix('other-fracture', 0, True), Affix('other', 0))
        kept = one_lock_survivors(a, 1)
        self.assertEqual(len(kept), 3)
        self.assertEqual(len(set(kept)), 3)

    def test_surviving_fractured_junk_not_clean_goal(self):
        a = (Affix('goal', 0, requested=True), Affix('junk', 1, fractured=True))
        kept = one_lock_survivors(a, 0)
        self.assertFalse(terminal(kept, 1, False))
        self.assertTrue(terminal(kept, 1, True))

    def test_remove_crafted_is_not_selective(self):
        a = (Affix('natural', 0, requested=True),
             Affix('requested-craft', 0, crafted=True, requested=True),
             Affix('multimod', 1, crafted=True), Affix('fractured-craft', 1, True, True))
        kept = remove_crafted_spec(a)
        self.assertEqual([x.name for x in kept], ['natural', 'fractured-craft'])
        self.assertFalse(terminal(kept, 2, True))

    def test_multimod_target_modes(self):
        a = (Affix('craft-a', 0, crafted=True, requested=True),
             Affix('craft-b', 1, crafted=True, requested=True),
             Affix('multimod', 1, crafted=True))
        self.assertTrue(terminal(a, 2, True))
        self.assertFalse(terminal(a, 2, False))
        self.assertEqual(sum(x.crafted for x in a), 3)

    def test_consumed_lock_paid_every_attempt(self):
        lock, roll, p = Q(4), Q(1), Q(1, 5)
        value = (lock + roll) / p
        self.assertEqual(value, 25)
        self.assertNotEqual(value, lock + roll / p)
        self.assertEqual(value, lock + roll + (1-p)*value)

    def test_complete_prefix_tail(self):
        self.assertEqual(complete_value(Q(2), {'tail': Q(1)}, {'tail': Q(7)}), 9)
        with self.assertRaises(ValueError):
            complete_value(Q(2), {'different-entry': Q(1)}, {'tail': Q(7)})

    def test_missing_mass_never_renormalized(self):
        with self.assertRaises(ValueError):
            complete_value(Q(1), {'goal': Q(9, 10)}, {'goal': Q(0)})

    def test_old_lower_not_portable_to_enlarged_scope(self):
        old_lower = Q('84.16')
        cleanup = Q('0.3741')
        self.assertGreater(old_lower, cleanup)
        self.assertFalse(old_lower <= complete_value(cleanup, {'goal': Q(1)}, {'goal': Q(0)}))

    def test_clamping_destination_does_not_fix_predecessor(self):
        v_root, v_tail = Q(101), Q(100)
        self.assertLessEqual(v_root, 1 + v_tail)
        v_tail = Q(2, 5)
        self.assertFalse(v_root <= 1 + v_tail)
        # A freshly computed vector must be checked throughout the relevant domain.
        v_root = 1 + v_tail
        self.assertEqual(v_root, Q(7, 5))

    def test_macro_lower_follows_each_internal_inequality(self):
        # A two-step proper programme: r->t costs 2, t->goal costs 3.
        v_r, v_t = Q(4), Q(3)
        self.assertLessEqual(v_r, 2 + v_t)
        self.assertLessEqual(v_t, 3)
        self.assertLessEqual(v_r, complete_value(Q(5), {'goal': Q(1)}, {'goal': Q(0)}))

    def test_usage_is_not_any_use_probability(self):
        # Enter a region with probability 1/2; then use an action geometrically
        # with escape probability 1/4. Expected uses 2, chance of any use 1/2.
        probability_any = Q(1, 2)
        expected_uses = probability_any / Q(1, 4)
        self.assertEqual(expected_uses, 2)
        self.assertNotEqual(probability_any, expected_uses)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {
        'scope': 'exact-rational illustrative specifications, not native engine tests',
        'native_modules_imported': False,
        'tests_run': result.testsRun,
        'failures': len(result.failures),
        'errors': len(result.errors),
        'successful': result.wasSuccessful(),
        'test_names': sorted(n for n in dir(Contracts) if n.startswith('test_')),
    }
    (Path(__file__).parent / 'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
