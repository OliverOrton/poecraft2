"""Conditional projected-kernel research; no full medium action authority."""
from pathlib import Path
from fractions import Fraction as F
import json
from free_value_fixtures import Row,lp_simplex,valid

path=Path(__file__).parent
x=json.loads((path/'native-medium.json').read_text())
mass=sum(F(e['p']) for e in x['exits'])
root_floor=F(x['root_lower'])
macro=Row('paid_ichor_then_exalt',F(x['price']),tuple((str(e['state']),F(e['p'])/mass) for e in x['exits']))
placeholder=Row('all_other_caller_actions_conservative_family',root_floor,(('sink',F(1)),))
model={'root':[macro,placeholder]}
records=[]
for version in ['baseline','reanchored']:
    boundary={str(e['state']):F(e[version+'_lower']) for e in x['exits']}|{'sink':F(0)}
    value,pivots=lp_simplex(model,boundary)
    local_rhs=macro.cost+sum(p*boundary[t] for t,p in macro.successors)
    assert valid(model,boundary,value)
    records.append(dict(version=version,program_rhs=local_rhs,program_rhs_float=float(local_rhs),
        covered_finite_model=value['root'],covered_finite_model_float=float(value['root']),
        published_independent_portfolio_max=max(root_floor,value['root']),
        published_independent_portfolio_gain=max(root_floor,value['root'])-root_floor,
        root_existing_lower_is_subsolution_for_this_model=valid(model,boundary,{'root':root_floor}),
        simplex_pivots=pivots))
assert not records[0]['root_existing_lower_is_subsolution_for_this_model']
assert records[1]['root_existing_lower_is_subsolution_for_this_model']
assert all(r['published_independent_portfolio_gain']==0 for r in records)
out=dict(evidence='conditional projected reference; full medium canonical action coverage and strict pushforward proof unestablished',
    normalization='explicit exact division by sum of exported binary probability coefficients',
    raw_binary_mass_defect=mass-1,records=records,
    checked_reference_RHS_gain=records[1]['program_rhs']-records[0]['program_rhs'],
    exact_original_medium_oracle_gap=None,
    exact_native_policy_deviation_Q=None,
    production_authority=False)
(path/'medium-model-comparison.json').write_text(json.dumps(out,indent=2,default=str))
for r in records:print(r['version'],r['program_rhs_float'],r['covered_finite_model_float'],'global portfolio gain',float(r['published_independent_portfolio_gain']))
print('raw binary mass defect',float(out['raw_binary_mass_defect']))
