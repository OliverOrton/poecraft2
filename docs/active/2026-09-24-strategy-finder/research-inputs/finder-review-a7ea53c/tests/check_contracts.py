"""Abstract specifications only: does not import, compile or emulate PoE mechanics.

The first test demonstrates a counterexample to raw JSON guard validation.
It passing means that the counterexample is reproduced in this small model,
NOT that the current native implementation passes its safety requirement.
"""
from __future__ import annotations
import argparse
import json
import unittest
from fractions import Fraction
from pathlib import Path

G = {"type": "native_goal", "roles": ["A", "B"]}

def raw_ingress_check(edge: dict) -> bool:
    return edge.get("to") == "goal" and edge.get("condition") == G

def effective_guard(edge: dict) -> dict:
    return {"type": "always"} if edge.get("is_default", False) else edge["condition"]

def repaired_ingress_check(edge: dict) -> bool:
    return not edge.get("is_default", False) and effective_guard(edge) == G

class ContractTests(unittest.TestCase):
    def test_decorated_default_is_a_counterexample(self):
        edge={"to":"goal", "is_default":True, "condition":G}
        self.assertTrue(raw_ingress_check(edge))
        self.assertEqual(effective_guard(edge), {"type":"always"})
        self.assertFalse(repaired_ingress_check(edge))

    def test_real_guard_is_preserved(self):
        edge={"to":"goal", "condition":G}
        self.assertTrue(raw_ingress_check(edge))
        self.assertTrue(repaired_ingress_check(edge))

    def test_wrong_target_is_not_a_guard(self):
        self.assertFalse(repaired_ingress_check({"to":"goal", "condition":{"type":"always"}}))

    def test_progress_retention_example(self):
        p,q=Fraction(1,10),Fraction(1,5)
        joint=1/(p*q)
        staged=1/p+1/q
        self.assertEqual(joint,50)
        self.assertEqual(staged,15)
        self.assertLess(staged,joint)

    def test_role_similarity_does_not_equal_value(self):
        self.assertEqual(1/Fraction(1,5),5)
        self.assertEqual(1/Fraction(1,100),100)
        self.assertNotEqual(1/Fraction(1,5),1/Fraction(1,100))

    def test_transfer_peak_is_not_final_live(self):
        base,old,active,checker,newcopy,cap=100,20,30,40,30,205
        self.assertLessEqual(base+old+active+checker,cap)
        self.assertGreater(base+old+active+checker+newcopy,cap)
        self.assertLessEqual(base+newcopy,cap)

    def test_rank_changes_top_k_candidate_set(self):
        actions=[("a",100),("b",50),("c",1),("d",2)]
        price={x[0] for x in sorted(actions,key=lambda x:(x[1],x[0]))[:2]}
        identity={x[0] for x in sorted(actions)[:2]}
        self.assertNotEqual(price,identity)

    def test_live_frontier_is_not_lifetime_generation(self):
        width=2
        live=["parent"]
        generated={"parent"}
        live.pop(0)
        live.extend(["childA","childB"]);generated.update(live)
        self.assertLessEqual(len(live),width)
        self.assertGreater(len(generated),width)

    def test_closed_component_not_finite_occupancy(self):
        # d = incoming + d has no finite solution for positive incoming.
        incoming=Fraction(1,3)
        for d in (Fraction(0),Fraction(1),Fraction(100000)):
            self.assertNotEqual(d,incoming+d)

if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args=parser.parse_args()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    record={"scope":"abstract specifications; no native execution", "tests_run":result.testsRun,
            "failures":len(result.failures),"errors":len(result.errors),"passed":result.wasSuccessful(),
            "default_edge_test":"passing test reproduces an abstract bypass counterexample, not a native fix"}
    if args.output:
        args.output.write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
    raise SystemExit(0 if result.wasSuccessful() else 1)
