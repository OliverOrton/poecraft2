"""Historical provisioning must fail closed before replacing source files."""
import hashlib
import io
import json

import pytest

from poecraft_ingest.repo_loader import (
    SOURCE_SPECS, _aggregate_source_hash, fetch_source_snapshot,
)


@pytest.fixture
def locked_sources(tmp_path, monkeypatch):
    payloads = {}
    entries = []
    for spec in SOURCE_SPECS:
        value = {"label": "é", "absent": None, "array": [None, {"absent": None}]} \
            if spec.expected_shape == "object" else [None, {"label": "é", "absent": None}]
        raw = json.dumps(value, indent=2).encode()
        expected = '{"label":"é","array":[null,{}]}' if spec.expected_shape == "object" \
            else '[null,{"label":"é"}]'
        output = expected.encode("utf-8")
        url = f"https://frozen.invalid/{spec.logical_name}"
        payloads[url] = raw
        entries.append({"logical_name": spec.logical_name, "byte_size": len(output),
            "content_hash": hashlib.sha256(output).hexdigest(),
            "archive": {"url": url, "encoding": "compact_without_null_fields",
                "sha256": hashlib.sha256(raw).hexdigest()}})
    manifest = {"files": entries, "source_hash": _aggregate_source_hash(entries)}
    lock = tmp_path / "lock.json"
    lock.write_text(json.dumps(manifest), encoding="utf-8")
    monkeypatch.setattr("urllib.request.urlopen", lambda request, **kwargs:
        io.BytesIO(payloads[request.full_url]))
    return lock, manifest, payloads


def test_locked_export_preserves_unicode_and_array_positions(tmp_path, locked_sources):
    lock, manifest, _ = locked_sources
    target = tmp_path / "source"
    assert fetch_source_snapshot(target, locked_manifest=lock) == manifest
    assert (target / "mods.json").read_bytes() == '{"label":"é","array":[null,{}]}'.encode()
    assert (target / "tags.json").read_bytes() == '[null,{"label":"é"}]'.encode()
    (target / "mods.json").write_bytes(b"stale")
    with pytest.raises(ValueError, match="differs from the locked source"):
        fetch_source_snapshot(target, locked_manifest=lock)
    assert (target / "mods.json").read_bytes() == b"stale"


@pytest.mark.parametrize("corruption", ["archive", "output"])
def test_locked_failure_preserves_previous_snapshot(tmp_path, locked_sources, corruption):
    lock, manifest, payloads = locked_sources
    target = tmp_path / "source"
    fetch_source_snapshot(target, locked_manifest=lock)
    before = {p.name: p.read_bytes() for p in target.iterdir()}
    last = manifest["files"][-1]
    if corruption == "archive":
        payloads[last["archive"]["url"]] += b" "
    else:
        last["content_hash"] = "0" * 64
        manifest["source_hash"] = _aggregate_source_hash(manifest["files"])
        lock.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ValueError, match="hash mismatch"):
        fetch_source_snapshot(target, locked_manifest=lock, force=True)
    assert {p.name: p.read_bytes() for p in target.iterdir()} == before
