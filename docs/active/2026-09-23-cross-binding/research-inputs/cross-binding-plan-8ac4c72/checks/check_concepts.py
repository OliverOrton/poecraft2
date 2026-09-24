#!/usr/bin/env python3
"""Exact, dependency-free illustrations; not native PoE or performance tests.

Run: python checks/check_concepts.py --output checks/results.json
"""
from __future__ import annotations
import argparse
import itertools
import json
import unittest
from dataclasses import dataclass
from fractions import Fraction as F
from pathlib import Path


@dataclass(frozen=True)
class Node:
    op: str
    args: tuple[int, ...]


@dataclass(frozen=True)
class PairTemplate:
    """One shared expression for the event A,B in two weighted draws.

    This small independent model has no native game-mechanics implementation.
    The first three entries are numeric parameter leaves a,b,j.
    """
    nodes: tuple[Node, ...] = (
        Node('input', (0,)), Node('input', (1,)), Node('input', (2,)),
        Node('add', (0, 1)), Node('add', (3, 2)),
        Node('sub', (4, 0)), Node('sub', (4, 1)),
        Node('div', (0, 4)), Node('div', (1, 5)), Node('mul', (7, 8)),
        Node('div', (1, 4)), Node('div', (0, 6)), Node('mul', (10, 11)),
        Node('add', (9, 12)),
    )

    def evaluate(self, weights: tuple[int | F, int | F, int | F]) -> F:
        bound = tuple(F(w) for w in weights)
        if len(bound) != 3 or any(w <= 0 for w in bound):
            raise ValueError('This template region requires three positive weights')
        values: list[F] = []
        for idx, node in enumerate(self.nodes):
            if node.op == 'input':
                values.append(bound[node.args[0]])
                continue
            if any(a >= idx or a < 0 for a in node.args):
                raise ValueError('Non-topological circuit')
            x, y = (values[a] for a in node.args)
            if node.op == 'add': value = x + y
            elif node.op == 'sub': value = x - y
            elif node.op == 'mul': value = x * y
            elif node.op == 'div':
                if y == 0: raise ZeroDivisionError('Invalid bound denominator')
                value = x / y
            else: raise ValueError(node.op)
            values.append(value)
        return values[-1]


def cold_pair(weights: tuple[int, ...], required: frozenset[int]) -> F:
    """Independent path enumeration for the simple no-repeat toy."""
    total = sum(weights)
    if any(w < 0 for w in weights) or total <= 0:
        raise ValueError('Invalid pool')
    answer = F(0)
    for first, second in itertools.permutations(range(len(weights)), 2):
        if frozenset((first, second)) != required or weights[first] == 0:
            continue
        remaining = total - weights[first]
        if remaining <= 0:
            continue
        answer += F(weights[first], total) * F(weights[second], remaining)
    return answer


def retry_value(cost: F, return_p: F) -> F:
    if not 0 <= return_p < 1:
        raise ValueError('Not a finite proper retry chain')
    return cost / (1 - return_p)


def normalize(outcomes: dict[int, F], retry: set[int], stop: set[int]) -> dict[str | int, F]:
    result: dict[str | int, F] = {}
    for state, p in outcomes.items():
        target: str | int = 'retry' if state in retry and state not in stop else state
        result[target] = result.get(target, F(0)) + p
    return result


