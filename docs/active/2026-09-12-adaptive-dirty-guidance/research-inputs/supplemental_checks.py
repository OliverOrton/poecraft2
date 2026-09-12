#!/usr/bin/env python3
"""Exact synthetic checks; not PoE execution, a general proof or a speed result."""
import argparse, json
from fractions import Fraction as F
from pathlib import Path

def main():
    checks={}
    def ck(name,condition):
        checks[name]=bool(condition)
        if not condition: raise AssertionError(name)
    old=F(1)/(1-F(99,100))
    new=F(2)/(1-F(9,10))
    a=F(2)+F(9,10)*old-old
    d_old=1/(1-F(99,100)); d_new=1/(1-F(9,10))
    ck('old_fixed_policy_value', old==100)
    ck('new_fixed_policy_value', new==20)
    ck('first_action_advantage',a==-8)
    ck('new_occupancy_policy_difference',d_new*a==new-old==-80)
    ck('old_occupancy_is_not_exact_saving',d_old*a==-800 and d_old*a!=new-old)
    ck('perfect_restricted_ranking_cannot_choose_missing_action',min([old])==100 and min([old,new])==20)
    ck('higher_immediate_cost_can_reduce_expected_cost',F(2)>F(1) and new<old)
    ck('expensive_protection_can_also_lose',F(20)/(1-F(9,10))==200>old)
    ck('unrelated_root_upper_not_entry_upper',F(1)<F(100))
    lo=F(174); estimate=F(2000000); upper=F(1973848)
    ck('estimate_may_exceed_verified_upper_without_changing_endpoints',estimate>upper and lo<upper)
    # Different phases: a zero-cost two-controller handoff cycle cannot be
    # called proper merely because each handoff exits its local component.
    ck('local_exits_do_not_supply_goal_absorption',1-F(1)*F(1)==0)
    return {'classification':'synthetic exact rational checks only',
            'number_of_checks':len(checks),'checks':checks,
            'old_J':str(old),'new_J':str(new),'A':str(a),
            'old_occupancy_prediction':str(d_old*a),'actual_change':str(new-old)}
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path);args=ap.parse_args()
    result=main();text=json.dumps(result,indent=2)+'\n'
    if args.output: args.output.write_text(text,encoding='utf-8')
    print(json.dumps({'passed':result['number_of_checks']}))
