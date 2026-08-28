# Native Solver Lab v0 corpus

This five-case corpus freezes the first Solver Lab comparison surface. It is
not a release acceptance corpus and should not be widened casually.

The orchestration profile is
`profiles/native_allflame_no_imprint_v1.json`. The profile identifies existing
native benchmark controls; it does not reproduce crafting mechanics in Python.
The cases retain native exact terminal semantics, fixed Allflame pricing,
Calculator-product action scope, automatic Imprint programs off, voluntary
economic Restart off, and independent exact strategy evaluation.

Install the optional local GUI and MCP dependencies from the repository root:

```powershell
py -3 -m pip install -e "tools/ingest[solver-lab]"
```

The corpus deliberately contains one exact three-prefix case, one exact
three-suffix case, the bounded four-mod PDR case, a non-armour four-goal Bow
control, and a partial four-to-five carrier control.
