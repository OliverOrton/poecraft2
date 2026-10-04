from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import platform
import sys

import pytest


@pytest.fixture
def record_process_result(request, tmp_path):
    """Retain full worker observations before pytest formats an assertion."""
    directory = Path(os.environ.get("POECRAFT_TEST_PROCESS_EVIDENCE_DIR", tmp_path / "process-probes"))
    directory.mkdir(parents=True, exist_ok=True)
    sequence = 0

    def record(result):
        nonlocal sequence
        sequence += 1
        key = hashlib.sha256(request.node.nodeid.encode()).hexdigest()[:20]
        path = directory / f"{key}-{sequence}.json"
        receipt = {"test_id": request.node.nodeid, "python": sys.version,
                   "python_executable": sys.executable, "platform": platform.platform(),
                   "process": result}
        with path.open("x", encoding="utf-8") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
        return path
    return record
