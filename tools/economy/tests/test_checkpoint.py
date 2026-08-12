from __future__ import annotations

import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

from poecraft_economy.checkpoint import create_manifest, verify


class CheckpointManifestTests(unittest.TestCase):
    @staticmethod
    def _create_database(path: Path, schema_version: int) -> None:
        connection = sqlite3.connect(path)
        try:
            connection.execute(
                """
                CREATE TABLE economy_manifest (
                    singleton INTEGER PRIMARY KEY,
                    schema_version INTEGER NOT NULL
                )
                """
            )
            connection.execute(
                "INSERT INTO economy_manifest VALUES (1, ?)",
                (schema_version,),
            )
            connection.commit()
        finally:
            connection.close()

    @staticmethod
    def _write_manifest(path: Path, manifest: dict[str, object]) -> None:
        path.write_text(json.dumps(manifest), encoding="utf-8")

    def test_new_manifest_records_and_verifies_database_schema_version(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            database = root / "economy.db"
            manifest_path = root / "checkpoint.json"
            self._create_database(database, schema_version=7)

            manifest = create_manifest(database)
            self.assertEqual(manifest["schema_version"], 1)
            self.assertEqual(manifest["database_schema_version"], 7)
            self._write_manifest(manifest_path, manifest)
            self.assertEqual(verify(database, manifest_path), manifest)

            manifest["database_schema_version"] = 8
            self._write_manifest(manifest_path, manifest)
            with self.assertRaisesRegex(ValueError, "database schema mismatch"):
                verify(database, manifest_path)

    def test_verify_accepts_legacy_manifest_without_database_schema_version(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            database = root / "economy.db"
            manifest_path = root / "checkpoint.json"
            self._create_database(database, schema_version=1)

            manifest = create_manifest(database)
            del manifest["database_schema_version"]
            self._write_manifest(manifest_path, manifest)

            self.assertEqual(verify(database, manifest_path), manifest)


if __name__ == "__main__":
    unittest.main()
