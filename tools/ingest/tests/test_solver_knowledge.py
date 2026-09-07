from pathlib import Path
import tempfile
import unittest
import subprocess

from poecraft_ingest.solver_knowledge import check, export_context, parse_claims


def claim(cid="CLM-0001", status="open", deps="None."):
    return f'''<a id="{cid.lower()}"></a>
## {cid} — Scoped proposition

**Kind:** mathematics.
**Statement:** A bounded potential is conditional evidence.
**Preconditions:** Proper policy, complete scope, bounded tail.
**Canonical argument:** [argument](argument.md#proof).
**Logical dependencies:** {deps}
**Attempted falsification:** Preserve the zero-cost cycle.
**Evidence and implementation correspondence:** [fixture](argument.md#proof).
**History:**
- 2026-09-06 — `{status}` — Test author; explicit synthetic basis for parser fixture.
'''


class KnowledgeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.docs = self.root / "docs/solver"
        self.docs.mkdir(parents=True)
        (self.docs / "argument.md").write_text("# Proof\n", encoding="utf-8")
        (self.docs / "research.md").write_text("### RQ-001 — Target\n\nCLM-0001\n", encoding="utf-8")
        self.ledger = self.docs / "claims.md"
        self.ledger.write_text(claim(), encoding="utf-8")
        self.source = self.root / "owner.cpp"
        self.source.write_text("", encoding="utf-8")

    def run_check(self, **kwargs):
        return check(self.root, source_paths=["owner.cpp"], **kwargs)

    def test_open_is_visible_and_not_automatic_error(self):
        result = self.run_check()
        self.assertEqual(result["errors"], [])
        self.assertIn("CLM-0001: open", result["warnings"])

    def test_relation_roles_and_refuted_use(self):
        self.ledger.write_text(claim(status="refuted"), encoding="utf-8")
        for relation in ("historical", "tests", "refutes", "obligation", "uses"):
            self.source.write_text("// math: " + relation + " CLM-0001\n", encoding="utf-8")
            result = self.run_check()
            self.assertEqual(bool(result["errors"]), relation == "uses")
        self.source.write_text("// math: invented CLM-0099\n", encoding="utf-8")
        self.assertTrue(self.run_check()["errors"])

    def test_missing_ids_links_fields_and_history(self):
        for text in (claim().replace("argument.md", "absent.md"),
                     claim().replace("#proof", "#missing"),
                     claim(deps="CLM-0999")):
            self.ledger.write_text(text, encoding="utf-8")
            self.assertTrue(self.run_check()["errors"])
        for text in (claim().replace("**Statement:**", "**Other:**"),
                     claim(status="invented"), claim() + claim(),
                     claim().replace("2026-09-06", "2026-99-99")):
            with self.assertRaises(ValueError):
                parse_claims(text)

    def test_dependency_cycle_and_rejected_premise(self):
        self.ledger.write_text(claim(deps="CLM-0002") + claim("CLM-0002", deps="CLM-0001"), encoding="utf-8")
        self.assertTrue(any("cycle" in x for x in self.run_check()["errors"]))
        self.ledger.write_text(claim(status="accepted", deps="CLM-0002") + claim("CLM-0002", "withdrawn"), encoding="utf-8")
        self.assertTrue(any("relies on withdrawn" in x for x in self.run_check()["errors"]))

    def test_editorial_changes_and_append_only_history(self):
        original = claim()
        self.source.write_text("// math: uses CLM-0001\n", encoding="utf-8")
        self.ledger.write_text(original.replace("Preserve the zero-cost cycle", "Retain the zero-cost cycle"), encoding="utf-8")
        result = self.run_check(base_text=original)
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["changes"][0]["affected_uses"], ["owner.cpp:1 uses"])
        self.ledger.write_text(original.replace("bounded tail", "arbitrary tail"), encoding="utf-8")
        self.assertTrue(any("new ID" in x for x in self.run_check(base_text=original)["errors"]))
        self.ledger.write_text(original.replace("`open`", "`accepted`"), encoding="utf-8")
        self.assertTrue(any("append-only" in x for x in self.run_check(base_text=original)["errors"]))
        self.ledger.write_text(original + "- 2026-09-07 — `accepted` — Test reviewer checked the unchanged conditional argument.\n", encoding="utf-8")
        self.assertEqual(self.run_check(base_text=original)["errors"], [])
        self.ledger.write_text(original.replace("Proper policy,", "Proper policy;") +
            "- 2026-09-07 — `open` — Test reviewer. Editorial: punctuation only; proposition and premises unchanged.\n", encoding="utf-8")
        reviewed = self.run_check(base_text=original)
        self.assertEqual(reviewed["errors"], [])
        self.assertEqual(reviewed["changes"][0]["kind"], "declared_editorial_review")

    def test_export_preserves_complete_premises_or_refuses(self):
        self.ledger.write_text(claim(status="accepted", deps="[CLM-0002](#clm-0002)") + claim("CLM-0002", "superseded"), encoding="utf-8")
        (self.docs / "research.md").write_text("### RQ-001 — Target\n\nCLM-0001 [local](#rq-001)\n", encoding="utf-8")
        self.commit_context()
        text = export_context(self.root, [], question="RQ-001")
        self.assertIn("Proper policy, complete scope, bounded tail.", text)
        self.assertIn("CLM-0002 is superseded", text)
        self.assertIn("/docs/solver/claims.md#clm-0002)", text)
        self.assertIn("/docs/solver/research.md#rq-001)", text)
        self.assertIn("Source revision:", text)
        self.assertIn("not observed in local remote refs", text)
        with self.assertRaisesRegex(ValueError, "nothing truncated"):
            export_context(self.root, ["CLM-0001"], max_chars=20)
        with self.assertRaisesRegex(ValueError, "unknown claim"):
            export_context(self.root, ["CLM-9999"])

    def commit_context(self):
        for args in (["init", "-q"], ["add", "docs/solver"],
                     ["-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-qm", "context fixture"]):
            subprocess.run(["git", *args], cwd=self.root, check=True, capture_output=True)

    def test_context_refuses_dirty_linked_content_and_preserves_historical_pins(self):
        self.ledger.write_text(claim().replace("Preserve the zero-cost cycle.",
            "[Historical](https://example.com/old#proof)."), encoding="utf-8")
        self.commit_context()
        text=export_context(self.root,["CLM-0001"])
        self.assertIn("(https://example.com/old#proof)",text)
        self.assertIn("/docs/solver/argument.md#proof)",text)
        (self.docs/"argument.md").write_text("# Changed\n",encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"uncommitted context source docs/solver/argument.md"):
            export_context(self.root,["CLM-0001"])
        self.ledger.write_text(claim()+"\n",encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"uncommitted context source docs/solver/claims.md"):
            export_context(self.root,["CLM-0001"])


if __name__ == "__main__":
    unittest.main()
