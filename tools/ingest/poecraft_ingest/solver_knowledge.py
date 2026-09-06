"""Narrow Markdown traceability checks. Neither theorem nor runtime proof authority."""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from datetime import date
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote


STATUSES = {"open", "accepted", "refuted", "superseded", "withdrawn"}
RELATIONS = {"uses", "tests", "refutes", "obligation", "historical"}
FIELDS = ("Kind", "Statement", "Preconditions", "Canonical argument",
          "Logical dependencies", "Attempted falsification",
          "Evidence and implementation correspondence", "History")
CLAIM_ID = r"CLM-\d{4}"
LEDGER = Path("docs/solver/claims.md")


@dataclass(frozen=True)
class Claim:
    id: str
    title: str
    fields: dict[str, str]
    history: tuple[tuple[str, str, str], ...]

    @property
    def status(self) -> str:
        return self.history[-1][1]

    @property
    def dependencies(self) -> set[str]:
        return set(re.findall(CLAIM_ID, self.fields["Logical dependencies"]))


def parse_claims(text: str) -> dict[str, Claim]:
    result = {}
    blocks = list(re.finditer(rf"^## ({CLAIM_ID}) — (.+)$", text, re.M))
    for i, match in enumerate(blocks):
        cid, title = match.groups()
        if cid in result:
            raise ValueError(f"duplicate claim {cid}")
        body = text[match.end():blocks[i + 1].start() if i + 1 < len(blocks) else len(text)]
        labels = list(re.finditer(r"^\*\*([^*]+):\*\*", body, re.M))
        fields = {}
        for j, label in enumerate(labels):
            key = label[1]
            if key in fields:
                raise ValueError(f"{cid}: duplicate field {key}")
            value = body[label.end():labels[j + 1].start() if j + 1 < len(labels) else len(body)]
            fields[key] = re.sub(r'<a id="clm-\d{4}"></a>', '', value).strip()
        for field in FIELDS:
            if not fields.get(field):
                raise ValueError(f"{cid}: missing {field}")
        history = []
        for line in fields["History"].splitlines():
            event = re.fullmatch(r"- (\d{4}-\d{2}-\d{2}) — `([a-z]+)` — (.+)", line)
            if not event or event[2] not in STATUSES:
                raise ValueError(f"{cid}: invalid history event {line!r}")
            date.fromisoformat(event[1])
            # The prose identifies the real responsible reviewer and basis;
            # parsing it is not an independent assessment of that basis.
            if len(event[3].split()) < 5:
                raise ValueError(f"{cid}: history needs responsible author and basis")
            history.append(event.groups())
        if not history or [x[0] for x in history] != sorted(x[0] for x in history):
            raise ValueError(f"{cid}: missing or unordered history")
        result[cid] = Claim(cid, title, fields, tuple(history))
    if not result:
        raise ValueError("empty claim ledger")
    return result


def anchors(text: str) -> set[str]:
    found = set(re.findall(r'<a\s+id=["\x27]([^"\x27]+)', text))
    counts: Counter[str] = Counter()
    fenced = False
    for line in text.splitlines():
        if line.startswith(("```", "~~~")):
            fenced = not fenced
        if fenced:
            continue
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*$", line)
        if match:
            slug = re.sub(r"<[^>]+>|[`*_~]", "", match[1]).strip().lower()
            slug = re.sub(r"[^\w\- ]", "", slug).replace(" ", "-")
            number = counts[slug]
            counts[slug] += 1
            found.add(slug if not number else f"{slug}-{number}")
    return found


def local_path(root: Path, relative: str) -> Path:
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()) or path == (root / "0").resolve():
        raise ValueError(f"outside permitted repository paths: {relative}")
    return path


def check_links(root: Path, relative: str, text: str) -> list[str]:
    errors = []
    for target in re.findall(r'\[[^\]]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)', text):
        if re.match(r"[a-zA-Z][\w+.-]*:", target):
            continue  # External sources stay attributed; this is an offline check.
        file, _, anchor = unquote(target.strip("<>")).partition("#")
        try:
            path = local_path(root, str(Path(relative).parent / file) if file else relative)
            if not path.exists():
                errors.append(f"{relative}: missing link {target}")
            elif anchor and (not path.is_file() or anchor not in anchors(path.read_text(encoding="utf-8"))):
                errors.append(f"{relative}: missing anchor {target}")
        except (ValueError, UnicodeError) as error:
            errors.append(f"{relative}: {error}")
    return errors