class ConceptTests(unittest.TestCase):
    def test_01_shared_circuit_unequal_values(self):
        c = PairTemplate()
        self.assertEqual(c.evaluate((3,2,1)), F(7,12))
        self.assertEqual(c.evaluate((1,2,3)), F(3,20))

    def test_02_cold_oracle_grid(self):
        c = PairTemplate()
        for w in itertools.product(range(1,6), repeat=3):
            self.assertEqual(c.evaluate(w), cold_pair(w, frozenset((0,1))), w)

    def test_03_complete_pair_mass(self):
        for w in itertools.product(range(1,6), repeat=3):
            self.assertEqual(sum((cold_pair(w, frozenset(pair))
                                 for pair in itertools.combinations(range(3),2)), F(0)), F(1))

    def test_04_whole_instance_relabeling(self):
        old=(3,2,1)
        for perm in itertools.permutations(range(3)):
            w=tuple(old[i] for i in perm)
            required=frozenset(perm.index(i) for i in (0,1))
            self.assertEqual(cold_pair(old,frozenset((0,1))),cold_pair(w,required))

    def test_05_fixed_asymmetric_count_not_equivalence(self):
        old=(3,2,1)
        self.assertNotEqual(cold_pair(old,frozenset((0,1))),cold_pair(old,frozenset((1,2))))

    def test_06_coefficient_contexts_are_separate(self):
        c=PairTemplate(); topology=c.nodes
        a=c.evaluate((3,2,1)); b=c.evaluate((1,2,3)); a_again=c.evaluate((3,2,1))
        self.assertEqual(a,a_again); self.assertNotEqual(a,b); self.assertIs(topology,c.nodes)

    def test_07_positive_region_rejects_support_change(self):
        with self.assertRaises(ValueError): PairTemplate().evaluate((3,2,0))
        self.assertEqual(cold_pair((3,2,0),frozenset((0,1))),1)

    def test_08_zero_escape_is_not_a_cached_proper_result(self):
        self.assertEqual(retry_value(F(1),F(9,10)),10)
        with self.assertRaises(ValueError): retry_value(F(1),F(1))

    def test_09_optimal_action_depends_on_parameters(self):
        choose=lambda p: 'retry' if F(1)/p < 20 else 'finish'
        self.assertEqual(choose(F(1,5)),'retry')
        self.assertEqual(choose(F(1,100)),'finish')

    def test_10_equal_topology_not_equal_reward(self):
        self.assertEqual(retry_value(F(1),F(1,2)),2)
        self.assertEqual(retry_value(F(3),F(1,2)),6)

    def test_11_equal_target_weight_not_equal_pool_probability(self):
        self.assertNotEqual(F(100,1000),F(100,5000))

    def test_12_ragged_uniform_segments_sum(self):
        for segment in ((2,3),(1,1,3),(5,), (1,1,1,1,1)):
            self.assertEqual(sum((F(w,15) for w in segment),F(0)),F(sum(segment),15))

    def test_13_hidden_member_observer_breaks_aggregation(self):
        values=(0,100)
        result=lambda weights: sum((F(w,sum(weights))*v for w,v in zip(weights,values)),F(0))
        self.assertEqual(result((1,9)),90); self.assertEqual(result((9,1)),10)

    def test_14_goal_status_histograms_not_universal_values(self):
        status_a=('sat','sat','missing'); status_b=('sat','missing','sat')
        self.assertEqual(sorted(status_a),sorted(status_b))
        costs={'A':2,'B':5,'C':100}
        self.assertNotEqual(costs['C'],costs['B'])

    def test_15_inverse_role_map(self):
        binding={'held0':'physical-B','held1':'physical-C','missing':'physical-A'}
        self.assertEqual(binding['missing'],'physical-A')
        rebound={'held0':'physical-A','held1':'physical-B','missing':'physical-C'}
        self.assertNotEqual(binding['missing'],rebound['missing'])

    def test_16_resource_labels_are_not_interchangeable(self):
        resources={'craft-A':F(2),'craft-B':F(1)}
        mapping={'craft-A':'craft-B','craft-B':'craft-A'}
        mapped={mapping[k]:v for k,v in resources.items()}
        self.assertEqual(sum(resources.values()),sum(mapped.values()))
        self.assertNotEqual(resources,mapped)

    def test_17_stop_extension_outside_retry_is_invariant(self):
        p={0:F(1,2),1:F(1,10),2:F(1,10),3:F(3,10)}
        self.assertEqual(normalize(p,{0,1},{2}),normalize(p,{0,1},{2,3}))

    def test_18_stop_extension_inside_retry_changes_law(self):
        p={0:F(1,2),1:F(1,10),2:F(1,10),3:F(3,10)}
        self.assertNotEqual(normalize(p,{0,1},{2}),normalize(p,{0,1},{1,2}))

    def test_19_stop_intersection_lemma_exhaustive(self):
        universe=set(range(4)); p={i:F(1,4) for i in universe}
        subsets=[{i for i in universe if mask&(1<<i)} for mask in range(16)]
        for r in subsets:
            for t1 in subsets:
                for t2 in subsets:
                    if r&t1==r&t2:
                        self.assertEqual(normalize(p,r,t1),normalize(p,r,t2))

    def test_20_joint_goal_hits_not_sum_of_marginals(self):
        joint={'none':F(1,4),'A':F(1,4),'B':F(1,4),'AB':F(1,4)}
        union=1-joint['none']
        naive=(joint['A']+joint['AB'])+(joint['B']+joint['AB'])
        self.assertEqual(union,F(3,4)); self.assertEqual(naive,1)

    def test_21_partial_mass_is_not_a_distribution(self):
        known=F(9,10); missing=F(1,10)
        self.assertNotEqual(known,1); self.assertEqual(known+missing,1)

    def test_22_local_termination_not_global_properness(self):
        # Each deterministic fragment exits in one step; A -> B -> A never reaches goal.
        s='A'
        for _ in range(100): s={'A':'B','B':'A'}[s]
        self.assertEqual(s,'A'); self.assertNotEqual(s,'goal')

    def test_23_memory_overlap_not_individual_max(self):
        parent,template,binding,output,scratch=100,40,30,20,50
        actual=parent+template+binding+output+scratch
        self.assertEqual(actual,240); self.assertGreater(actual,200)
        self.assertLess(max(parent,template+binding+output+scratch),200)

    def test_24_break_even_includes_matching_and_outputs(self):
        n=4
        cold=n*(10+20+30)
        shared=15+n*(2+20+30)
        self.assertEqual(cold,240); self.assertEqual(shared,223)
        expensive_matching=15+n*(15+20+30)
        self.assertGreater(expensive_matching,cold)

    def test_25_work_debit_is_not_refunded_by_eviction(self):
        debit=20; live=10
        live-=10
        self.assertEqual(live,0); self.assertEqual(debit,20)

    def test_26_witness_identity_keeps_graph_and_value_paired(self):
        verified={'graph-A':F(10),'graph-B':F(7)}
        selected=min(verified,key=verified.get)
        self.assertEqual((selected,verified[selected]),('graph-B',F(7)))
        del verified['graph-B']
        self.assertNotIn('graph-B',verified)

    def test_27_empty_suffix_requirement_does_not_make_junk_goal(self):
        # This illustrates the explicitly stated current clean-terminal contract.
        terminal=lambda sat,total,required: sat>=required and sat==total
        self.assertTrue(terminal(3,3,3)); self.assertFalse(terminal(3,4,3))

    def test_28_template_is_not_a_state_count_reduction(self):
        c=PairTemplate(); instances=[(1,2,3),(2,2,3),(3,2,3)]
        outputs=[c.evaluate(w) for w in instances]
        self.assertEqual(len(set(outputs)),3)
        self.assertEqual(len(instances),3)


