"""Evaluate an archived controller as a reference, never as a search seed."""
import hashlib
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / "tools/ingest"), str(ROOT / "bindings/python")]
from poecraft_engine import load_data, load_economy, engine_library_path

OUT = ROOT / "out/2026-09-09-cross-base-capability-recovery"
old_path = ROOT / "build/performance/solver-quality-gate0-product8-ledger-fixed/cases/conquest-lamellar-allflame-partial-3-to-5-product8.json"
old = json.loads(old_path.read_text())
case = old["cases"][0]
current = json.loads((ROOT / "fixtures/solver-quality-ladder/v1/cases/conquest-lamellar-allflame-partial-3-to-5-product8.json").read_text())
manifest = json.loads((ROOT / "data/compiled/current/manifest.json").read_text())
for key in ("session", "start", "goal", "product_action_envelope", "economy", "verification", "corpus"):
    assert case["input"][key] == current[key], key
assert old["artifact"]["manifest"]["files"] == manifest["files"]
graph_path = Path(case["compiled_graph"]["strategy_output_path"])
graph = graph_path.read_text()
snapshot = json.loads((ROOT / current["economy"]["snapshot_path"]).read_text())
snapshot["prices"].update(current["economy"]["manual_overrides"])
started = time.monotonic()
with load_data(ROOT / "data/compiled/current") as data:
    with data.create_session(current["session"]["base_metadata_path"], current["session"]["item_level"]) as session:
        with load_economy(snapshot) as economy:
            with session.compile_strategy(graph) as strategy:
                evaluation = strategy.evaluate(economy=economy, max_states=1000000,
                    max_pairs=5000000, max_transitions=20000000, max_owned_bytes=1073741824)
receipt = {
    "reference_kind": "archived_graph_current_engine_evaluation_not_search_seed",
    "old_report": str(old_path.relative_to(ROOT)),
    "old_graph_sha256": hashlib.sha256(graph_path.read_bytes()).hexdigest(),
    "engine_library_sha256": hashlib.sha256(Path(engine_library_path()).read_bytes()).hexdigest(),
    "matching_input_fields": ["session", "start", "goal", "product_action_envelope", "economy", "verification", "corpus"],
    "artifact_files_match": True,
    "historical_cost": case["solve_summary"]["upper_bound"],
    "elapsed_ms": (time.monotonic() - started) * 1000,
    "evaluation": evaluation,
}
(OUT / "A0-partial-reference.json").write_bytes((json.dumps(receipt, indent=2) + "\n").encode())
print(json.dumps({k: v for k, v in receipt.items() if k != "evaluation"}))
print(json.dumps({"converged": evaluation.get("converged"), "terminals": evaluation.get("terminals"),
                  "totals": evaluation.get("accounting", {}).get("totals")}))
