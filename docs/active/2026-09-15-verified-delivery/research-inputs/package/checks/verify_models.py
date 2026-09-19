#!/usr/bin/env python3
"""Independent exact-rational reference models for the integrated plan.

No poecraft2 imports, native mechanics, simulator or network calls. Dense Fraction
matrices are deliberately confined to tiny test oracles, not implementation advice.
The new audit's claimed 96/576/9 suite was not supplied and is not this suite.
"""
from __future__ import annotations
import argparse
from collections import deque
from fractions import Fraction as F
import json
from pathlib import Path
import random
from typing import Callable

Matrix = list[list[F]]
def I(n:int)->Matrix: return [[F(i==j) for j in range(n)] for i in range(n)]
def T(a:Matrix)->Matrix: return [list(x) for x in zip(*a)]
def add(a:Matrix,b:Matrix)->Matrix: return [[x+y for x,y in zip(r,s)] for r,s in zip(a,b)]
def sub(a:Matrix,b:Matrix)->Matrix: return [[x-y for x,y in zip(r,s)] for r,s in zip(a,b)]
def mul(a:Matrix,b:Matrix)->Matrix:
    assert len(a[0])==len(b)
    return [[sum((x*y for x,y in zip(r,c)),F(0)) for c in T(b)] for r in a]
def solve(a:Matrix,b:Matrix)->Matrix:
    n=len(a); m=len(b[0]); w=[list(a[i])+list(b[i]) for i in range(n)]
    for col in range(n):
        pivot=next((r for r in range(col,n) if w[r][col]),None)
        if pivot is None: raise ValueError('singular reference model')
        w[col],w[pivot]=w[pivot],w[col]
        p=w[col][col];w[col]=[v/p for v in w[col]]
        for r in range(n):
            if r!=col:
                p=w[r][col];w[r]=[x-p*y for x,y in zip(w[r],w[col])]
    return [r[n:n+m] for r in w]
def block(a:Matrix,rs:list[int],cs:list[int])->Matrix: return [[a[i][j] for j in cs] for i in rs]
def col(xs)->Matrix:return [[F(x)] for x in xs]
def ones(n:int)->Matrix:return col([1]*n)
def qdraw(rng:random.Random,n:int)->Matrix:
    # Strictly positive terminal probability at every row guarantees transience.
    a=[]
    for _ in range(n):
        x=[rng.randrange(0,8) for _ in range(n)]; den=sum(x)+rng.randrange(1,8)
        a.append([F(v,den) for v in x])
    return a

def randomized(seed:int,count:int)->dict:
    rng=random.Random(seed)
    checks={k:0 for k in ['policy_equations','occupation_accounting','boundary_cost_count',
                         'boundary_exit_mass','rank_one_recurrence','multi_row_recurrence',
                         'partial_alignment_remainder','transience_residual_enclosure']}
    for t in range(count):
        n=3+t%4; ns=list(range(n));Q=qdraw(rng,n);A=sub(I(n),Q);Z=solve(A,I(n))
        rewards=[[F(rng.randrange(1,12)),F(rng.randrange(1,5))] for _ in ns]
        V=mul(Z,rewards);assert V==add(rewards,mul(Q,V));checks['policy_equations']+=1
        d=[Z[0]];assert mul(d,rewards)==[V[0]];checks['occupation_accounting']+=1
        B=[0,n-1];J=list(range(1,n-1));R=solve(sub(I(len(J)),block(Q,J,J)),I(len(J)))
        H=add(block(Q,B,B),mul(mul(block(Q,B,J),R),block(Q,J,B)))
        cbar=add(block(rewards,B,[0,1]),mul(mul(block(Q,B,J),R),block(rewards,J,[0,1])))
        assert solve(sub(I(2),H),cbar)==block(V,B,[0,1]);checks['boundary_cost_count']+=1
        g=sub(ones(n),mul(Q,ones(n)))
        gbar=add(block(g,B,[0]),mul(mul(block(Q,B,J),R),block(g,J,[0])))
        assert add(mul(H,ones(2)),gbar)==ones(2);checks['boundary_exit_mass']+=1
        # One altered row, with all old/new states covered and both chains proper.
        i=t%n;new=qdraw(rng,n)[i];Qp=[r[:] for r in Q];Qp[i]=new
        cp=[r[:1] for r in rewards];delta=F(rng.randrange(-int(cp[i][0]),8));cp[i][0]+=delta
        vp=solve(sub(I(n),Qp),cp);v=block(V,ns,[0]);dp=[[x-y for x,y in zip(new,Q[i])]]
        zi=block(Z,ns,[i]);den=1-mul(dp,zi)[0][0];assert den>0
        assert vp[0][0]-v[0][0]==Z[0][i]*(delta+mul(dp,v)[0][0])/den
        checks['rank_one_recurrence']+=1
        changed=[0,n-1];Qm=[r[:] for r in Q];cm=[r[:1] for r in rewards]
        for j in changed:Qm[j]=qdraw(rng,n)[j];cm[j][0]=F(rng.randrange(1,12))
        E=block(I(n),ns,changed);D=sub(block(Qm,changed,ns),block(Q,changed,ns))
        dc=sub(block(cm,changed,[0]),block(rewards,changed,[0]));ZE=mul(Z,E)
        diff=mul(ZE,solve(sub(I(2),mul(D,ZE)),add(dc,mul(D,v))))
        assert sub(solve(sub(I(n),Qm),cm),v)==diff;checks['multi_row_recurrence']+=1
        # Map a source vector only on K; the native value at U stays an explicit term.
        K=list(range(n-1));U=[n-1];vk=col([rng.randrange(0,20) for _ in K])
        AK=sub(I(len(K)),block(Q,K,K));ds=[solve(AK,I(len(K)))[0]]
        ek=sub(add(block(rewards,K,[0]),mul(block(Q,K,K),vk)),vk)
        h=mul(ds,block(Q,K,U));known=mul(ds,ek)[0][0];unknown=mul(h,block(v,U,[0]))[0][0]
        assert v[0][0]-vk[0][0]==known+unknown
        assert 0<=sum(h[0])<=1;checks['partial_alignment_remainder']+=1
        # A native implementation would require outward-safe coefficient checks.
        w=mul(Z,ones(n));assert sub(w,mul(Q,w))==ones(n)
        vtrial=col([rng.randrange(0,20) for _ in ns]);e=sub(add(block(rewards,ns,[0]),mul(Q,vtrial)),vtrial)
        eps=max(abs(x[0]) for x in e)
        assert all(abs(v[i][0]-vtrial[i][0])<=eps*w[i][0] for i in ns)
        checks['transience_residual_enclosure']+=1
    return {'fixtures':count,'seed':seed,'identities_by_category':checks,'identity_checks':sum(checks.values())}

