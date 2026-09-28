import json
from contextlib import closing
from pathlib import Path
import sqlite3
import struct
import tempfile
import unittest
from unittest.mock import patch

from poecraft_ingest import ui_assets as assets


class UiAssetTests(unittest.TestCase):
    def test_visual_paths_stay_on_the_official_image_origin(self):
        self.assertEqual(assets.source_url({"dds_file": "Art/2DItems/Test's Icon.dds"}),
                         "https://web.poecdn.com/image/Art/2DItems/Test%27s%20Icon.png")
        for path in ("../secrets.dds", "https://elsewhere/icon.dds", "Art/2DItems/../secret.dds", "Art/2DItems/icon.exe"):
            self.assertIsNone(assets.source_url({"dds_file": path}))
        with self.assertRaises(ValueError):
            assets.png_size(b"<html>not an image</html>")
        with self.assertRaises(ValueError):
            assets.png_size(b"\x89PNG\r\n\x1a\n\0\0\0\rIHDR" + struct.pack(">II", 0, 32))

    def test_catalog_is_joined_to_the_locked_runtime_and_validated_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "runtime"
            runtime.mkdir()
            db = root / "source.db"
            with closing(sqlite3.connect(db)) as connection, connection:
                connection.executescript("CREATE TABLE data_manifest(data_hash TEXT,source_version TEXT);"
                                         "CREATE TABLE base_item(metadata_path TEXT,name TEXT,visual_identity_json TEXT);")
                connection.execute("INSERT INTO data_manifest VALUES('source-hash','fixture')")
                for key, name in [("supported-base", "Test Base"), ("unsupported-base", "Other Base"), ("currency", "Chaos Orb")]:
                    connection.execute("INSERT INTO base_item VALUES(?,?,?)", (key, name, json.dumps({"dds_file": "Art/2DItems/Fixture.dds"})))
            files = {"game-data.json": {"base_items": {"metadata_path_string_ids": [0, 1], "session_support_codes": [0, 1]}},
                     "strings.json": {"strings": ["supported-base", "unsupported-base"]}}
            manifest = {"source": {"data_hash": "source-hash"}, "files": {}}
            for name, value in files.items():
                raw = json.dumps(value).encode()
                (runtime / name).write_bytes(raw)
                manifest["files"][name] = {"sha256": assets.digest(raw)}
            raw = json.dumps(manifest).encode()
            (runtime / "manifest.json").write_bytes(raw)
            lock = root / "lock.json"
            lock.write_text(json.dumps({"runtime_directory": "runtime", "manifest_sha256": assets.digest(raw)}))
            # A minimal PNG header is enough for the bounded header validator.
            png = b"\x89PNG\r\n\x1a\n\0\0\0\rIHDR" + struct.pack(">II", 32, 64)
            sha = assets.digest(png)
            output = root / "art"
            output.mkdir()
            (output / (sha + ".png")).write_bytes(png)
            url = assets.source_url({"dds_file": "Art/2DItems/Fixture.dds"})
            cached = {"file": sha + ".png", "sha256": sha, "bytes": len(png), "width": 32, "height": 64}
            (output / "catalog.json").write_text(json.dumps({"images": {url: cached}}))
            with patch.object(assets, "ROOT", root), patch.object(assets.subprocess, "run") as network:
                result = assets.build(db, lock, output, 1)
                network.assert_not_called()
                self.assertEqual(set(result["items"]), {"supported-base", "currency"})
                self.assertEqual(result["actions"]["chaos"], "currency")
                self.assertEqual(result["items"]["supported-base"]["image"], sha + ".png")
                self.assertEqual(result["unavailable"], [])
                first = (output / "catalog.json").read_bytes()
                assets.build(db, lock, output, 1)
                self.assertEqual((output / "catalog.json").read_bytes(), first)
                with closing(sqlite3.connect(db)) as connection, connection:
                    connection.execute("UPDATE data_manifest SET data_hash='wrong-snapshot'")
                with self.assertRaisesRegex(ValueError, "does not match"):
                    assets.metadata(db, lock)
                (runtime / "strings.json").write_text("{}")
                with self.assertRaisesRegex(ValueError, "payload identity mismatch"):
                    assets.metadata(db, lock)


if __name__ == "__main__":
    unittest.main()
