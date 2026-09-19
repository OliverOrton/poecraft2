"""Programme adapter: immutable inputs; existing runner owns serial supervision."""
import ctypes, hashlib, json, subprocess, sys
from ctypes import wintypes
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path
from poecraft_ingest.solver_corpus_runner import load_case_tasks, run_corpus
from poecraft_ingest.solver_worker import capture_execution_provenance, resolve_case_execution, AttemptPaths

root=Path.cwd(); record=root/'docs/active/2026-09-15-verified-delivery'; out=root/'out/verified-delivery'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
jobs=json.loads(Path(sys.argv[1]).read_text())
prepared=[]
for job in jobs:
 binary=(root/job['binary']).resolve();manifest=(root/job['manifest']).resolve();dest=out/job['name']
 assert sha(binary)==job['binary_sha256'], 'Frozen executable changed'
 tasks=load_case_tasks(manifest,case_ids=set(job['cases'])); assert len(tasks)==len(job['cases'])
 preflight={'programme':'verified_delivery_A7','baseline_commit':job.get('source_commit'),
  'identity':asdict(capture_execution_provenance(root=root,executable=binary,artifact=root/'data/compiled/current',corpus=manifest)),
  'runtime_hashes':{str(p.relative_to(root)):sha(p) for p in [root/'bindings/wasm/dist/poecraft_engine.js',root/'bindings/wasm/dist/poecraft_engine.wasm',root/'data/compiled/current/game_data.bin',root/'data/compiled/current/strings.bin'] if p.exists()},
  'cases':[]}
 for task in tasks:
  resolved=resolve_case_execution(task,executable=binary,artifact=root/'data/compiled/current',corpus=manifest,root=root,
   paths=AttemptPaths.legacy(dest,task.case_id,'preflight'),exact_evaluation=True,run_verification=False,
   goal_progress_gated_reforges=False,watchdog_seconds=float(job['host_watchdog']),worker_headroom_bytes=6*1024**3,
   native_retention_diagnostic='reuse',native_dirty_guidance=job.get('mode'),native_execution_action_price=job.get('action_price'))
  preflight['cases'].append(asdict(resolved))
  prepared.append((job,binary,manifest,dest,task,asdict(resolved)['reservation']))
 (record/(job['name']+'-preflight.json')).write_text(json.dumps(preflight,indent=2,default=str)+'\n')

class MemoryStatus(ctypes.Structure):
 _fields_=[('length',wintypes.DWORD),('load',wintypes.DWORD)]+[(n,ctypes.c_ulonglong) for n in ['total_physical','available_physical','total_page','available_page','total_virtual','available_virtual','extended_virtual']]
for job,binary,manifest,dest,task,reservation in prepared:
 status=MemoryStatus();status.length=ctypes.sizeof(status)
 if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):raise ctypes.WinError()
 outside=max(8*1024**3,(status.total_physical+4)//5);reserved=reservation['solver_owned_cap_bytes']+reservation['worker_headroom_bytes']
 admission={'case':task.case_id,'utc':datetime.now(timezone.utc).isoformat(),'available_physical':status.available_physical,
  'available_commit':status.available_page,'reserved_bytes':reserved,'outside_bytes':outside,
  'admitted':status.available_physical>=reserved+outside and status.available_page>=reserved}
 (record/(job['name']+'-admission.json')).write_text(json.dumps(admission,indent=2)+'\n')
 if not admission['admitted']:raise RuntimeError('Host admission refused: '+task.case_id)
 print(json.dumps({'begin':job['name'],'case':task.case_id}),flush=True)
 ledger=run_corpus(root=root,executable=binary,artifact=root/'data/compiled/current',corpus=manifest,output_directory=dest,
  tasks=[task],max_workers=1,exact_evaluation=True,native_retention_diagnostic='reuse',native_dirty_guidance=job.get('mode'),
  native_execution_action_price=job.get('action_price'),host_watchdog_seconds=float(job['host_watchdog']),
  memory_budget_bytes=reserved,worker_headroom_bytes=6*1024**3)
 print(json.dumps({'finished':job['name'],'case':task.case_id,'all_completed':ledger['all_completed'],'survivors':ledger['survivors']}),flush=True)
 if ledger['survivors']:raise RuntimeError('Native survivor blocks subsequent timed job')