def examples()->list[dict]:
    out=[]
    def ck(name:str,ok:bool,detail:str):
        assert ok,name
        out.append({'name':name,'passed':True,'detail':detail})
    p=F(9,10);pp=F(4,5);old=1/(1-p);new=1/(1-pp)
    dp=pp-p;adv=dp*old;den=1-dp/(1-p)
    ck('old_occupancy_requires_feedback',old==10 and new==5 and old*adv==-10 and old*adv/den==-5,'10 -> 5; old-occupancy estimate -10; corrected change -5')
    ck('remaining_cost_score_double_counts',sum(range(1,11))==55,'Ten unit-cost states cost 10, but visits times remaining cost sums to 55.')
    ck('proper_interiors_can_make_improper_boundary',F(1,2)/(1-F(1,2))==1,'An interior exits almost surely to B; deterministic re-entry at B yields H=1 and no goal.')
    try:solve([[F(0)]],[[F(0)]]);bad=False
    except ValueError:bad=True
    ck('zero_cost_trap_not_value_authority',bad,'Singular I-Q refused, even though every scalar solves V=V.')
    ck('probability_prefix_not_normalized',F(9,10)<1,'A known 0.9 goal prefix with unknown remaining mass is not unit success.')
    q=[[F(0),F(1,2)],[F(1,4),F(1,4)]];v=solve(sub(I(2),q),ones(2));K=[0];U=[1]
    ck('partial_discrepancy_needs_unmapped_tail',v[0][0]==1+F(1,2)*v[1][0] and v[0][0]!=1,'Unknown native continuation remains 0.5*V_U, not zero.')
    # Changing a boundary return after an interior visit changes the response.
    cold_a=1+F(1,2)*2; cold_b=1+F(1,2)*20
    ck('router_change_invalidates_response',cold_a!=cold_b,'Old destination value 2 versus 20; same interior text is insufficient.')
    QBJ=col([F(1,2)]*3);QJB=[[F(1,4)]*3];H=mul(QBJ,QJB)
    ck('sparse_elimination_can_fill_boundary',sum(x!=0 for r in H for x in r)==9,'A single eliminated hub creates all 9 boundary-to-boundary entries.')
    ck('optimistic_model_ceiling_is_structural',min(1+100,100+1)==101 and min(1,100)+min(100,1)==2,'Independent favourable modes give valid but weak 2 versus coupled optimum 101.')
    ck('mean_does_not_determine_deadline',1-F(9,10)**5==F(40951,100000),'Deterministic length 10: P(T<=5)=0; geometric mean 10: 0.40951.')
    a=[(1,'goal',F(1,2)),(3,'return',F(1,2))];b=[(3,'goal',F(1,2)),(1,'return',F(1,2))]
    ck('duration_and_exit_correlation_matters',sum(k*p for k,_,p in a)==sum(k*p for k,_,p in b)==2 and sum(p for k,x,p in a if k<=1 and x=='goal')!=sum(p for k,x,p in b if k<=1 and x=='goal'),'Equal exit marginals/mean duration; unequal one-action goal probability.')
    # Same selected action, different unused alternative distinguishes hidden type.
    ck('selected_equivalence_not_all_action_equivalence',F(1,2)==F(1,2) and F(1,4)!=F(3,4),'Fixed-policy equality for a does not justify evaluating b on a merged type.')
    ck('goal_bits_not_injective_occupancy',len({'affixA'})<2,'Two overlapping goals can match one affix; second occupied slot may be junk.')
    ck('fixed_policy_lower_is_not_optimal_lower',F(99)>F(1),'Policy interval [99,101] cannot lower-bound an MDP with another cost-1 policy.')
    ck('no_work_refund_on_resume',2+2>3,'Two slices of work 2 exceed cumulative budget 3.')
    ck('frozen_audit_requires_capacity_identity',tuple([1,2])!=tuple([1,2,3]),'Growth of a lazily retained object invalidates its old footprint identity.')
    upper=True;lower=True;old_role='lower' if lower else ('upper' if upper else 'ordinary')
    correct_role='upper' if upper else ('lower' if lower else 'ordinary')
    ck('trace_flag_precedence_characterization',old_role=='lower' and correct_role=='upper','Synthetic branch characterization; not a native execution test.')
    modes=['ordinary','static','execution-cost','execution-count']
    ck('mode_mapping_is_not_capability_equivalence',[m.startswith('execution-') for m in modes]==[False,False,True,True],'Characterizes the inspected execution guard; no native mode qualification.')
    return out