def annotations(text: str):
    for number, line in enumerate(text.splitlines(), 1):
        prefix = r"^\s*(?://|#|/\*|\*)\s*math:\s+"
        match = re.search(prefix + rf"(\w+)\s+({CLAIM_ID})\b", line)
        if match:
            for cid in dict.fromkeys(re.findall(CLAIM_ID, line[match.start():])):
                yield number, match[1], cid
        elif re.search(prefix, line):
            raise ValueError(f"line {number}: malformed math annotation")


def tracked_sources(root: Path) -> list[str]:
    # Explicit source namespaces avoid generated data, private inputs and protected 0.
    return subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--",
         "engine", "tools/ingest", "bindings", "apps/web/src"],
        cwd=root, text=True, encoding="utf-8").splitlines()


def check(root: Path, *, source_paths: list[str] | None = None,
          base_text: str | None = None) -> dict:
    errors, warnings, changes = [], [], []
    claims = parse_claims((root / LEDGER).read_text(encoding="utf-8"))
    uses: dict[str, list[str]] = {cid: [] for cid in claims}
    visiting, visited = set(), set()

    def visit(cid):
        if cid in visiting:
            errors.append(f"logical dependency cycle at {cid}")
            return
        if cid in visited:
            return
        visiting.add(cid)
        for dep in sorted(claims[cid].dependencies):
            if dep not in claims:
                errors.append(f"{cid}: missing dependency {dep}")
            else:
                visit(dep)
                if claims[dep].status != "accepted":
                    warnings.append(f"{cid}: prerequisite {dep} is {claims[dep].status}")
                if claims[cid].status == "accepted" and claims[dep].status in {"refuted", "withdrawn"}:
                    errors.append(f"{cid}: accepted claim relies on {claims[dep].status} {dep}")
        visiting.remove(cid)
        visited.add(cid)

    for cid, claim in claims.items():
        visit(cid)
        if claim.status != "accepted":
            warnings.append(f"{cid}: {claim.status}")
        for field in ("Canonical argument", "Evidence and implementation correspondence"):
            if not re.search(r"\[[^\]]+\]\([^)]+\)", claim.fields[field]):
                errors.append(f"{cid}: {field} needs an evidence link")

    for path in sorted((root / "docs/solver").rglob("*.md")):
        if path.name == "architecture-history.md":
            continue  # Preserved historical index is not a current-contract rewrite.
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(root).as_posix()
        errors.extend(check_links(root, relative, text))
        for cid in set(re.findall(CLAIM_ID, text)) - claims.keys():
            errors.append(f"{relative}: unknown {cid}")

    for relative in source_paths if source_paths is not None else tracked_sources(root):
        path = local_path(root, relative)
        if path.suffix not in {".cpp", ".hpp", ".h", ".inc", ".py", ".ts"} or not path.is_file():
            continue
        try:
            for line, relation, cid in annotations(path.read_text(encoding="utf-8")):
                owner = f"{relative}:{line} {relation}"
                if relation not in RELATIONS or cid not in claims:
                    errors.append(f"{owner}: unknown relation/claim {cid}")
                    continue
                uses[cid].append(owner)
                status = claims[cid].status
                if relation == "uses" and status in {"refuted", "withdrawn"}:
                    errors.append(f"{owner}: active reliance on {status} {cid}")
                elif relation in {"uses", "obligation"} and status != "accepted":
                    warnings.append(f"{owner}: {cid} is {status}; review its premises")
        except ValueError as error:
            errors.append(f"{relative}: {error}")

    if base_text is not None:
        old = parse_claims(base_text)
        for cid, before in old.items():
            if cid not in claims:
                errors.append(f"{cid}: removed historical proposition; retain it")
                continue
            after = claims[cid]
            normalize = lambda x: " ".join(x.split())
            semantic = any(normalize(before.fields[f]) != normalize(after.fields[f])
                           for f in ("Statement", "Preconditions"))
            editorial = any("Editorial:" in event[2] for event in after.history[len(before.history):])
            if semantic and not editorial:
                errors.append(f"{cid}: statement/precondition changed; allocate a new ID")
            if after.history[:len(before.history)] != before.history:
                errors.append(f"{cid}: history is not append-only")
            if before != after:
                changes.append({"claim": cid, "kind": "declared_editorial_review" if semantic and editorial
                                else "semantic" if semantic else "editorial_or_history",
                                "affected_uses": uses[cid]})
    return {"scope": "traceability only; no mathematical or runtime certification",
            "claims": len(claims), "errors": sorted(set(errors)),
            "warnings": sorted(set(warnings)), "changes": changes, "uses": uses}


