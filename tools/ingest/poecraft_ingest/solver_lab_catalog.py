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


CATALOG_SCHEMA_VERSION = 2


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
                    blocked_reason TEXT,
                    cancel_requested INTEGER NOT NULL DEFAULT 0,
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
                    lease_id TEXT,
                    process_id INTEGER,
                    process_identity_token TEXT,
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

                CREATE TABLE IF NOT EXISTS supervisor_sessions (
                    supervisor_id TEXT PRIMARY KEY,
                    process_identity_token TEXT,
                    status TEXT NOT NULL,
                    started_at TEXT NOT NULL,
                    heartbeat_at TEXT NOT NULL,
                    stopped_at TEXT,
                    configuration_json TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS leases (
                    lease_id TEXT PRIMARY KEY,
                    attempt_id TEXT NOT NULL REFERENCES attempts(attempt_id),
                    job_id TEXT NOT NULL REFERENCES jobs(job_id),
                    supervisor_id TEXT NOT NULL REFERENCES supervisor_sessions(supervisor_id),
                    process_identity_token TEXT,
                    status TEXT NOT NULL,
                    acquired_at TEXT NOT NULL,
                    heartbeat_at TEXT NOT NULL,
                    released_at TEXT
                );
                CREATE INDEX IF NOT EXISTS leases_status
                    ON leases(status, heartbeat_at);
                """
            )
            self._ensure_column(connection, "jobs", "blocked_reason", "TEXT")
            self._ensure_column(
                connection,
                "jobs",
                "cancel_requested",
                "INTEGER NOT NULL DEFAULT 0",
            )
            self._ensure_column(connection, "attempts", "lease_id", "TEXT")
            self._ensure_column(connection, "attempts", "process_id", "INTEGER")
            self._ensure_column(
                connection, "attempts", "process_identity_token", "TEXT"
            )
            connection.execute(
                "INSERT INTO catalog_metadata(key, value) VALUES('schema_version', ?) "
                "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                (str(CATALOG_SCHEMA_VERSION),),
            )
            connection.execute(f"PRAGMA user_version = {CATALOG_SCHEMA_VERSION}")

    @staticmethod
    def _ensure_column(
        connection: sqlite3.Connection,
        table: str,
        column: str,
        declaration: str,
    ) -> None:
        columns = {
            row[1] for row in connection.execute(f"PRAGMA table_info({table})")
        }
        if column not in columns:
            connection.execute(
                f"ALTER TABLE {table} ADD COLUMN {column} {declaration}"
            )

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

    def record_operation(
        self,
        *,
        command_id: str,
        idempotency_key: str,
        operation: str,
        target_id: str | None,
        request: Mapping[str, Any],
        result: Mapping[str, Any],
    ) -> dict[str, Any]:
        with self.transaction(immediate=True) as connection:
            existing = connection.execute(
                "SELECT * FROM commands WHERE idempotency_key=?",
                (idempotency_key,),
            ).fetchone()
            if existing:
                return _decode(existing["result_json"], {})
            self._insert_command(
                connection,
                command_id=command_id,
                idempotency_key=idempotency_key,
                operation=operation,
                target_id=target_id,
                dry_run=False,
                request=request,
                result=result,
            )
        return dict(result)

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

    def list_dispatch_candidates(self, *, limit: int = 200) -> list[dict[str, Any]]:
        limit = max(1, min(int(limit), 1000))
        with self._connect() as connection:
            rows = connection.execute(
                "SELECT * FROM jobs WHERE status IN ('queued', 'blocked') "
                "ORDER BY priority DESC, created_at ASC, job_id ASC LIMIT ?",
                (limit,),
            ).fetchall()
        return [self._job_row(row) for row in rows]

    def claim_job(
        self,
        *,
        job_id: str,
        supervisor_id: str,
        attempt_id: str,
        attempt_directory: Path,
        lease_id: str | None = None,
    ) -> tuple[dict[str, Any], dict[str, Any]] | None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            row = connection.execute(
                "SELECT * FROM jobs WHERE job_id=? AND status IN ('queued', 'blocked')",
                (job_id,),
            ).fetchone()
            if row is None:
                return None
            job = self._job_row(row)
            ordinal = int(
                connection.execute(
                    "SELECT COALESCE(MAX(ordinal), 0) + 1 FROM attempts WHERE job_id=?",
                    (job_id,),
                ).fetchone()[0]
            )
            updated = connection.execute(
                "UPDATE jobs SET status='running', blocked_reason=NULL, "
                "cancel_requested=0, updated_at=? "
                "WHERE job_id=? AND status IN ('queued', 'blocked')",
                (now, job_id),
            )
            if updated.rowcount != 1:
                return None
            connection.execute(
                """
                INSERT INTO attempts(
                    attempt_id, schema_version, job_id, ordinal, status,
                    directory, supervisor_id, lease_id, created_at, started_at
                ) VALUES(?, ?, ?, ?, 'running', ?, ?, ?, ?, ?)
                """,
                (
                    attempt_id,
                    ATTEMPT_SCHEMA_VERSION,
                    job_id,
                    ordinal,
                    str(attempt_directory.resolve()),
                    supervisor_id,
                    lease_id,
                    now,
                    now,
                ),
            )
            if lease_id is not None:
                connection.execute(
                    """
                    INSERT INTO leases(
                        lease_id, attempt_id, job_id, supervisor_id,
                        process_identity_token, status, acquired_at,
                        heartbeat_at, released_at
                    ) VALUES(?, ?, ?, ?, NULL, 'active', ?, ?, NULL)
                    """,
                    (lease_id, attempt_id, job_id, supervisor_id, now, now),
                )
            self._insert_event(
                connection,
                "attempt_started",
                "attempt",
                attempt_id,
                {"job_id": job_id, "ordinal": ordinal, "lease_id": lease_id},
            )
        job["status"] = "running"
        job["blocked_reason"] = None
        attempt = self.get_attempt(attempt_id)
        assert attempt is not None
        return job, attempt

    def claim_next_job(
        self,
        *,
        supervisor_id: str,
        attempt_id: str,
        attempt_directory: Path,
        lease_id: str | None = None,
    ) -> tuple[dict[str, Any], dict[str, Any]] | None:
        candidates = self.list_dispatch_candidates(limit=1)
        if not candidates:
            return None
        return self.claim_job(
            job_id=candidates[0]["job_id"],
            supervisor_id=supervisor_id,
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
            lease_id=lease_id,
        )

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

    def set_attempt_process(
        self,
        *,
        attempt_id: str,
        process_id: int,
        process_identity_token: str | None,
    ) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            row = connection.execute(
                "SELECT lease_id FROM attempts WHERE attempt_id=?", (attempt_id,)
            ).fetchone()
            if row is None:
                raise KeyError(attempt_id)
            connection.execute(
                "UPDATE attempts SET process_id=?, process_identity_token=? "
                "WHERE attempt_id=?",
                (process_id, process_identity_token, attempt_id),
            )
            if row["lease_id"]:
                connection.execute(
                    "UPDATE leases SET process_identity_token=?, heartbeat_at=? "
                    "WHERE lease_id=?",
                    (process_identity_token, now, row["lease_id"]),
                )
            self._insert_event(
                connection,
                "attempt_process_started",
                "attempt",
                attempt_id,
                {
                    "process_id": process_id,
                    "process_identity_token": process_identity_token,
                },
            )

    def start_supervisor_session(
        self,
        *,
        supervisor_id: str,
        process_identity_token: str | None,
        configuration: Mapping[str, Any],
    ) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            connection.execute(
                """
                INSERT INTO supervisor_sessions(
                    supervisor_id, process_identity_token, status, started_at,
                    heartbeat_at, stopped_at, configuration_json
                ) VALUES(?, ?, 'active', ?, ?, NULL, ?)
                ON CONFLICT(supervisor_id) DO UPDATE SET
                    process_identity_token=excluded.process_identity_token,
                    status='active', heartbeat_at=excluded.heartbeat_at,
                    stopped_at=NULL, configuration_json=excluded.configuration_json
                """,
                (
                    supervisor_id,
                    process_identity_token,
                    now,
                    now,
                    _json(configuration),
                ),
            )

    def heartbeat_supervisor(self, supervisor_id: str) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "UPDATE supervisor_sessions SET heartbeat_at=? "
                "WHERE supervisor_id=? AND status='active'",
                (now, supervisor_id),
            )
            connection.execute(
                "UPDATE leases SET heartbeat_at=? WHERE supervisor_id=? AND status='active'",
                (now, supervisor_id),
            )

    def stop_supervisor_session(self, supervisor_id: str) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "UPDATE supervisor_sessions SET status='stopped', "
                "heartbeat_at=?, stopped_at=? WHERE supervisor_id=?",
                (now, now, supervisor_id),
            )

    def list_supervisor_sessions(self, *, limit: int = 20) -> list[dict[str, Any]]:
        limit = max(1, min(int(limit), 100))
        with self._connect() as connection:
            rows = connection.execute(
                "SELECT * FROM supervisor_sessions ORDER BY started_at DESC LIMIT ?",
                (limit,),
            ).fetchall()
        return [
            {
                "supervisor_id": row["supervisor_id"],
                "process_identity_token": row["process_identity_token"],
                "status": row["status"],
                "started_at": row["started_at"],
                "heartbeat_at": row["heartbeat_at"],
                "stopped_at": row["stopped_at"],
                "configuration": _decode(row["configuration_json"], {}),
            }
            for row in rows
        ]

    def stale_attempts(self, *, heartbeat_before: str) -> list[dict[str, Any]]:
        with self._connect() as connection:
            rows = connection.execute(
                """
                SELECT a.* FROM attempts a
                LEFT JOIN leases l ON l.lease_id = a.lease_id
                LEFT JOIN supervisor_sessions s ON s.supervisor_id = a.supervisor_id
                WHERE a.status IN ('running', 'canceling')
                  AND (
                    a.lease_id IS NULL OR l.status != 'active'
                    OR l.heartbeat_at < ? OR s.status != 'active'
                    OR s.heartbeat_at < ?
                  )
                ORDER BY a.created_at, a.attempt_id
                """,
                (heartbeat_before, heartbeat_before),
            ).fetchall()
        return [self._attempt_row(row) for row in rows]

    def mark_attempt_orphaned(
        self,
        *,
        attempt_id: str,
        job_status: str,
        recovery: Mapping[str, Any],
    ) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            row = connection.execute(
                "SELECT job_id, lease_id FROM attempts WHERE attempt_id=?",
                (attempt_id,),
            ).fetchone()
            if row is None:
                raise KeyError(attempt_id)
            connection.execute(
                "UPDATE attempts SET status='orphaned', result_json=?, finished_at=? "
                "WHERE attempt_id=?",
                (_json(recovery), now, attempt_id),
            )
            connection.execute(
                "UPDATE jobs SET status=?, cancel_requested=0, updated_at=? "
                "WHERE job_id=?",
                (job_status, now, row["job_id"]),
            )
            if row["lease_id"]:
                connection.execute(
                    "UPDATE leases SET status='recovered_orphan', released_at=? "
                    "WHERE lease_id=?",
                    (now, row["lease_id"]),
                )
            self._insert_event(
                connection,
                "attempt_recovered_orphan",
                "attempt",
                attempt_id,
                dict(recovery),
            )

    def mark_job_blocked(self, job_id: str, reason: str) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "UPDATE jobs SET status='blocked', blocked_reason=?, updated_at=? "
                "WHERE job_id=? AND status IN ('queued', 'blocked')",
                (reason, now, job_id),
            )

    def queue_paused(self) -> bool:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT value_json FROM settings WHERE key='queue_paused'"
            ).fetchone()
        return bool(_decode(row[0], False)) if row else False

    def set_queue_paused(self, paused: bool) -> None:
        now = utc_now()
        with self.transaction(immediate=True) as connection:
            connection.execute(
                "INSERT INTO settings(key, value_json, updated_at) VALUES('queue_paused', ?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value_json=excluded.value_json, "
                "updated_at=excluded.updated_at",
                (_json(bool(paused)), now),
            )

    def is_cancel_requested(self, job_id: str) -> bool:
        with self._connect() as connection:
            row = connection.execute(
                "SELECT cancel_requested FROM jobs WHERE job_id=?", (job_id,)
            ).fetchone()
        return bool(row and row[0])

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
                "SELECT job_id, lease_id FROM attempts WHERE attempt_id=?", (attempt_id,)
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
                "UPDATE jobs SET status=?, cancel_requested=0, blocked_reason=NULL, "
                "updated_at=? WHERE job_id=?",
                (job_status, now, job_id),
            )
            if row["lease_id"]:
                connection.execute(
                    "UPDATE leases SET status='released', heartbeat_at=?, released_at=? "
                    "WHERE lease_id=?",
                    (now, now, row["lease_id"]),
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

    def request_cancel_job(
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
            current = str(row["status"])
            if current in {"queued", "blocked"}:
                target = "canceled"
                requested = 0
            elif current in {"running", "canceling"}:
                target = "canceling"
                requested = 1
            else:
                raise ValueError(f"cannot cancel terminal job in status={current}")
            connection.execute(
                "UPDATE jobs SET status=?, cancel_requested=?, updated_at=? WHERE job_id=?",
                (target, requested, now, job_id),
            )
            if target == "canceling":
                connection.execute(
                    "UPDATE attempts SET status='canceling' "
                    "WHERE job_id=? AND status='running'",
                    (job_id,),
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
                connection,
                "job_cancel_requested" if requested else "job_canceled",
                "job",
                job_id,
                {"from": current, "to": target},
            )
        return dict(operation_result)

    def retry_job(
        self,
        *,
        job_id: str,
        command_id: str,
        idempotency_key: str,
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
            if row["status"] in {"queued", "blocked", "running", "canceling"}:
                raise ValueError(f"cannot retry active job in status={row['status']}")
            connection.execute(
                "UPDATE jobs SET status='queued', blocked_reason=NULL, "
                "cancel_requested=0, updated_at=? WHERE job_id=?",
                (now, job_id),
            )
            self._insert_command(
                connection,
                command_id=command_id,
                idempotency_key=idempotency_key,
                operation="retry_job",
                target_id=job_id,
                dry_run=False,
                request={"job_id": job_id},
                result=operation_result,
            )
            self._insert_event(
                connection,
                "job_retried",
                "job",
                job_id,
                {"from": row["status"], "to": "queued"},
            )
        return dict(operation_result)

    def clone_job(
        self,
        *,
        source_job_id: str,
        job: Mapping[str, Any],
        command_id: str,
        idempotency_key: str,
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
            if connection.execute(
                "SELECT 1 FROM jobs WHERE job_id=?", (source_job_id,)
            ).fetchone() is None:
                raise KeyError(source_job_id)
            connection.execute(
                """
                INSERT INTO jobs(
                    job_id, schema_version, experiment_id, case_id, case_path,
                    profile_id, priority, status, watchdog_seconds,
                    reserved_memory_bytes, identity_sha256, request_json,
                    created_at, updated_at
                ) VALUES(?, ?, ?, ?, ?, ?, ?, 'queued', ?, ?, ?, ?, ?, ?)
                """,
                (
                    job["job_id"],
                    JOB_SCHEMA_VERSION,
                    job.get("experiment_id"),
                    job["case_id"],
                    job["case_path"],
                    job["profile_id"],
                    job["priority"],
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
                operation="clone_job",
                target_id=str(job["job_id"]),
                dry_run=False,
                request={"source_job_id": source_job_id},
                result=operation_result,
            )
            self._insert_event(
                connection,
                "job_cloned",
                "job",
                str(job["job_id"]),
                {"source_job_id": source_job_id},
            )
        return dict(operation_result)

    def change_priority(
        self,
        *,
        job_id: str,
        priority: int,
        command_id: str,
        idempotency_key: str,
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
                "SELECT priority, status FROM jobs WHERE job_id=?", (job_id,)
            ).fetchone()
            if row is None:
                raise KeyError(job_id)
            if row["status"] not in {"queued", "blocked"}:
                raise ValueError("priority may change only before dispatch")
            connection.execute(
                "UPDATE jobs SET priority=?, updated_at=? WHERE job_id=?",
                (priority, now, job_id),
            )
            self._insert_command(
                connection,
                command_id=command_id,
                idempotency_key=idempotency_key,
                operation="change_priority",
                target_id=job_id,
                dry_run=False,
                request={"job_id": job_id, "priority": priority},
                result=operation_result,
            )
            self._insert_event(
                connection,
                "job_priority_changed",
                "job",
                job_id,
                {"from": row["priority"], "to": priority},
            )
        return dict(operation_result)

    def set_queue_paused_command(
        self,
        *,
        paused: bool,
        command_id: str,
        idempotency_key: str,
        operation_result: Mapping[str, Any],
    ) -> dict[str, Any]:
        now = utc_now()
        operation = "pause_queue" if paused else "resume_queue"
        with self.transaction(immediate=True) as connection:
            existing = connection.execute(
                "SELECT * FROM commands WHERE idempotency_key=?",
                (idempotency_key,),
            ).fetchone()
            if existing:
                return _decode(existing["result_json"], {})
            connection.execute(
                "INSERT INTO settings(key, value_json, updated_at) VALUES('queue_paused', ?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value_json=excluded.value_json, "
                "updated_at=excluded.updated_at",
                (_json(paused), now),
            )
            self._insert_command(
                connection,
                command_id=command_id,
                idempotency_key=idempotency_key,
                operation=operation,
                target_id=None,
                dry_run=False,
                request={"paused": paused},
                result=operation_result,
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

    def list_artifacts(self, attempt_id: str) -> list[dict[str, Any]]:
        with self._connect() as connection:
            rows = connection.execute(
                "SELECT * FROM artifacts WHERE attempt_id=? ORDER BY kind, path",
                (attempt_id,),
            ).fetchall()
        return [
            {
                "schema_version": row["schema_version"],
                "artifact_id": row["artifact_id"],
                "attempt_id": row["attempt_id"],
                "kind": row["kind"],
                "path": row["path"],
                "content_sha256": row["content_sha256"],
                "size_bytes": row["size_bytes"],
                "created_at": row["created_at"],
            }
            for row in rows
        ]

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
            "blocked_reason": row["blocked_reason"],
            "cancel_requested": bool(row["cancel_requested"]),
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
            "lease_id": row["lease_id"],
            "process_id": row["process_id"],
            "process_identity_token": row["process_identity_token"],
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
