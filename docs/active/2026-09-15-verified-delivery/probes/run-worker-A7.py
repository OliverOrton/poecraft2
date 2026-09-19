"""Selected worker qualification using existing supervision and benchmark owners."""
import ctypes, hashlib, json
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path
from poecraft_ingest.solver_worker import run_isolated_process
root=Path.cwd(); record=root/'docs/active/2026-09-15-verified-delivery'; out=root/'out/verified-delivery'
build=json.loads((record/'build-A7.json').read_text(encoding='utf-8'))
for path,digest in build['wasm'].items(): assert hashlib.sha256((root/path).read_bytes()).hexdigest()==digest
class MemoryStatus(ctypes.Structure):
    _fields_=[('length',wintypes.DWORD),('load',wintypes.DWORD)]+[(n,ctypes.c_ulonglong) for n in ['total_physical','available_physical','total_page','available_page','total_virtual','available_virtual','extended_virtual']]
corpus=str(root/'docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json')
jobs=[]
# Counterbalanced delivery pairs, then affected public and actual Calculator controls.
for name,case,manual in [
 ('A7-manual-conquest-1','cb01-cross-base-product8-long240',True),
 ('A7-default-conquest-1','cb01-cross-base-product8-long240',False),
 ('A7-default-conquest-2','cb01-cross-base-product8-long240',False),
 ('A7-manual-conquest-2','cb01-cross-base-product8-long240',True),
 ('A7-default-conquest-four','cb02-cross-base-product8-long240',False),
 ('A7-regalia','cb12-cross-base-product8-long240',False)]:
 args=['test/solver-benchmark.ts','--corpus',corpus,'--case',case,'--skip-verification','--output',str(out/(name+'.json'))]
 if manual:args.append('--finish-at-first-verified')
 jobs.append((name,args))
for name,control in [('A7-calculator-finish','finish'),('A7-calculator-cancel-setup','cancel_setup')]:
 jobs.append((name,['test/calculator-delivery-probe.ts','cb01-cross-base-product8-long240',str(out/(name+'.json')),control]))

processes=[]
for name,args in jobs:
    status=MemoryStatus();status.length=ctypes.sizeof(status);assert ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status))
    outside=max(8*1024**3,(status.total_physical+4)//5);reserved=8*1024**3
    admission={'utc':datetime.now(timezone.utc).isoformat(),'available_physical':status.available_physical,'available_commit':status.available_page,'outside_bytes':outside,'reserved_bytes':reserved,'admitted':status.available_physical>=outside+reserved and status.available_page>=reserved}
    assert admission['admitted'],admission
    command=['C:/Program Files/nodejs/node.exe','--import','tsx',*args]
    row={'name':name,'command':command,'watchdog_seconds':315,'admission':admission,'build':'build-A7.json'}
    processes.append(row);(record/'A7-worker-processes.json').write_text(json.dumps(processes,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'begin':name}),flush=True)
    result=run_isolated_process(command,watchdog_seconds=315,cwd=root/'apps/web')
    (out/(name+'.log')).write_text(result.pop('output'),encoding='utf-8',newline='\n');row['process']=result
    (record/'A7-worker-processes.json').write_text(json.dumps(processes,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'finished':name,**result}),flush=True)
    if result['timed_out'] or result['survivor'] or not (out/(name+'.json')).is_file(): raise RuntimeError('Worker qualification failed: '+name)