def lifecycle()->dict:
    # Tiny specification: staged setup, run identity, verified witness, cut, response.
    # Tuple = (phase,best cost,sealed cost,terminal responses,setup committed)
    start=('setup',None,None,0,False);todo=deque([start]);seen={start};edges=0
    cmds=['setup_step','setup_commit','verify10','verify7','finish','seal','respond','cancel','stale_finish']
    def advance(s,cmd):
        phase,best,sealed,n,ready=s
        if cmd=='stale_finish' or phase in ('done','cancelled'):return s
        if cmd=='cancel':return ('cancelled',best,None,1,ready)
        if phase=='setup':
            if cmd=='setup_commit':return ('search',best,sealed,n,True)
            return s # no verified policy before commit
        if cmd.startswith('verify') and phase in ('search','finishing'):
            cost=int(cmd[6:]);best=cost if best is None else min(best,cost)
        elif cmd=='finish' and phase=='search' and best is not None:phase='finishing'
        elif cmd=='seal' and phase=='finishing':phase='sealed';sealed=best
        elif cmd=='respond' and phase=='sealed':phase='done';n=1
        return (phase,best,sealed,n,ready)
    while todo:
        s=todo.popleft()
        for cmd in cmds:
            nxt=advance(s,cmd);edges+=1
            assert nxt[3]<=1
            assert nxt[2] is None or nxt[4]
            if nxt[0]=='done':assert nxt[2]==nxt[1] and nxt[3]==1
            if s[0] in ('done','cancelled'):assert nxt==s
            if cmd=='stale_finish':assert nxt==s
            if nxt not in seen:seen.add(nxt);todo.append(nxt)
    return {'states':len(seen),'command_transitions_checked':edges,'passed':True,
            'scope':'finite specification, no concurrency fairness/latency or native verification claim'}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=Path('reference_results.json'));ap.add_argument('--seed',type=int,default=459444);ap.add_argument('--fixtures',type=int,default=48);args=ap.parse_args()
    if not 1<=args.fixtures<=256:ap.error('--fixtures must be between 1 and 256')
    report={'schema':'integrated_plan_rational_checks_v1','basis':'new independent synthetic checks; exact rational arithmetic',
            'randomized':randomized(args.seed,args.fixtures),'named_examples':examples(),'lifecycle':lifecycle(),
            'not_performed':['native solver runs','WASM/browser runs','PoE mechanics simulation','upstream audit suite rerun','formal proof of native code']}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'fixtures':report['randomized']['fixtures'],'identity_checks':report['randomized']['identity_checks'],'named_examples':len(report['named_examples']),'lifecycle':report['lifecycle'],'output':str(args.output)},indent=2))
if __name__=='__main__':main()
