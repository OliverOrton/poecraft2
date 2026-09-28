"""Derive the browser artwork catalogue from the canonical, product-matched DB.

No game tables or compiled mechanics are modified. Image paths come from the
canonical visual identities; action aliases are presentation-only joins by name.
Downloads use curl's platform certificate verification (Schannel on Windows).
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import closing
import hashlib
import json
import re
from pathlib import Path
import sqlite3
import struct
import subprocess
import tempfile
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[3]
CDN = "https://web.poecdn.com/image/"
ACTION_NAMES = {
    "transmute": "Orb of Transmutation", "augment": "Orb of Augmentation",
    "alteration": "Orb of Alteration", "regal": "Regal Orb",
    "alchemy": "Orb of Alchemy", "chaos": "Chaos Orb", "exalt": "Exalted Orb",
    "annul": "Orb of Annulment", "scour": "Orb of Scouring",
    "fracture": "Fracturing Orb", "divine": "Divine Orb", "blessed": "Blessed Orb",
    "veiled_chaos": "Veiled Chaos Orb", "veiled_exalt": "Veiled Exalted Orb",
    "eldritch_ember": "Lesser Eldritch Ember", "eldritch_ichor": "Lesser Eldritch Ichor",
    "eldritch_exalt": "Eldritch Exalted Orb", "eldritch_chaos": "Eldritch Chaos Orb",
    "eldritch_annul": "Eldritch Orb of Annulment", "influence_exalt": "Crusader's Exalted Orb",
    "essence": "Deafening Essence of Woe", "fossil": "Pristine Fossil",
    "harvest_reforge": "Vivid Crystallised Lifeforce", "harvest_augment": "Sacred Crystallised Lifeforce",
    "harvest_resist": "Primal Crystallised Lifeforce",
}
INFLUENCE_NAMES = {
    "shaper": "Shaper's Exalted Orb", "elder": "Elder's Exalted Orb",
    "crusader": "Crusader's Exalted Orb", "hunter": "Hunter's Exalted Orb",
    "redeemer": "Redeemer's Exalted Orb", "warlord": "Warlord's Exalted Orb",
    "searing exarch": "Lesser Eldritch Ember", "eater of worlds": "Lesser Eldritch Ichor",
}
LIFEFORCE_NAMES = {
    "wild": "Wild Crystallised Lifeforce", "vivid": "Vivid Crystallised Lifeforce",
    "primal": "Primal Crystallised Lifeforce", "sacred": "Sacred Crystallised Lifeforce",
    "rancour": "Crystallised Rancour",
}


def craft_details(connection: sqlite3.Connection) -> dict[str, dict]:
    """Presentation text only, joined from the same canonical snapshot as the art."""
    details: dict[str, dict] = {}
    for row in connection.execute("SELECT key, descriptions_json FROM fossil ORDER BY key"):
        descriptions = json.loads(row["descriptions_json"] or "{}")
        details[row["key"]] = {"description": "\n".join(descriptions.values())}
    for row in connection.execute("""
        SELECT e.key, em.item_class_key, m.text
        FROM essence e JOIN essence_mod em USING(essence_id) JOIN mod m USING(mod_id)
        ORDER BY e.key, em.item_class_key
    """):
        details.setdefault(row["key"], {}).setdefault("essence_mods", {})[row["item_class_key"]] = row["text"]
    return details


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def source_url(visual: dict) -> str | None:
    path = visual.get("dds_file", "")
    if not path.startswith("Art/2DItems/") or not path.endswith(".dds") or ".." in path:
        return None
    return CDN + quote(path[:-4] + ".png", safe="/")


def png_size(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or not data.startswith(b"\x89PNG\r\n\x1a\n") or data[12:16] != b"IHDR":
        raise ValueError("asset response is not PNG")
    width, height = struct.unpack(">II", data[16:24])
    if not 0 < width <= 4096 or not 0 < height <= 4096:
        raise ValueError("asset dimensions outside display bounds")
    return width, height


def metadata(database: Path, lock_path: Path) -> tuple[dict, dict, dict, dict, dict]:
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    runtime = ROOT / lock["runtime_directory"]
    raw = (runtime / "manifest.json").read_bytes()
    if digest(raw) != lock["manifest_sha256"]:
        raise ValueError("product manifest identity mismatch")
    manifest = json.loads(raw)
    payloads = {}
    for name in ("game-data.json", "strings.json"):
        data = (runtime / name).read_bytes()
        if digest(data) != manifest["files"][name]["sha256"]:
            raise ValueError("product payload identity mismatch: " + name)
        payloads[name] = json.loads(data)
    bases = payloads["game-data.json"]["base_items"]
    strings = payloads["strings.json"]["strings"]
    supported = {strings[bases["metadata_path_string_ids"][i]]
                 for i, code in enumerate(bases["session_support_codes"]) if code == 0}
    with closing(sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)) as connection:
        connection.row_factory = sqlite3.Row
        identity = dict(connection.execute("SELECT * FROM data_manifest").fetchone())
        if identity["data_hash"] != manifest["source"]["data_hash"]:
            raise ValueError("canonical database does not match selected product runtime")
        rows = connection.execute("SELECT metadata_path,name,visual_identity_json,properties_json FROM base_item ORDER BY metadata_path").fetchall()
        details = craft_details(connection)
    wanted_names = set(ACTION_NAMES.values()) | set(INFLUENCE_NAMES.values()) | set(LIFEFORCE_NAMES.values())
    items, names = {}, {}
    for row in rows:
        name, key = row["name"], row["metadata_path"]
        if key not in supported and name not in wanted_names and "Essence of " not in name and not name.endswith(" Fossil"):
            continue
        visual = json.loads(row["visual_identity_json"] or "{}") or {}
        url = source_url(visual)
        items[key] = {"name": name, "visual_identity": visual.get("id"), "source_url": url}
        description = (json.loads(row["properties_json"] or "{}") or {}).get("description")
        if description:
            items[key]["description"] = description
        items[key].update(details.get(key, {}))
        names.setdefault(name, key)
    actions = {key: names[name] for key, name in ACTION_NAMES.items() if name in names}
    influences = {key: names[name] for key, name in INFLUENCE_NAMES.items() if name in names}
    recipes = json.loads((ROOT / "fixtures/economy/harvest-recipes-v1.json").read_text(encoding="utf-8"))
    # Primary lifeforce art follows the recipe owner; sacred/rancour are supplementary costs.
    harvest = {f"component:{key}": names[name] for key, name in LIFEFORCE_NAMES.items() if name in names}
    for kind in ("reforge", "augment", "resistance"):
        for tag, costs in recipes[kind].items():
            primary = next((component for component in costs if component in ("wild", "vivid", "primal")), None)
            name = LIFEFORCE_NAMES.get(primary)
            if name in names:
                harvest[f"{kind}:{tag}"] = names[name]
    source = {"runtime_manifest_sha256": lock["manifest_sha256"], "canonical_data_hash": identity["data_hash"],
              "source_version": identity["source_version"], "image_origin": CDN}
    return source, items, actions, influences, harvest


def download(url: str, output: Path, cached: dict | None) -> dict:
    if cached and re.fullmatch(r"[a-f0-9]{64}\.png", cached.get("file", "")):
        path = output / cached["file"]
        if path.is_file():
            data = path.read_bytes()
            sha = digest(data)
            if sha == cached.get("sha256") and cached["file"] == sha + ".png":
                width, height = png_size(data)
                return {"file": cached["file"], "sha256": sha, "bytes": len(data), "width": width, "height": height}
    with tempfile.TemporaryDirectory(prefix="poecraft-art-") as temporary:
        path = Path(temporary) / "asset.png"
        result = subprocess.run([
            "curl", "--fail", "--silent", "--show-error", "--proto", "=https",
            "--max-time", "30", "--retry", "1", "--output", str(path),
            "--write-out", "%{content_type}", url,
        ], capture_output=True, text=True, timeout=70)
        if result.returncode:
            raise ValueError(result.stderr.strip()[-220:])
        if not result.stdout.lower().startswith("image/png"):
            raise ValueError("asset content type is not image/png")
        data = path.read_bytes()
    width, height = png_size(data)
    sha = digest(data)
    file = sha + ".png"
    (output / file).write_bytes(data)
    return {"file": file, "sha256": sha, "bytes": len(data), "width": width, "height": height}


def build(database: Path, lock: Path, output: Path, workers: int) -> dict:
    source, items, actions, influences, harvest = metadata(database, lock)
    output.mkdir(parents=True, exist_ok=True)
    existing = output / "catalog.json"
    prior = json.loads(existing.read_text(encoding="utf-8")) if existing.exists() else {}
    cache = prior.get("images", {})
    urls = sorted({item["source_url"] for item in items.values() if item["source_url"]})
    images, missing = {}, []
    with ThreadPoolExecutor(max_workers=max(1, min(4, workers))) as pool:
        jobs = {pool.submit(download, url, output, cache.get(url)): url for url in urls}
        for count, future in enumerate(as_completed(jobs), 1):
            url = jobs[future]
            try:
                images[url] = future.result()
            except Exception as error:
                missing.append({"source_url": url, "reason": str(error)})
            if count % 50 == 0:
                print(f"artwork {count}/{len(urls)}; unavailable {len(missing)}", flush=True)
    for item in items.values():
        image = images.get(item["source_url"])
        if image:
            item["image"] = image["file"]
            item["width"], item["height"] = image["width"], image["height"]
    result = {"schema_version": 1, "source": source, "items": items, "actions": actions, "influences": influences, "harvest": harvest,
              "images": dict(sorted(images.items())), "unavailable": sorted(missing, key=lambda row: row["source_url"])}
    existing.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"items": len(items), "images": len(images), "unavailable": len(missing),
                      "bytes": sum(image["bytes"] for image in images.values()), "catalog": str(existing)}))
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, default=ROOT / "data/sqlite/poecraft.db")
    parser.add_argument("--lock", type=Path, default=ROOT / "apps/web/runtime.lock.json")
    parser.add_argument("--output", type=Path, default=ROOT / "apps/web/public/game-assets")
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()
    build(args.database, args.lock, args.output, args.workers)


if __name__ == "__main__":
    main()