class RecordingResult(unittest.TextTestResult):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs); self.passed=[]
    def addSuccess(self,test):
        super().addSuccess(test); self.passed.append(test.id())


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).with_name('results.json'))
    args=parser.parse_args()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ConceptTests)
    result=unittest.TextTestRunner(verbosity=1,resultclass=RecordingResult).run(suite)
    baseline=json.loads((Path(__file__).resolve().parents[1]/'evidence/baseline.json').read_text())
    a=baseline['a4']['baseline']; m=baseline['a4']['same_attempt_memo']
    saved_ns=a['protected_attempt_ns']-m['protected_attempt_ns']
    report={
        'scope':'Exact-rational arithmetic and finite abstract specification examples; not native PoE qualification or a performance profile',
        'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
        'passed_test_ids':result.passed,
        'subcase_note':'Tests 02 and 03 each iterate 125 positive weight vectors; test 19 enumerates finite subsets. These are not separately counted native tests.',
        'illustrative_outputs':{'pair_3_2_1':str(PairTemplate().evaluate((3,2,1))), 'pair_1_2_3':str(PairTemplate().evaluate((1,2,3)))},
        'derived_from_recorded_receipt':{'protected_attempt_saved_ns':saved_ns, 'protected_attempt_saved_percent':100*saved_ns/a['protected_attempt_ns'], 'baseline_attempt_share_of_recorded_total_percent':100*(a['protected_attempt_ns']/1e6)/a['total_wall_ms'], 'caveat':'Recorded times were not remeasured; stage and all-in savings are different quantities.'},
        'native_build_or_run_performed':False,
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if result.wasSuccessful() else 1

if __name__=='__main__':
    raise SystemExit(main())
