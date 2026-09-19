import json,hashlib
from pathlib import Path
root=Path.cwd();out=root/'out/verified-delivery';record=root/'docs/active/2026-09-15-verified-delivery'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text(encoding='utf-8'))
rows=[]
for name in ['A7-manual-conquest-1','A7-default-conquest-1','A7-default-conquest-2','A7-manual-conquest-2','A7-default-conquest-four','A7-regalia']:
 p=out/(name+'.json');c=load(p)['cases'][0];m={v['stage']:v['worker_elapsed_ms'] for v in c['worker_milestones']};d=c['delivery_control'];e=c['exact_evaluation'];ev={v['sequence']:v for t in c['progress_trace'] for v in (t.get('trace') or {}).get('events',[])}
 first=next(t['trace']['current'] for t in c['progress_trace'] if t.get('trace'))
 usable=c['phase_wall_ms']['solve']+c['phase_wall_ms']['compile'];intent=d['finish_intent_host_ms']
 rows.append({'name':name,'report':str(p.relative_to(root)),'report_sha256':sha(p),'status':c['actual_status'],'expectations_met':c['expectation_met'],'cost':c['solve_summary']['upper_bound'],'expected_primitive_actions':e['result']['expected_actions'],'independent_evaluation':{k:v for k,v in e.items() if k!='result'},'prepared_graph_sha256':c['prepared_strategy']['sha256'],'phase_wall_ms':c['phase_wall_ms'],'worker_milestones':m,'delivery':d,'native_export_from_worker_start_ms':m['result_export_completed'],'finish_intent_to_worker_response_host_ms':None if intent is None else c['phase_wall_ms']['solve']-intent,'usable_graph_timestamp':'not separately instrumented in headless benchmark; see actual Calculator milestones','setup_goal_cover_ms':first['setup_goal_cover_ns']/1e6,'setup_retention_ms':first['setup_retention_ns']/1e6,'events':[v for v in ev.values() if v['kind'] in ['finish_acknowledged','selection_sealed','native_done','check_completed','incumbent_retained']],'errors':c['errors']})
native=[]
refs=load(root/'docs/active/2026-09-14-progress-and-delivery/retained-policies.json')['policies']
for name,refname in [('A7-ring','ring'),('A7-bow','bow'),('A7-amulet','amulet'),('B-conquest-four','conquest-four'),('A7-conquest-four','conquest-four')]:
 paths=list((out/name/'cases').glob('*.json'))
 if not paths:continue
 p=paths[0];c=load(p)['cases'][0];e=c['exact_strategy_evaluation'];g=Path(c['compiled_graph']['strategy_output_path']);ref=next(v for v in refs if v['name']==refname)
 native.append({'name':name,'report':str(p.relative_to(root)),'report_sha256':sha(p),'status':c['actual_status'],'expectations_met':c['expectation_met'],'cost':c['solve_summary']['upper_bound'],'expected_primitive_actions':e['result']['expected_actions'],'independent_evaluation':{k:v for k,v in e.items() if k!='result'},'graph_sha256':sha(g),'reference_graph_unchanged':sha(g)==ref['strategy_sha256'],'reference':ref['strategy'],'phase_wall_ms':c['phase_wall_ms'],'errors':c['errors']})
ui=[]
for name in ['A7-calculator-finish','A7-calculator-cancel-setup']:
 p=out/(name+'.json');c=load(p);t=c['trace'];m={v['stage']:v['ui_elapsed_ms'] for v in t['ui_milestones']}
 row={'name':name,'report':str(p.relative_to(root)),'report_sha256':sha(p),'environment':c['environment'],'visual_review':c['visual_review'],'status':t['status'],'ui_milestones':m,'worker':t['worker'],'usable_strategy':c['usable_strategy'],'graph_sha256':c['graph_sha256'],'error':c['error']}
 if name.endswith('finish'):
  graph=c['graph'];graph=json.loads(graph) if isinstance(graph,str) else graph;graph=dict(graph);graph.pop('economy',None)
  matched=load(out/'A7-manual-conquest-1.json')['cases'][0]
  assert graph==matched['prepared_strategy']['document']
  goal=dict(c['resolved']['goal']);actions=goal.pop('actions');assert goal==matched['input']['goal'];assert actions==matched['product_action_ids']
  economy=matched['input']['economy'];prices=load(root/economy['snapshot_path'])['prices'] | economy['manual_overrides']
  for key,value in prices.items():assert c['request']['economy']['snapshot']['prices'][key]==value
  row['complete_graph_equal_to_independently_evaluated_manual_arm_excluding_economy_identity']=True
  row['finish_intent_to_ui_ready_ms']=m['ui_delivery_completed']-m['finish_intent']
 else:row['cancel_intent_to_ui_release_ms']=m['ui_delivery_completed']-m['cancel_intent']
 ui.append(row)
result={'base':'4594e44b6b851511ae5471e52f232bce57d16856','build':'build-A7.json','worker_processes':'A7-worker-processes.json','worker':rows,'calculator':ui,'native':native,'rendered_UI_review':'not run; Oliver owns visual review','simulation':'unchanged retained graphs not resimulated','timing_interpretation':'Measured host windows; no latency theorem or numerical-authority change. A5 and A7 setup spans differ, so cross-build wall-time differences are not isolated algorithm effects.'}
(record/'qualification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'worker_cases':len(rows),'native_cases':len(native),'calculator_cases':len(ui)}))
