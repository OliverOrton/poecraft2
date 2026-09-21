"""S1: actual public native evaluator for the identical worker request."""
import json
from pathlib import Path
from time import perf_counter
from poecraft_engine import load_data, load_economy

root = Path.cwd()
out = root / "out/cooperative-setup"
request = json.loads((out / "S1-request.json").read_text(encoding="utf-8"))
started = perf_counter()
with load_data(root / "data/compiled/current/manifest.json") as data:
    with data.create_session(request["session"]["base_metadata_path"], request["session"]["item_level"]) as session:
        with session.compile_strategy(request["graph"]) as strategy:
            with load_economy(request["economy"]) as economy:
                result = strategy.evaluate(economy=economy, **request["options"])
(out / "S1-native-evaluation.json").write_text(json.dumps({"result": result, "wall_ms": (perf_counter() - started) * 1000}), encoding="utf-8")
