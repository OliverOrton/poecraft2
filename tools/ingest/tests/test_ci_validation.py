from __future__ import annotations

import json
from pathlib import Path
import subprocess

import pytest

from poecraft_ingest.ci_changes import validation_identity
from poecraft_ingest.ci_validation import STAGE_LIMITS, main, run_stage


@pytest.mark.parametrize(("process", "status"), [
    ({"exit_code": 0}, "passed"), ({"exit_code": 1}, "failed"),
    ({"exit_code": 1, "timed_out": True}, "timed_out"),
    ({"exit_code": 1, "canceled": True}, "canceled"),
    ({"exit_code": 0, "survivor": True}, "cleanup_failed"),
    ({"exit_code": 0, "descendants_after_parent_exit": True}, "cleanup_failed"),
])
def test_stage_records_start_terminal_cause_and_bounded_tail(tmp_path, monkeypatch, process, status):
    output = tmp_path / "stage"
    monkeypatch.setattr("poecraft_ingest.ci_validation.validation_identity", lambda *_: "identity")
    monkeypatch.setattr("poecraft_ingest.ci_validation.git_provenance", lambda *_: {"commit": "source", "dirty": False})
    def run(command, **kwargs):
        start = json.loads((output / "stage.json").read_text())
        assert start["status"] == "running"
        assert start["command"] == ["test", "two"]
        assert kwargs["watchdog_seconds"] == 0.5
        assert kwargs["log_path"] == output / "stage.log"
        kwargs["log_path"].write_text("full stream")
        return {"exit_code": 0, "survivor": False, "timed_out": False,
                "canceled": False, "output": "bounded tail", **process}
    monkeypatch.setattr("poecraft_ingest.ci_validation.run_isolated_process", run)
    result = run_stage(root=tmp_path, scope="Python", output=output,
                       command=["test", "two"], watchdog_seconds=0.5)
    saved = json.loads((output / "stage.json").read_text())
    assert saved == result
    assert saved["status"] == status and saved["ended_at"] >= saved["started_at"]
    assert (output / "tail.log").read_text() == "bounded tail"
    with pytest.raises(ValueError, match="fresh"):
        run_stage(root=tmp_path, scope="Python", output=output,
                  command=["test"], watchdog_seconds=0.5)


def test_stage_keeps_atomic_failure_evidence_on_observer_error(tmp_path, monkeypatch):
    monkeypatch.setattr("poecraft_ingest.ci_validation.validation_identity", lambda *_: "identity")
    monkeypatch.setattr("poecraft_ingest.ci_validation.git_provenance", lambda *_: {})
    def fail(*args, **kwargs):
        raise RuntimeError("observer failed")
    monkeypatch.setattr("poecraft_ingest.ci_validation.run_isolated_process", fail)
    with pytest.raises(RuntimeError):
        run_stage(root=tmp_path, scope="Native", output=tmp_path / "stage",
                  command=["test"], watchdog_seconds=0.5)
    result = json.loads((tmp_path / "stage/stage.json").read_text())
    assert result["status"] == "error" and result["ended_at"]
    assert "observer failed" in result["error"]


@pytest.mark.parametrize("scope", list(STAGE_LIMITS))
def test_stage_cli_preserves_scopes_and_outer_limits(tmp_path, monkeypatch, scope):
    monkeypatch.setattr("poecraft_ingest.ci_validation.shutil.which", lambda _: "pwsh")
    received = {}
    def run(**kwargs):
        received.update(kwargs)
        return {"status": "passed"}
    monkeypatch.setattr("poecraft_ingest.ci_validation.run_stage", run)
    assert main(["--root", str(tmp_path), "--scope", scope, "--output", str(tmp_path / "out")]) == 0
    assert received["watchdog_seconds"] == STAGE_LIMITS[scope]
    argv = received["command"]
    assert argv[argv.index("-Scope") + 1] == scope
    assert "-SkipBuild" in argv
    assert ("-FetchPinnedData" in argv) is (scope == "Prepare")
    assert ("-SkipDataPreparation" in argv) is (scope != "Prepare")
    assert ("-InstallTestBrowser" in argv) is (scope == "Web")


def test_validation_identity_ignores_only_known_prose_and_binds_deleted_source(tmp_path):
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=tmp_path).decode().strip()
    git("init", "-q")
    git("config", "user.name", "Fixture")
    git("config", "user.email", "fixture@example.invalid")
    (tmp_path / "engine").mkdir()
    (tmp_path / "docs").mkdir()
    (tmp_path / "engine/source.cpp").write_text("first")
    (tmp_path / "docs/plan.md").write_text("first")
    git("add", "engine/source.cpp", "docs/plan.md")
    git("commit", "-qm", "initial")
    original = git("rev-parse", "HEAD")
    initial = validation_identity(tmp_path)
    (tmp_path / "docs/plan.md").write_text("changed prose")
    git("add", "docs/plan.md")
    git("commit", "-qm", "prose")
    assert validation_identity(tmp_path) == initial
    (tmp_path / "engine/source.cpp").unlink()
    git("add", "engine/source.cpp")
    git("commit", "-qm", "removed native input")
    assert validation_identity(tmp_path, original) == initial
    assert validation_identity(tmp_path) != initial