def export_context(root: Path, ids: list[str], *, question: str | None = None,
                   max_chars: int = 20000) -> str:
    claims = parse_claims((root / LEDGER).read_text(encoding="utf-8"))
    parts = ["# Selected solver research context\n\nTraceability is not native proof authority.\n"
             "Local links use docs/solver as their base; claim history remains in claims.md.\n"]
    if question:
        if not re.fullmatch(r"RQ-\d{3}", question):
            raise ValueError("invalid question ID")
        text = (root / "docs/solver/research.md").read_text(encoding="utf-8")
        match = re.search(rf"^### {question} — .+?(?=^<a id=|^## |\Z)", text, re.M | re.S)
        if not match:
            raise ValueError(f"unknown question {question}")
        parts.append(match[0])
        ids = ids + re.findall(CLAIM_ID, match[0])
    if not ids:
        raise ValueError("select a question or claim")
    for cid in dict.fromkeys(ids):
        if cid not in claims:
            raise ValueError(f"unknown claim {cid}")
        claim = claims[cid]
        parts.append(f"## {cid} — {claim.title}\n\nStatus: {claim.status}. Last event: {' — '.join(claim.history[-1])}\n")
        for field in FIELDS[:-1]:
            parts.append(f"**{field}:** {claim.fields[field]}\n")
        pending, seen = list(claim.dependencies), set()
        while pending:
            dep = pending.pop()
            if dep in seen:
                continue
            seen.add(dep)
            if dep not in claims:
                raise ValueError(f"missing prerequisite {dep}")
            if claims[dep].status != "accepted":
                parts.append(f"Premise warning: {dep} is {claims[dep].status}; its complete preconditions: {claims[dep].fields['Preconditions']}\n")
            pending.extend(claims[dep].dependencies - seen)
    # A selected export does not contain the entire ledger's anchor namespace.
    result = "\n".join(parts).replace("](\u0023clm-", "](claims.md#clm-")
    if len(result) > max_chars:
        raise ValueError(f"complete context needs {len(result)} characters; raise --max-chars or select fewer claims (nothing truncated)")
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    sub = parser.add_subparsers(dest="command", required=True)
    lint = sub.add_parser("lint")
    lint.add_argument("--base", help="Git revision whose claim identities/history must be preserved")
    context = sub.add_parser("context")
    context.add_argument("--claim", action="append", default=[])
    context.add_argument("--question")
    context.add_argument("--max-chars", type=int, default=20000)
    metadata = sub.add_parser("check-metadata", help="read-only series check/dry-run; never launches a solver")
    metadata.add_argument("path", type=Path)
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        if args.command == "context":
            print(export_context(root, args.claim, question=args.question, max_chars=args.max_chars))
            return 0
        if args.command == "check-metadata":
            from .solver_reports import build_research_report
            report = build_research_report(root, args.path)
        else:
            base_text = None
            if args.base:
                base_text = subprocess.check_output(["git", "show", f"{args.base}:{LEDGER.as_posix()}"],
                                                    cwd=root, text=True, encoding="utf-8")
            report = check(root, base_text=base_text)
        print(json.dumps(report, indent=2, ensure_ascii=True))
        return bool(report.get("errors"))
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
