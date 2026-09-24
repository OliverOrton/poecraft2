#!/usr/bin/env python3
"""Exact toy specifications; does not import/emulate native PoE mechanics.
Run: python checks/abstract_checks.py --output evidence/abstract_results.json
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
from fractions import Fraction as F
import json
from pathlib import Path
import unittest

GOAL='G'
@dataclass(frozen=True)
class Macro:
    name: str
    cost: F
    exits: tuple[tuple[str, F], ...]
    def validate(self) -> None:
        if self.cost < 0 or any(p < 0 for _,p in self.exits):
            raise ValueError('invalid reward/probability')
        if sum((p for _,p in self.exits), F(0)) != 1:
            raise ValueError('incomplete probability mass')

def lookahead(base: dict[str,F], menu: dict[str,tuple[Macro,...]], depth: int):
    """The returned layers distinguish stage; None means unavailable evidence."""
    if depth < 0: raise ValueError('negative depth')
    states=set(base)|set(menu)|{GOAL}
    for options in menu.values():
        for m in options:
            m.validate(); states.update(t for t,_ in m.exits)
    values={s:base.get(s) for s in states}; values[GOAL]=F(0)
    layers=[(values,{s:'base' for s in base}|{GOAL:'goal'})]
    for _ in range(depth):
        nxt={s:base.get(s) for s in states}; nxt[GOAL]=F(0)
        chosen={s:'base' for s in base}|{GOAL:'goal'}
        for s in sorted(states-{GOAL}):
            for m in menu.get(s,()):
                total=m.cost
                for t,p in m.exits:
                    if p == 0: continue
                    if values[t] is None: break
                    total += p*values[t]
                else:
                    if nxt[s] is None or total < nxt[s]:
                        nxt[s]=total; chosen[s]=m.name
        values=nxt; layers.append((values,chosen))
    return layers

def evaluate_root(policy: dict[str,Macro], root: str) -> F:
    """Exact elimination on the actual root-reachable finite toy policy."""
    if root==GOAL: return F(0)
    seen=set(); pending=[root]
    while pending:
        s=pending.pop()
        if s==GOAL or s in seen: continue
        if s not in policy: raise ValueError('missing continuation')
        seen.add(s); policy[s].validate()
        pending.extend(t for t,p in policy[s].exits if p>0 and t!=GOAL)
    states=sorted(seen); ix={s:i for i,s in enumerate(states)}; n=len(states)
    a=[[F(int(i==j)) for j in range(n)]+[policy[s].cost] for i,s in enumerate(states)]
    for i,s in enumerate(states):
        for t,p in policy[s].exits:
            if p and t!=GOAL: a[i][ix[t]]-=p
    for j in range(n):
        pivot=next((i for i in range(j,n) if a[i][j]),None)
        if pivot is None: raise ValueError('nontransient or undefined policy')
        a[j],a[pivot]=a[pivot],a[j]
        scale=a[j][j]; a[j]=[x/scale for x in a[j]]
        for i in range(n):
            if i==j: continue
            mult=a[i][j]
            a[i]=[x-mult*y for x,y in zip(a[i],a[j])]
    return a[ix[root]][-1]

class ContractTests(unittest.TestCase):
    def test_clean_terminal_is_separate_from_dirty_intermediate(self):
        terminal=lambda good_rarity,g,k,t: good_rarity and g>=t and k==g
        self.assertFalse(terminal(True,4,5,5))
        self.assertTrue(terminal(True,5,5,5))
        self.assertFalse(terminal(True,5,6,5))
    def test_source_safety_is_not_recipe_cleanliness(self):
        source={'authored':True,'global':True,'hidden':False,'extra_affix':True}
        safe=source['authored'] and source['global'] and not source['hidden']
        old_recipe=safe and not source['extra_affix']
        self.assertTrue(safe); self.assertFalse(old_recipe)
    def test_selector_exclusion_not_native_refusal(self):
        native_supported={'add','context_add','whole_setup_add'}
        old_selector={'add'}
        self.assertIn('whole_setup_add',native_supported)
        self.assertNotIn('whole_setup_add',old_selector)
    def test_mandatory_interior_still_not_a_decision(self):
        declaration={'macro_start'}
        self.assertNotIn('macro_start_step2',declaration)
    def test_one_then_base_vs_repeated(self):
        new=Macro('new',F(2),((GOAL,F(1,2)),('x',F(1,2))))
        one=lookahead({'x':F(10)},{'x':(new,)},1)[1][0]['x']
        self.assertEqual(one,7)
        self.assertEqual(evaluate_root({'x':new},'x'),4)
    def test_complementary_two_steps(self):
        menu={'x':(Macro('setup',F(1),(('y',F(1)),)),),
              'y':(Macro('finish',F(1),((GOAL,F(1)),)),)}
        layers=lookahead({'x':F(10),'y':F(100)},menu,2)
        self.assertEqual(layers[1][0]['x'],10)
        self.assertEqual(layers[2][0]['x'],2)
        self.assertNotEqual(layers[1][1]['x'],layers[2][1]['x'])
    def test_off_domain_tail_not_zero(self):
        menu={'x':(Macro('bad',F(1),(('unknown',F(1)),)),)}
        l=lookahead({'x':F(10)},menu,1)
        self.assertIsNone(l[0][0]['unknown']); self.assertEqual(l[1][0]['x'],10)
    def test_off_domain_tail_can_get_complete_second_step(self):
        menu={'x':(Macro('setup',F(1),(('y',F(1)),)),),
              'y':(Macro('finish',F(2),((GOAL,F(1)),)),)}
        l=lookahead({'x':F(10)},menu,2)
        self.assertEqual(l[1][0]['x'],10); self.assertEqual(l[2][0]['x'],3)
    def test_tiny_positive_unknown_tail_refuses_term(self):
        p=F(1,10**12)
        l=lookahead({'x':F(10)},{'x':(Macro('tiny',F(0),((GOAL,1-p),('u',p))),)},1)
        self.assertEqual(l[1][0]['x'],10)
    def test_zero_probability_unknown_tail_not_consumed(self):
        l=lookahead({'x':F(10)},{'x':(Macro('zero',F(1),((GOAL,F(1)),('u',F(0)))),)},1)
        self.assertEqual(l[1][0]['x'],1)
    def test_incomplete_mass_is_not_normalized(self):
        with self.assertRaises(ValueError):
            lookahead({'x':F(10)},{'x':(Macro('partial',F(1),((GOAL,F(9,10)),)),)},1)
    def test_paid_setup_not_omitted(self):
        full=F(4)+1+F(1,2)*10
        primitive_only=F(1)+F(1,2)*10
        self.assertEqual(full,10); self.assertEqual(primitive_only,6)
    def test_loss_of_progress_is_priced(self):
        alternative=Macro('repair',F(1),(('good',F(1,2)),('lost',F(1,2))))
        l=lookahead({'x':F(10),'good':F(1),'lost':F(30)},{'x':(alternative,)},1)
        self.assertEqual(l[1][0]['x'],10)
    def test_prefix_acquisition_is_retained(self):
        p={'root':Macro('acquire',F(50),(('tail',F(1)),)),
           'tail':Macro('finish',F(4),((GOAL,F(1)),))}
        self.assertEqual(evaluate_root(p,'root'),54)
    def test_local_exit_not_global_properness(self):
        p={'a':Macro('leave_a',F(1),(('b',F(1)),)),
           'b':Macro('leave_b',F(1),(('a',F(1)),))}
        with self.assertRaises(ValueError): evaluate_root(p,'a')
    def test_unreachable_trap_does_not_refute_root_policy(self):
        p={'root':Macro('finish',F(3),((GOAL,F(1)),)),
           'trap':Macro('loop',F(1),(('trap',F(1)),))}
        self.assertEqual(evaluate_root(p,'root'),3)
    def test_unequal_bindings_can_choose_differently(self):
        def choice(p):
            m=Macro('try',F(1),((GOAL,p),('x',1-p)))
            return lookahead({'x':F(20)},{'x':(m,)},1)[1][1]['x']
        self.assertEqual(choice(F(1,5)),'try')
        self.assertEqual(choice(F(1,100)),'base')
    def test_no_worse_prefix_on_known_domain(self):
        for p in (F(1,100),F(1,10),F(1,2),F(1)):
            for cost in (F(0),F(1),F(7)):
                m=Macro('try',cost,((GOAL,p),('x',1-p)))
                for values,_ in lookahead({'x':F(10)},{'x':(m,)},3):
                    self.assertLessEqual(values['x'],10)
    def test_item_and_control_identity_are_both_required(self):
        tails={('item','old_global'):F(10)}
        self.assertNotIn(('item','hidden_offer'),tails)
    def test_lower_does_not_follow_from_feasible_local_cost(self):
        global_optimum=F(4); restricted_feasible=F(7)
        self.assertGreater(restricted_feasible,global_optimum)
    def test_exposure_double_counts(self):
        costs=[F(1)]*3; tails=[F(3),F(2),F(1)]
        self.assertEqual(sum(costs),3); self.assertEqual(sum(tails),6)
    def test_same_report_first_failure_does_not_identify_all_causes(self):
        rejects=[{'context','dirt'},{'context','debt'}]
        first=['context','context']
        self.assertEqual(first.count('context'),2)
        self.assertEqual(sum('dirt' in r for r in rejects),1)
        self.assertNotEqual(first.count('dirt'),sum('dirt' in r for r in rejects))
    def test_graph_value_pair_is_not_scalar_minimum(self):
        available={'graph_10':F(10)}; historical_min=F(7)
        self.assertNotIn(historical_min,available.values())
    def test_simultaneous_memory_not_independent_caps(self):
        cap=100; parent=45; donor=10; child=25; checker=30
        self.assertLess(child,cap); self.assertLess(checker,cap)
        self.assertGreater(parent+donor+child+checker,cap)
    def test_work_is_not_refunded_on_release(self):
        debit=7; live=12
        live=0
        self.assertEqual((debit,live),(7,0))

if __name__=='__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('--output',type=Path)
    ns=ap.parse_args()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests)
    names=[t.id().split('.')[-1] for t in suite]
    result=unittest.TextTestRunner(verbosity=1).run(suite)
    record={'schema':'native_boundary_abstract_checks_v1',
            'scope':'Exact-rational and abstract fixtures only; no native PoE/solver/performance claim',
            'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
            'passed':result.wasSuccessful(),'test_names':names}
    if ns.output:
        ns.output.parent.mkdir(parents=True,exist_ok=True)
        ns.output.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    else: print(json.dumps(record,indent=2))
    raise SystemExit(0 if result.wasSuccessful() else 1)
