"""Focused negative checks for the explicit subsolution audit; no native run."""
import json
from pathlib import Path
import runpy
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
verify = runpy.run_path(str(ROOT / "engine/benchmarks/verify_quotient_lower_probe.py"))["verify_joint"]
early, full = map(Path, sys.argv[1:3])


def refused(label, path, **kwargs):
    try:
        verify(path, **kwargs)
    except AssertionError:
        print(label + ": refused as required")
    else:
        raise AssertionError(label + " was accepted")


refused("early vector does not satisfy the full-refinement quality contract", early)
refused("full vector cannot claim an early-endpoint audit", full, checked_subsolution=True)
data = json.loads(early.read_text(encoding="utf-8-sig"))
p = data["probabilistic_donor"]
row = next(r for r in p["checked_relations"] if len(r["exits"]) >= 2
           and r["exits"][0][1] != r["exits"][1][1]
           and p["values"][r["exits"][0][0]] != p["values"][r["exits"][1][0]])
row["exits"][0][1], row["exits"][1][1] = row["exits"][1][1], row["exits"][0][1]
# Preserve total mass but invalidate the final-value minimizing allocation.
with tempfile.TemporaryDirectory(prefix="poecraft-subsolution-audit-") as directory:
    mutant = Path(directory) / "stale-allocation.json"
    mutant.write_text(json.dumps(data), encoding="utf-8")
    refused("normalized stale allocation", mutant, checked_subsolution=True)
print("All three audit boundary negatives passed")
