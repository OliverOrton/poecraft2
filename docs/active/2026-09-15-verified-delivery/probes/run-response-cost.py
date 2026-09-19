"""B's final saved-controller check, using the existing process supervisor."""
import ctypes
import hashlib
import json
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path
from poecraft_ingest.solver_worker import run_isolated_process

root = Path.cwd()
record = root / 'docs/active/2026-09-15-verified-delivery'
out = root / 'out/verified-delivery'
load = lambda p: json.loads(p.read_text(encoding='utf-8'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
case = load(next((out / 'A7-ring/cases').glob('*.json')))['cases'][0]
economy = case['input']['economy']
snapshot = load(root / economy['snapshot_path'])
snapshot['prices'].update(economy['manual_overrides'])
(out / 'B-economy.json').write_text(json.dumps(snapshot) + '\n', encoding='utf-8')
graphs = [
    'docs/active/2026-09-13-execution-aware-proposals/strategies/ring-four-count.strategy.json',
    'docs/active/2026-09-14-progress-and-delivery/strategies/ring-native.strategy.json',
]
class MemoryStatus(ctypes.Structure):
    _fields_ = [('length', wintypes.DWORD), ('load', wintypes.DWORD)] + [
        (n, ctypes.c_ulonglong) for n in ['total_physical', 'available_physical',
        'total_page', 'available_page', 'total_virtual', 'available_virtual', 'extended_virtual']]
status = MemoryStatus()
status.length = ctypes.sizeof(status)
assert ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status))
outside = max(8 * 1024**3, (status.total_physical + 4) // 5)
reserve = 8 * 1024**3
admission = dict(utc=datetime.now(timezone.utc).isoformat(),
    available_physical=status.available_physical, available_commit=status.available_page,
    outside_bytes=outside, reserved_bytes=reserve,
    admitted=status.available_physical >= outside + reserve and status.available_page >= reserve)
assert admission['admitted'], admission
exe = out / 'response-cost-probe.exe'
command = [str(exe), *graphs]
receipt = dict(command=command, build='build-A7.json', admission=admission,
    probe_source_sha256=sha(record / 'probes/response-cost-probe.cpp'),
    executable_sha256=sha(exe), library_sha256=sha(root / 'build/engine/libpoecraft_engine.a'),
    economy=dict(source=economy, resolved_sha256=sha(out / 'B-economy.json')),
    graphs={p: sha(root / p) for p in graphs}, watchdog_seconds=60)
process = run_isolated_process(command, watchdog_seconds=60, cwd=root)
output = process.pop('output')
(out / 'B-response-costs.log').write_text(output, encoding='utf-8')
receipt['process'] = process
if process['exit_code'] == 0 and not process['timed_out'] and not process['survivor']:
    receipt['result'] = json.loads(output)
    (out / 'B-response-costs.json').write_text(output, encoding='utf-8')
(record / 'B-response-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: receipt[k] for k in ['process', 'result'] if k in receipt}))
assert 'result' in receipt, process
