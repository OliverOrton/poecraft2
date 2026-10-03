from pathlib import Path
import sys,json,hashlib,datetime,subprocess,os
root=Path.cwd();out=root/'out/selective-cap-repair';sys.path.insert(0,r'C:\Users\Oliver\Documents\poecraft2\tools\ingest')
from poecraft_ingest.solver_worker import SolverCaseCommand,run_isolated_process
tag = sys.argv[1] if len(sys.argv) > 1 else 'production-wasm-r3'
law3 = tag.startswith('law3-')
qualification = 'law3-native-r2' if law3 else 'production-r5'
q=json.loads((out/f'{qualification}-qualification-result.json').read_text(encoding='utf-8'));assert all(r['exit_code']==0 and not r['timed_out'] and not r['survivor'] for r in q)
if law3:
 source=json.loads((out/f'{qualification}-qualification-request.json').read_text(encoding='utf-8'))
 for name,h in source['source_files'].items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==h,name
 replay='law3-conquest-solve-r1'
 row=json.loads((out/f'{replay}-runtime-result.json').read_text(encoding='utf-8'))[0]
 assert row['arm']=='conquest5' and row['receipt']['exit_code']==0 and not row['receipt']['survivor'] and row['exact']['status']=='matched' and row['exact']['cost_complete']
else:
 assert json.loads((out/'budget-identity-r3-tests.receipt.json').read_text())['exit_code']==0
 replay='production-fixed-bow4-r2'
 bow=json.loads((out/f'{replay}-runtime-result.json').read_text())[0];assert bow['receipt']['exit_code']==0 and not bow['receipt']['survivor'] and bow['exact']['status']=='matched' and bow['exact']['cost_complete']
paths=[p for p in (root/'engine').rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.h']]+[root/'engine/CMakeLists.txt',root/'bindings/wasm/wasm_api.cpp',root/'scripts/build-wasm.ps1']
a=['powershell','-NoProfile','-File','scripts/build-wasm.ps1'];os.environ['EMCC_CORES']='2'
request={'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'native_qualification':qualification,'qualified_replay':replay,'reporter_tests':qualification if law3 else 'budget-identity-r3','rare_reforge_count_law_version':3 if law3 else 2,'model_assumption':'owner-approved single-side ordinary-total 8:3:1; provisional game model' if law3 else None,'purpose':'matching release WASM for repaired native product default; preserve latest feature source','compiler_jobs':2,'source_files':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'memory_configuration':{'initial_bytes':134217728,'maximum_bytes':4294967296,'allow_growth':True},'command':SolverCaseCommand(tuple(a),root).canonical_document(host_watchdog_seconds=1200),'LOCAL_owner':'Sol6.1 serial WASM build'}
(out/f'{tag}-request.json').write_text(json.dumps(request,indent=2)+'\n',encoding='utf-8')
def started(pid,token):
 e={'pid':pid,'identity_token':token,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'watchdog_seconds':1200};(out/f'{tag}-started.json').write_text(json.dumps(e,indent=2)+'\n',encoding='utf-8');print(json.dumps({'started':e}),flush=True)
r=run_isolated_process(a,watchdog_seconds=1200,cwd=root,on_started=started);log=r.pop('output','');(out/f'{tag}.log').write_text(log,encoding='utf-8');(out/f'{tag}.receipt.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps({'receipt':r,'tail':log[-1800:]}),flush=True)
if r['exit_code']==0 and not r['survivor']:
 identity={str(p.relative_to(root)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in [root/'bindings/wasm/dist/poecraft_engine.wasm',root/'bindings/wasm/dist/poecraft_engine.mjs']};(out/f'{tag}-identity.json').write_text(json.dumps(identity,indent=2)+'\n',encoding='utf-8');print(json.dumps({'wasm_identity':identity}),flush=True)
