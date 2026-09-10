"""Native-flag adapter around the existing corpus runner and isolated worker.

No solver mechanics, evaluator, scheduling or report classification is replaced.
The corpus CLI does not expose the native-only retention diagnostic flag.
"""
from dataclasses import replace
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'out/2026-09-09-cross-base-capability-recovery'
sys.path.insert(0, str(ROOT / 'tools/ingest'))
from poecraft_ingest import solver_corpus_runner as runner
from poecraft_ingest.solver_worker import sha256_file

def main():
    label, cohort, executable, *case_ids = sys.argv[1:]
    output = OUT / label
    output.mkdir(parents=True, exist_ok=False)
    original_resolve = runner.resolve_case_execution
    original_run = runner.run_isolated_process
    def resolve(*args, **kwargs):
        resolved = original_resolve(*args, **kwargs)
        resolved = replace(resolved, watchdog_seconds=315,
            command=replace(resolved.command, argv=resolved.command.argv + ('--native-retention-diagnostic', 'reuse')))
        (output / (resolved.case_id + '.command.json')).write_text(json.dumps(
            resolved.command.canonical_document(host_watchdog_seconds=315,
                reservation=resolved.reservation.as_dict()), indent=2), encoding='utf-8')
        return resolved
    def isolated(*args, **kwargs):
        kwargs['cancel_requested'] = lambda: (OUT / 'cancel').exists()
        def started(pid, token):
            (output / 'active-process.json').write_text(json.dumps({'process_id': pid,
                'process_identity_token': token}), encoding='utf-8')
        kwargs['on_started'] = started
        try:
            return original_run(*args, **kwargs)
        finally:
            (output / 'active-process.json').unlink(missing_ok=True)
    runner.resolve_case_execution = resolve
    runner.run_isolated_process = isolated
    (output / 'adapter-identity.json').write_text(json.dumps({'sha256': sha256_file(Path(__file__)),
        'native_arguments': ['--native-retention-diagnostic', 'reuse'], 'proof_handoff_seconds': 0,
        'native_watchdog_seconds': 300, 'outer_cleanup_seconds': 315}, indent=2), encoding='utf-8')
    args = ['--root', str(ROOT), '--executable', executable, '--artifact', str(ROOT / 'data/compiled/current'),
        '--corpus', str(HERE / cohort / 'manifest.json'), '--output', str(output), '--max-workers', '1']
    for case_id in case_ids:
        args += ['--case', case_id]
    return runner.main(args)

if __name__ == '__main__':
    raise SystemExit(main())
