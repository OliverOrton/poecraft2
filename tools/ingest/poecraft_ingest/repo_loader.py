from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import tempfile
from typing import Any
import urllib.request


REPOE_BASE_URL = "https://repoe-fork.github.io"


@dataclass(frozen=True)
class SourceSpec:
    logical_name: str
    remote_name: str
    expected_shape: str


SOURCE_SPECS = (
    SourceSpec("mods.json", "mods.min.json", "object"),
    SourceSpec("base_items.json", "base_items.min.json", "object"),
    SourceSpec("tags.json", "tags.min.json", "array"),
    SourceSpec("fossils.json", "fossils.min.json", "object"),
    SourceSpec("essences.json", "essences.min.json", "object"),
    SourceSpec("item_classes.json", "item_classes.min.json", "object"),
    SourceSpec(
        "crafting_bench_options.json",
        "crafting_bench_options.min.json",
        "array",
    ),
    SourceSpec("stat_translations.json", "stat_translations.min.json", "array"),
    SourceSpec("stats.json", "stats.min.json", "object"),
    SourceSpec("mod_types.json", "mod_types.min.json", "object"),
    SourceSpec("cluster_jewels.json", "cluster_jewels.json", "object"),
    SourceSpec(
        "cluster_jewel_notables.json",
        "cluster_jewel_notables.json",
        "array",
    ),
)


@dataclass(frozen=True)
class LoadedSource:
    spec: SourceSpec
    path: Path
    data: Any
    content_hash: str
    byte_size: int
    row_count: int
    metadata: dict[str, Any]


@dataclass(frozen=True)
class SourceSnapshot:
    directory: Path
    sources: dict[str, LoadedSource]
    source_hash: str
    source_version: str
    created_at_utc: str


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00",
        "Z",
    )


def _sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _validate_shape(spec: SourceSpec, value: Any) -> None:
    if spec.expected_shape == "object" and not isinstance(value, dict):
        raise ValueError(f"{spec.logical_name} must contain a JSON object")
    if spec.expected_shape == "array" and not isinstance(value, list):
        raise ValueError(f"{spec.logical_name} must contain a JSON array")


def _row_count(value: Any) -> int:
    return len(value)


def _aggregate_source_hash(files: list[dict[str, Any]]) -> str:
    digest = hashlib.sha256()
    for entry in sorted(files, key=lambda item: item["logical_name"]):
        digest.update(entry["logical_name"].encode("utf-8"))
        digest.update(b"\0")
        digest.update(entry["content_hash"].encode("ascii"))
        digest.update(b"\0")
    return digest.hexdigest()


def fetch_source_snapshot(
    output_directory: Path,
    *,
    force: bool = False,
    base_url: str = REPOE_BASE_URL,
) -> dict[str, Any]:
    output_directory.mkdir(parents=True, exist_ok=True)
    manifest_path = output_directory / "source-manifest.json"
    previous_manifest: dict[str, Any] = {}
    if manifest_path.exists():
        previous_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    previous_files = {
        entry["logical_name"]: entry
        for entry in previous_manifest.get("files", [])
        if "logical_name" in entry
    }

    staged: list[tuple[Path, Path]] = []
    file_entries: list[dict[str, Any]] = []
    try:
        for spec in SOURCE_SPECS:
            destination = output_directory / spec.logical_name
            previous = previous_files.get(spec.logical_name, {})
            if destination.exists() and not force:
                payload = destination.read_bytes()
                value = json.loads(payload)
                _validate_shape(spec, value)
                entry = {
                    "logical_name": spec.logical_name,
                    "remote_name": spec.remote_name,
                    "source_url": f"{base_url}/{spec.remote_name}",
                    "content_hash": _sha256_bytes(payload),
                    "byte_size": len(payload),
                    "row_count": _row_count(value),
                    "etag": previous.get("etag"),
                    "last_modified": previous.get("last_modified"),
                }
                file_entries.append(entry)
                continue

            request = urllib.request.Request(
                f"{base_url}/{spec.remote_name}",
                headers={"User-Agent": "poecraft2-ingest/0.1"},
            )
            with urllib.request.urlopen(request, timeout=120) as response:
                payload = response.read()
                headers = response.headers
            value = json.loads(payload)
            _validate_shape(spec, value)

            with tempfile.NamedTemporaryFile(
                dir=output_directory,
                prefix=f".{spec.logical_name}.",
                suffix=".download",
                delete=False,
            ) as handle:
                handle.write(payload)
                temporary = Path(handle.name)
            staged.append((temporary, destination))
            file_entries.append(
                {
                    "logical_name": spec.logical_name,
                    "remote_name": spec.remote_name,
                    "source_url": f"{base_url}/{spec.remote_name}",
                    "content_hash": _sha256_bytes(payload),
                    "byte_size": len(payload),
                    "row_count": _row_count(value),
                    "etag": headers.get("ETag"),
                    "last_modified": headers.get("Last-Modified"),
                }
            )

        for temporary, destination in staged:
            temporary.replace(destination)
        staged.clear()

        source_hash = _aggregate_source_hash(file_entries)
        generated_at = (
            previous_manifest.get("generated_at_utc")
            if previous_manifest and not force
            else _utc_now()
        )
        manifest = {
            "manifest_version": 1,
            "source": "RePoE fork",
            "base_url": base_url,
            "generated_at_utc": generated_at or _utc_now(),
            "source_hash": source_hash,
            "source_version": f"repoe-{source_hash[:16]}",
            "files": sorted(file_entries, key=lambda item: item["logical_name"]),
        }
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return manifest
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def load_source_snapshot(directory: Path) -> SourceSnapshot:
    directory = directory.resolve()
    manifest_path = directory / "source-manifest.json"
    manifest: dict[str, Any] = {}
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest_files = {
        entry["logical_name"]: entry
        for entry in manifest.get("files", [])
        if "logical_name" in entry
    }

    sources: dict[str, LoadedSource] = {}
    aggregate_entries: list[dict[str, Any]] = []
    for spec in SOURCE_SPECS:
        path = directory / spec.logical_name
        if not path.exists():
            raise FileNotFoundError(f"missing required RePoE source: {path}")
        payload = path.read_bytes()
        value = json.loads(payload)
        _validate_shape(spec, value)
        content_hash = _sha256_bytes(payload)
        metadata = dict(manifest_files.get(spec.logical_name, {}))
        declared_hash = metadata.get("content_hash")
        if declared_hash and declared_hash != content_hash:
            raise ValueError(
                f"{spec.logical_name} does not match source-manifest.json"
            )
        loaded = LoadedSource(
            spec=spec,
            path=path,
            data=value,
            content_hash=content_hash,
            byte_size=len(payload),
            row_count=_row_count(value),
            metadata=metadata,
        )
        sources[spec.logical_name] = loaded
        aggregate_entries.append(
            {
                "logical_name": spec.logical_name,
                "content_hash": content_hash,
            }
        )

    source_hash = _aggregate_source_hash(aggregate_entries)
    declared_source_hash = manifest.get("source_hash")
    if declared_source_hash and declared_source_hash != source_hash:
        raise ValueError("source snapshot hash does not match source-manifest.json")

    return SourceSnapshot(
        directory=directory,
        sources=sources,
        source_hash=source_hash,
        source_version=manifest.get(
            "source_version",
            f"repoe-{source_hash[:16]}",
        ),
        created_at_utc=manifest.get(
            "generated_at_utc",
            "1970-01-01T00:00:00Z",
        ),
    )
