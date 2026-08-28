"""SQLite catalog authority for local Native Solver Lab experiments."""

from __future__ import annotations

from contextlib import contextmanager
from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3
from typing import Any, Iterator, Mapping

from poecraft_ingest.solver_lab_contracts import (
    ARTIFACT_SCHEMA_VERSION,
    ATTEMPT_SCHEMA_VERSION,
    COMMAND_SCHEMA_VERSION,
    EVENT_SCHEMA_VERSION,
    EXPERIMENT_SCHEMA_VERSION,
    JOB_SCHEMA_VERSION,
)


CATALOG_SCHEMA_VERSION = 1


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def _json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _decode(value: str | None, default: Any) -> Any:
    if value is None:
        return default
    return json.loads(value)


class SolverLabCatalog:
    """Small transactional repository; callers never issue ad hoc SQL."""

    def __init__(self, path: Path):
        self.path = path.resolve()
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.initialize()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.path, timeout=30.0)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 30000")
        return connection

    @contextmanager
    def transaction(self, *, immediate: bool = False) -> Iterator[sqlite3.Connection]:
        connection = self._connect()
        try:
            connection.execute("BEGIN IMMEDIATE" if immediate else "BEGIN")
            yield connection
            connection.commit()
        except Exception:
            connection.rollback()
            raise
        finally:
            connection.close()

    def initialize(self) -> None:
        with self._connect() as connection:
            connection.execute("PRAGMA journal_mode = WAL")
            connection.executescript(
                """
                CREATE TABLE IF NOT EXISTS catalog_metadata (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS experiments (
                    experiment_id TEXT PRIMARY KEY,
                    schema_version TEXT NOT NULL,
                    name TEXT NOT NULL,
                    description TEXT NOT NULL,
                    profile_id TEXT NOT NULL,
                    created_at TEXT NOT NULL,
                    document_json TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS jobs (
                    job_id TEXT PRIMARY KEY,
                    schema_version TEXT NOT NULL,
                    experiment_id TEXT REFERENCES experiments(experiment_id),
                    case_id TEXT NOT NULL,
                    case_path TEXT NOT NULL,
                    profile_id TEXT NOT NULL,
                    priority INTEGER NOT NULL,
                    status TEXT NOT NULL,
                    watchdog_seconds REAL NOT NULL,
                    reserved_memory_bytes INTEGER NOT NULL,
                    identity_sha256 TEXT NOT NULL,
                    request_json TEXT NOT NULL,
                    created_at TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                );
                CREATE INDEX IF NOT EXISTS jobs_queue_order
                    ON jobs(status, priority DESC, created_at ASC, job_id ASC);

                CREATE TABLE IF NOT EXISTS attempts (
                    attempt_id TEXT PRIMARY KEY,
                    schema_version TEXT NOT NULL,
                    job_id TEXT NOT NULL REFERENCES jobs(job_id),
                    ordinal INTEGER NOT NULL,
                    status TEXT NOT NULL,
                    directory TEXT NOT NULL UNIQUE,
                    supervisor_id TEXT,
                    command_json TEXT,
                    command_identity_sha256 TEXT,
                    result_json TEXT,
                    started_at TEXT,
                    finished_at TEXT,
                    created_at TEXT NOT NULL,
                    UNIQUE(job_id, ordinal)
                );
                CREATE INDEX IF NOT EXISTS attempts_job
                    ON attempts(job_id, ordinal DESC);

                CREATE TABLE IF NOT EXISTS commands (
                    command_id TEXT PRIMARY KEY,
                    schema_version TEXT NOT NULL,
                    idempotency_key TEXT NOT NULL UNIQUE,
                    operation TEXT NOT NULL,
                    target_id TEXT,
                    dry_run INTEGER NOT NULL,
                    request_json TEXT NOT NULL,
                    result_json TEXT NOT NULL,
                    created_at TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS events (
                    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    schema_version TEXT NOT NULL,
                    timestamp TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    entity_type TEXT NOT NULL,
                    entity_id TEXT NOT NULL,
                    payload_json TEXT NOT NULL
                );
                CREATE INDEX IF NOT EXISTS events_entity
                    ON events(entity_type, entity_id, event_id);

                CREATE TABLE IF NOT EXISTS artifacts (
                    artifact_id TEXT PRIMARY KEY,
                    schema_version TEXT NOT NULL,
                    attempt_id TEXT NOT NULL REFERENCES attempts(attempt_id),
                    kind TEXT NOT NULL,
                    path TEXT NOT NULL,
                    content_sha256 TEXT NOT NULL,
                    size_bytes INTEGER NOT NULL,
                    created_at TEXT NOT NULL,
                    UNIQUE(attempt_id, kind, path)
                );

                CREATE TABLE IF NOT EXISTS settings (
                    key TEXT PRIMARY KEY,
                    value_json TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                );
                """
            )
            connection.execute(
                "INSERT INTO catalog_metadata(key, value) VALUES('schema_version', ?) "
                "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                (str(CATALOG_SCHEMA_VERSION),),
            )
            connection.execute(f"PRAGMA user_version = {CATALOG_SCHEMA_VERSION}")

    def create_experiment(self, document: Mapping[str, Any]) -> dict[str, Any]:
        now = document.get("created_at") or utc_now()
        row = {
            "schema_version": EXPERIMENT_SCHEMA_VERSION,
            "experiment_id": str(document["experiment_id"]),
            "name": str(document["name"]),
            "description": str(document.get("description", "")),
            "profile_id": str(document["profile_id"]),
            "created_at": str(now),
        }
        stored = {**document, **row}
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "INSERT INTO experiments VALUES(?, ?, ?, ?, ?, ?, ?)",
                (
                    row["experiment_id"],
                    row["schema_version"],
                    row["name"],
                    row["description"],
                    row["profile_id"],
                    row["created_at"],
                    _json(stored),
                ),
            )
            self._insert_event(
                connection,
                "experiment_created",
                "experiment",
                row["experiment_id"],
                {"name": row["name"]},
            )
        return stored

    def get_experiment(self, experiment_id: str) -> dict[str, Any] | None:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT document_json FROM experiments WHERE experiment_id=?",
                (experiment_id,),
            ).fetchone()
        return _decode(row[0], {}) if row else None

    def command_by_idempotency_key(self, key: str) -> dict[str, Any] | None:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT * FROM commands WHERE idempotency_key=?", (key,)
            ).fetchone()
        return self._command_row(row) if row else None

    def submit_job(
        self,
        *,
        job: Mapping[str, Any],
        command_id: str,
        idempotency_key: str,
        operation_request: Mapping[str, Any],
        operation_result: Mapping[str, Any],
    ) -> dict[str, Any]:
        now = str(job["created_at"])
        with self.transaction(immediate=True) as connection:
            existing = connection.execute(
                "SELECT * FROM commands WHERE idempotency_key=?",
                (idempotency_key,),
            ).fetchone()
            if existing:
                return _decode(existing["result_json"], {})
            connection.execute(
                """
                INSERT INTO jobs(
                    job_id, schema_version, experiment_id, case_id, case_path,
                    profile_id, priority, status, watchdog_seconds,
                    reserved_memory_bytes, identity_sha256, request_json,
                    created_at, updated_at
                ) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    job["job_id"],
                    JOB_SCHEMA_VERSION,
                    job.get("experiment_id"),
                    job["case_id"],
                    job["case_path"],
                    job["profile_id"],
                    job["priority"],
                    job["status"],
                    job["watchdog_seconds"],
                    job["reserved_memory_bytes"],
                    job["identity_sha256"],
                    _json(job["request"]),
                    now,
                    now,
                ),
            )
            self._insert_command(
                connection,
                command_id=command_id,
                idempotency_key=idempotency_key,
                operation="submit_job",
                target_id=str(job["job_id"]),
                dry_run=False,
                request=operation_request,
                result=operation_result,
            )
            self._insert_event(
                connection,
                "job_queued",
                "job",
                str(job["job_id"]),
                {
                    "case_id": job["case_id"],
                    "priority": job["priority"],
                    "reserved_memory_bytes": job["reserved_memory_bytes"],
                },
            )
        return dict(operation_result)

    def list_jobs(self, *, limit: int = 200) -> list[dict[str, Any]]:
        limit = max(1, min(int(limit), 1000))
        with self._connect() as connection:
            rows = connection.execute(
                "SELECT * FROM jobs ORDER BY created_at DESC, job_id DESC LIMIT ?",
                (limit,),
            ).fetchall()
        return [self._job_row(row) for row in rows]

    def get_job(self, job_id: str) -> dict[str, Any] | None:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT * FROM jobs WHERE job_id=?", (job_id,)
            ).fetchone()
        return self._job_row(row) if row else None

    def latest_attempt(self, job_id: str) -> dict[str, Any] | None:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT * FROM attempts WHERE job_id=? "
                "ORDER BY ordinal DESC LIMIT 1",
                (job_id,),
            ).fetchone()
        return self._attempt_row(row) if row else None

    def get_attempt(self, attempt_id: str) -> dict[str, Any] | None:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT * FROM attempts WHERE attempt_id=?", (attempt_id,)
            ).fetchone()
        return self._attempt_row(row) if row else None

    def claim_next_job(
        self,
        *,
        supervisor_id: str,
        attempt_id: str,
        attempt_directory: Path,
    ) -> tuple[dict[str, Any], dict[str, Any]] | None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            row = connection.execute(
                "SELECT * FROM jobs WHERE status='queued' "
                "ORDER BY priority DESC, created_at ASC, job_id ASC LIMIT 1"
            ).fetchone()
            if row is None:
                return None
            job = self._job_row(row)
            ordinal = int(
                connection.execute(
                    "SELECT COALESCE(MAX(ordinal), 0) + 1 FROM attempts WHERE job_id=?",
                    (job["job_id"],),
                ).fetchone()[0]
            )
            connection.execute(
                "UPDATE jobs SET status='running', updated_at=? "
                "WHERE job_id=? AND status='queued'",
                (now, job["job_id"]),
            )
            connection.execute(
                """
                INSERT INTO attempts(
                    attempt_id, schema_version, job_id, ordinal, status,
                    directory, supervisor_id, created_at, started_at
                ) VALUES(?, ?, ?, ?, 'running', ?, ?, ?, ?)
                """,
                (
                    attempt_id,
                    ATTEMPT_SCHEMA_VERSION,
                    job["job_id"],
                    ordinal,
                    str(attempt_directory.resolve()),
                    supervisor_id,
                    now,
                    now,
                ),
            )
            self._insert_event(
                connection,
                "attempt_started",
                "attempt",
                attempt_id,
                {"job_id": job["job_id"], "ordinal": ordinal},
            )
        job["status"] = "running"
        attempt = self.get_attempt(attempt_id)
        assert attempt is not None
        return job, attempt

    def set_attempt_command(
        self,
        attempt_id: str,
        command: Mapping[str, Any],
    ) -> None:
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "UPDATE attempts SET command_json=?, command_identity_sha256=? "
                "WHERE attempt_id=?",
                (_json(command), command["identity_sha256"], attempt_id),
            )
            self._insert_event(
                connection,
                "attempt_command_resolved",
                "attempt",
                attempt_id,
                {"identity_sha256": command["identity_sha256"]},
            )

    def finish_attempt(
        self,
        *,
        attempt_id: str,
        attempt_status: str,
        job_status: str,
        result: Mapping[str, Any],
    ) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            row = connection.execute(
                "SELECT job_id FROM attempts WHERE attempt_id=?", (attempt_id,)
            ).fetchone()
            if row is None:
                raise KeyError(attempt_id)
            job_id = str(row["job_id"])
            connection.execute(
                "UPDATE attempts SET status=?, result_json=?, finished_at=? "
                "WHERE attempt_id=?",
                (attempt_status, _json(result), now, attempt_id),
            )
            connection.execute(
                "UPDATE jobs SET status=?, updated_at=? WHERE job_id=?",
                (job_status, now, job_id),
            )
            self._insert_event(
                connection,
                "attempt_finished",
                "attempt",
                attempt_id,
                {"job_id": job_id, "status": attempt_status},
            )
            self._insert_event(
                connection,
                "job_finished",
                "job",
                job_id,
                {"status": job_status, "attempt_id": attempt_id},
            )

    def cancel_queued_job(
        self,
        *,
        job_id: str,
        command_id: str,
        idempotency_key: str,
        operation_request: Mapping[str, Any],
        operation_result: Mapping[str, Any],
    ) -> dict[str, Any]:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            existing = connection.execute(
                "SELECT * FROM commands WHERE idempotency_key=?",
                (idempotency_key,),
            ).fetchone()
            if existing:
                return _decode(existing["result_json"], {})
            row = connection.execute(
                "SELECT status FROM jobs WHERE job_id=?", (job_id,)
            ).fetchone()
            if row is None:
                raise KeyError(job_id)
            if row["status"] != "queued":
                raise ValueError(
                    f"Gate 2 can cancel queued jobs only; current status={row['status']}"
                )
            connection.execute(
                "UPDATE jobs SET status='canceled', updated_at=? WHERE job_id=?",
                (now, job_id),
            )
            self._insert_command(
                connection,
                command_id=command_id,
                idempotency_key=idempotency_key,
                operation="cancel_job",
                target_id=job_id,
                dry_run=False,
                request=operation_request,
                result=operation_result,
            )
            self._insert_event(
                connection, "job_canceled", "job", job_id, {"from": "queued"}
            )
        return dict(operation_result)

    def list_events(
        self,
        *,
        entity_type: str,
        entity_id: str,
        limit: int = 200,
    ) -> list[dict[str, Any]]:
        limit = max(1, min(int(limit), 1000))
        with self._connect() as connection:
            rows = connection.execute(
                "SELECT * FROM events WHERE entity_type=? AND entity_id=? "
                "ORDER BY event_id DESC LIMIT ?",
                (entity_type, entity_id, limit),
            ).fetchall()
        return [self._event_row(row) for row in reversed(rows)]

    def add_artifact(self, artifact: Mapping[str, Any]) -> None:
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "INSERT OR IGNORE INTO artifacts VALUES(?, ?, ?, ?, ?, ?, ?, ?)",
                (
                    artifact["artifact_id"],
                    ARTIFACT_SCHEMA_VERSION,
                    artifact["attempt_id"],
                    artifact["kind"],
                    artifact["path"],
                    artifact["content_sha256"],
                    artifact["size_bytes"],
                    artifact.get("created_at", utc_now()),
                ),
            )

    def _insert_command(
        self,
        connection: sqlite3.Connection,
        *,
        command_id: str,
        idempotency_key: str,
        operation: str,
        target_id: str | None,
        dry_run: bool,
        request: Mapping[str, Any],
        result: Mapping[str, Any],
    ) -> None:
        connection.execute(
            "INSERT INTO commands VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                command_id,
                COMMAND_SCHEMA_VERSION,
                idempotency_key,
                operation,
                target_id,
                int(dry_run),
                _json(request),
                _json(result),
                utc_now(),
            ),
        )

    def _insert_event(
        self,
        connection: sqlite3.Connection,
        kind: str,
        entity_type: str,
        entity_id: str,
        payload: Mapping[str, Any],
    ) -> None:
        connection.execute(
            "INSERT INTO events(schema_version, timestamp, kind, entity_type, entity_id, payload_json) "
            "VALUES(?, ?, ?, ?, ?, ?)",
            (
                EVENT_SCHEMA_VERSION,
                utc_now(),
                kind,
                entity_type,
                entity_id,
                _json(payload),
            ),
        )

    @staticmethod
    def _job_row(row: sqlite3.Row) -> dict[str, Any]:
        return {
            "schema_version": row["schema_version"],
            "job_id": row["job_id"],
            "experiment_id": row["experiment_id"],
            "case_id": row["case_id"],
            "case_path": row["case_path"],
            "profile_id": row["profile_id"],
            "priority": row["priority"],
            "status": row["status"],
            "watchdog_seconds": row["watchdog_seconds"],
            "reserved_memory_bytes": row["reserved_memory_bytes"],
            "identity_sha256": row["identity_sha256"],
            "request": _decode(row["request_json"], {}),
            "created_at": row["created_at"],
            "updated_at": row["updated_at"],
        }

    @staticmethod
    def _attempt_row(row: sqlite3.Row) -> dict[str, Any]:
        return {
            "schema_version": row["schema_version"],
            "attempt_id": row["attempt_id"],
            "job_id": row["job_id"],
            "ordinal": row["ordinal"],
            "status": row["status"],
            "directory": row["directory"],
            "supervisor_id": row["supervisor_id"],
            "command": _decode(row["command_json"], None),
            "command_identity_sha256": row["command_identity_sha256"],
            "result": _decode(row["result_json"], None),
            "created_at": row["created_at"],
            "started_at": row["started_at"],
            "finished_at": row["finished_at"],
        }

    @staticmethod
    def _command_row(row: sqlite3.Row) -> dict[str, Any]:
        return {
            "command_id": row["command_id"],
            "schema_version": row["schema_version"],
            "idempotency_key": row["idempotency_key"],
            "operation": row["operation"],
            "target_id": row["target_id"],
            "dry_run": bool(row["dry_run"]),
            "request": _decode(row["request_json"], {}),
            "result": _decode(row["result_json"], {}),
            "created_at": row["created_at"],
        }

    @staticmethod
    def _event_row(row: sqlite3.Row) -> dict[str, Any]:
        return {
            "schema_version": row["schema_version"],
            "event_id": row["event_id"],
            "timestamp": row["timestamp"],
            "kind": row["kind"],
            "entity_type": row["entity_type"],
            "entity_id": row["entity_id"],
            "payload": _decode(row["payload_json"], {}),
        }
