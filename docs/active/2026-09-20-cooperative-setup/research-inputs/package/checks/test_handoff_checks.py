#!/usr/bin/env python3
"""Diagnostic-tool tests and abstract examples, NOT native poecraft2 qualification."""
from __future__ import annotations
import argparse
import io
import json
import subprocess
import sys
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path
from inspect_runtime_identity import inspect, fingerprint, field_differences, load_object

class IdentityTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.artifact = self.root / 'artifact'
        self.artifact.mkdir()
        for name in ('game-data.json','strings.json'):
            (self.artifact/name).write_bytes(b'{"fixture":true}\n')
        self.manifest = {'generated_at_utc':'2026-08-30T02:54:53Z',
            'source':{'data_hash':'synthetic-not-native'},
            'files':{n:fingerprint(self.artifact/n) for n in ('game-data.json','strings.json')}}
        self.save_manifest()
        self.reference = self.root/'reference.json'
        self.reference.write_bytes((self.artifact/'manifest.json').read_bytes())
        self.lock = self.root/'lock.json'
        self.lock.write_text(json.dumps({'runtime_artifact':{
            'generated_at_utc': self.manifest['generated_at_utc'],
            'manifest_sha256':fingerprint(self.reference)['sha256'],
            'files':self.manifest['files']}}),encoding='utf-8')
    def save_manifest(self) -> None:
        (self.artifact/'manifest.json').write_bytes((json.dumps(self.manifest,indent=2,sort_keys=True)+'\n').encode())
    def report(self) -> dict:
        return inspect(self.lock,self.artifact,self.reference)
    def test_exact_positive(self):
        r=self.report();self.assertTrue(r['exact_pinned_runtime_matches']);self.assertEqual(r['reference_field_difference_count'],0)
    def test_timestamp_only_localized_not_blessed(self):
        self.manifest['generated_at_utc']='08/30/2026 02:54:53';self.save_manifest()
        r=self.report();self.assertFalse(r['exact_pinned_runtime_matches'])
        self.assertTrue(all(p['matches_lock'] for p in r['payloads'].values()))
        self.assertEqual([d['pointer'] for d in r['reference_field_differences']],['/generated_at_utc'])
    def test_payload_tampering_refuses(self):
        (self.artifact/'strings.json').write_bytes(b'{}\n')
        r=self.report();self.assertFalse(r['exact_pinned_runtime_matches']);self.assertFalse(r['payloads']['strings.json']['matches_lock'])
    def test_raw_whitespace_diff_not_semantic_equality(self):
        (self.artifact/'manifest.json').write_text(json.dumps(self.manifest),encoding='utf-8')
        r=self.report();self.assertFalse(r['exact_pinned_runtime_matches']);self.assertEqual(r['reference_field_difference_count'],0)
    def test_missing_payload_is_unavailable(self):
        (self.artifact/'game-data.json').unlink()
        with self.assertRaises(FileNotFoundError):self.report()
    def test_duplicate_and_nonfinite_json_refuse(self):
        for text in ('{"a":1,"a":2}','{"a":NaN}'):
            self.lock.write_text(text,encoding='utf-8')
            with self.assertRaises(ValueError):load_object(self.lock)
    def test_wrong_timestamp_type_refuses(self):
        lock=load_object(self.lock);lock['runtime_artifact']['generated_at_utc']=True
        self.lock.write_text(json.dumps(lock),encoding='utf-8')
        with self.assertRaises(ValueError):self.report()
    def test_unexpected_payload_name_refuses(self):
        lock=load_object(self.lock);lock['runtime_artifact']['files']['../foreign']={}
        self.lock.write_text(json.dumps(lock),encoding='utf-8')
        with self.assertRaises(ValueError):self.report()
    def test_boolean_not_integer_size(self):
        lock=load_object(self.lock);lock['runtime_artifact']['files']['strings.json']['byte_size']=True
        self.lock.write_text(json.dumps(lock),encoding='utf-8')
        with self.assertRaises(ValueError):self.report()
    def test_pointer_escaping_and_missing_values(self):
        d=field_differences({'a/b':None,'x':True},{'a/b':0,'x':1,'z':3})
        self.assertEqual({x['pointer'] for x in d},{'/a~1b','/x','/z'})
    def test_cli_exit_contract(self):
        command=[sys.executable,str(Path(__file__).with_name('inspect_runtime_identity.py')),
            '--lock',str(self.lock),'--artifact',str(self.artifact)]
        self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
        self.manifest['generated_at_utc']='different';self.save_manifest()
        self.assertEqual(subprocess.run(command,capture_output=True).returncode,1)
        self.lock.unlink();self.assertEqual(subprocess.run(command,capture_output=True).returncode,2)

