# Native Solver Lab CLI-First MCP Removal v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Gate 0 activation

### Repository boundary

- HEAD: `ecf778c0ab9b8e3ebbeb285ca7748774845c325f`
- Parent: `13a53219f9585b048d1a0156c8debcaa13cef933`
- Branch: `main`
- Upstream divergence: `origin/main...HEAD = 0/0`
- Status: only protected `?? 0`; no other dirty path

### Existing automation surfaces

- CLI entry point: `poecraft-solver-lab`
- CLI schema: `solver_lab_operation_result_v1`
- MCP entry point selected for removal: `poecraft-solver-lab-mcp`
- MCP server version selected for removal: `0.2.0`
- MCP typed tool count selected for removal: `31`
- User-local registration selected for removal:
  `poecraft2-native-solver-lab`
- Registered command:
  `poecraft-solver-lab-mcp.exe --root C:\Users\Oliver\Documents\poecraft2 --with-supervisor --max-workers 1`

### Live MCP process inventory

Four exact repository-specific launcher/process trees were live:

| launcher PID | Python PID | disposition at activation |
| ---: | ---: | --- |
| 8264 | 53128 | catalog dispatcher owner |
| 18448 | 18468 | control-only |
| 12180 | 3500 | control-only |
| 13808 | 25780 | control-only |

The active catalog dispatcher was
`supervisor-e1363676-96b8-486d-bb2c-1050479f1f1e`, PID `53128`. The queue
was resumed. Job counts contained only terminal states: 26 canceled, 71
completed, 34 dispatch-refused, 11 failed, and 12 partial. No active host
reservation or nonterminal job/attempt was observed.

### Existing CLI evidence

Read-only CLI calls returned:

- one `native_allflame_no_imprint_v1` profile;
- exactly seven frozen cases;
- `fragment-clean-one-goal-renewal-control-v1`;
- `fragment-clean-one-goal-renewal-shadow-v1`; and
- the same catalog dispatcher, queue, host-resource, and durable session
  information available through the shared service.

This establishes that removal does not require a replacement serialization
layer. Subsequent gates retain and qualify the existing JSON and artifact
contracts.
