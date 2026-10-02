"""Read the author-linked XLSX with the standard library; no runtime model."""
import collections, hashlib, json, re, sys, zipfile
import xml.etree.ElementTree as E
from pathlib import Path
p = Path(sys.argv[1])
n = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
report = {"dataset_sha256": hashlib.sha256(p.read_bytes()).hexdigest(), "sheets": {},
          "remembrance_counts": {}, "consumption": {}}
with zipfile.ZipFile(p) as z:
    ss = ["".join(e.itertext()) for e in E.fromstring(z.read("xl/sharedStrings.xml")).findall("s:si", n)] if "xl/sharedStrings.xml" in z.namelist() else []
    links = {e.attrib["Id"]: e.attrib["Target"].lstrip("/") for e in E.fromstring(z.read("xl/_rels/workbook.xml.rels"))}
    for s in E.fromstring(z.read("xl/workbook.xml")).findall("s:sheets/s:sheet", n):
        name = s.attrib["name"]
        if name not in ("remembrance data", "foulborn and misc currency data") and "orb of alter" not in name: continue
        path = links[s.attrib["{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id"]]
        if not path.startswith("xl/"): path = "xl/" + path
        rows = E.fromstring(z.read(path)).findall("s:sheetData/s:row", n)
        report["sheets"][name] = len(rows) - 1
        for row in rows[1:]:
            values = {}
            for c in row.findall("s:c", n):
                v = c.find("s:v", n)
                text = v.text if v is not None else "".join(c.find("s:is", n).itertext()) if c.find("s:is", n) is not None else ""
                if c.attrib.get("t") == "s": text = ss[int(text)]
                values[re.sub(r"[0-9]+", "", c.attrib["r"])] = text
            if name == "remembrance data":
                count = str(int(float(values["A"])))
                report["remembrance_counts"][count] = report["remembrance_counts"].get(count, 0) + 1
                continue
            if "orb of alter" in name:
                before, after, action = int(float(values["A"])), int(float(values["B"])), "Orb of Alteration"
            else:
                match = re.search(r"Memory Strands: (\d+)", values.get("J", ""))
                if not match: continue
                before = int(match.group(1))
                match = re.search(r"Memory Strands: (\d+)", values.get("K", ""))
                after = int(match.group(1)) if match else 0
                action = values["C"]
            record = report["consumption"].setdefault(action, {"rows": 0, "uncensored_input_ge_40": {}, "clipped_to_zero": 0})
            record["rows"] += 1
            if after == 0: record["clipped_to_zero"] += 1
            if before >= 40 and after > 0:
                delta = str(before - after)
                h = record["uncensored_input_ge_40"]
                h[delta] = h.get(delta, 0) + 1
report["unit"] = "recorded XLSX rows; misc currency rows may repeat a craft for different modifiers; do not infer independent trial counts"
report["label"] = "empirical observations only; no approved generating law"
Path(sys.argv[2]).write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
for action, data in report["consumption"].items(): print(action, data)
counts = report["remembrance_counts"]
print("Remembrance", sum(counts.values()), "range", min(map(int, counts)), max(map(int, counts)), "mean", sum(int(k)*v for k,v in counts.items())/sum(counts.values()), "sha", report["dataset_sha256"])