class ToyProducer:
    """Ordered arithmetic and two commits; deliberately has no crafting semantics."""
    def __init__(self):
        self.cursor=0;self.work=0;self.staged=[];self.committed=();self.cancelled=False
    def read(self):return self.committed
    def step(self,quantum):
        if self.cancelled:return
        for _ in range(quantum):
            if self.cursor==16:break
            self.staged.append(self.cursor*self.cursor)
            self.cursor+=1;self.work+=1
            if self.cursor in (8,16):
                self.committed+=tuple(self.staged);self.staged=[]
    def cancel(self):self.cancelled=True;self.staged=[]

class AbstractContractTests(unittest.TestCase):
    def test_chunk_sizes_preserve_ordered_commits(self):
        expected=tuple(i*i for i in range(16))
        for q in (1,2,3,5,8,17):
            p=ToyProducer()
            while p.cursor<16:p.step(q)
            self.assertEqual(p.read(),expected);self.assertEqual(p.work,16)
    def test_every_prefix_and_passive_read(self):
        p=ToyProducer()
        for i in range(17):
            before=(p.cursor,p.work,tuple(p.staged))
            for _ in range(100):self.assertEqual(len(p.read()),8*(i//8))
            self.assertEqual(before,(p.cursor,p.work,tuple(p.staged)))
            if i<16:p.step(1)
    def test_cancel_at_each_prefix_releases_only_staged_without_refund(self):
        for stop in range(17):
            p=ToyProducer();p.step(stop);evidence=p.read();work=p.work
            p.cancel();p.step(100)
            self.assertEqual(p.read(),evidence);self.assertEqual(p.work,work);self.assertEqual(p.staged,[])
    def test_stale_minimum_false_acceptance_counterexample(self):
        old=(Fraction(1),Fraction(10));new=(Fraction(10),Fraction(1));source=Fraction(5)
        old_index=min(range(2),key=lambda i:old[i])
        self.assertLessEqual(source,new[old_index]);self.assertGreater(source,min(new))
    def test_partial_mass_changes_success(self):
        emitted=Fraction(9,10);pending_trap=Fraction(1,10)
        self.assertEqual(emitted+pending_trap,1);self.assertNotEqual(emitted,emitted/emitted)
    def test_transfer_and_release_do_not_refund_work(self):
        reservation,staged,committed,work=64,40,0,12
        # Reservation is an admission bound, not an extra live copy.
        self.assertLessEqual(staged+committed,reservation)
        committed,staged=staged,0
        self.assertEqual(staged+committed,40)
        before=work;committed=0
        self.assertEqual(work,before);self.assertEqual(staged+committed,0)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    suite=unittest.TestSuite([unittest.defaultTestLoader.loadTestsFromTestCase(c) for c in (IdentityTests,AbstractContractTests)])
    stream=io.StringIO();result=unittest.TextTestRunner(stream=stream,verbosity=2).run(suite)
    receipt={'scope':'offline diagnostic utility and abstract counterexamples only; no native/PowerShell/WASM qualification',
        'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),'successful':result.wasSuccessful(),
        'powershell_probe_executed':False,'python':sys.version,'log':stream.getvalue()}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in receipt.items() if k!='log'},indent=2))
    return 0 if result.wasSuccessful() else 1
if __name__=='__main__':raise SystemExit(main())
