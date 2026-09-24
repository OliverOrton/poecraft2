# Minimal candidate/control interface for the first native consumer

This is a proposed in-memory construction view, **not a new public strategy-file format**. Its final output is the existing ordinary strategy document. Reuse existing compiler representations when they already express these fields.

## 1. Small control vocabulary

```text
Candidate:
    problem binding
    native role/parameter bindings
    start control node
    finite control nodes
    lineage and untrusted proposal statistics

Control node:
    Test(native expressible condition, true successor, false successor)
    Run(stable native primitive/program specification, continuation)
    GoalTerminal                 # created/guarded by request-bound assembler
    FailureTerminal              # accepted only if unreachable
    UnresolvedHole               # search only; cannot be published
```

A `Run` uses the **existing native program compiler**, including retries, observation choices and mandatory cleanup. It is NOT implemented by blindly concatenating an option's primitive list. Some options have control structure, resource accounting or normalization that a raw list omits. Unknown/unsupported program compilation is a typed refusal.

Conditions are existing native conditions or conditions emitted by a native-supported builder. `is_action_legal(...)`, `predicted_value(...)` or `model(...)` are NOT new runtime predicates silently invented by this design. A source-side legality query can reject a proposal; the final graph still needs expressible guards and correct behavior for every reached item. A representative item's legality does not establish class-wide legality.

## 2. A concrete seed and coordinated family

**Seed shape:** test the original native goal; otherwise run one supported native renewal/attempt program; return to the test. Native acceptance determines whether it is legal, complete, proper and affordable. The source pays any necessary initiation and recovery. The graph does not reset the physical item merely because its control returns to the first node.

**First coordinated alternative shape:** acquisition loop for a selected role/subset, followed by a completion/protection/recovery phase, with explicit branches returning control to acquisition when progress is lost. The choices of acquisition program, role subset, completion program, and recovery are holes filled from the original native grammar. The same parameterized construction can bind a bow or armour request without equating their transition laws.

Search must compare at least one coordinated binding that cannot be obtained merely by changing the stopping label of an otherwise identical selected kernel. Native support can include a paid neutral setup when it changes later opportunities. Do not hardcode a named base, modifier family or archived strategy.

The exact initial two-phase family is finalized in F0/F1 from the real available native emitter and tested on a supported root fixture. That is an implementation decision with a small input/output example—not permission to introduce a universal DSL before a consumer exists.

## 3. Search and checker responsibilities

The search may evaluate an unresolved hole optimistically, use a lossy feature class, omit candidates or retain a locally bad partial edit. It must preserve which nodes are unresolved and what information its score used.

The assembler resolves every role and native specification against the immutable problem, inserts the original goal check, validates references and produces complete finite control. Any unresolved hole, missing native program, incompatible context or unexpressible required condition refuses assembly. A failure default may remain in a graph but must have zero reached mass for acceptance.

The independent evaluator supplies the actual selected-controller state product, native probabilities, resources, properness and numerical results. The accepted artifact additionally binds original root/goal/scope. A search-generated probability table or its own marked `success=true` is never passed as native evidence.

## 4. Stable identity and replay

Candidate identity includes canonical structural control, concrete native bindings and full problem identity. Exclude heuristic scores and transient queue indices from semantic identity. Preserve compiler/algorithm versions separately for caching/replay. Hash equality is an index; confirm the authoritative payload before reusing a receipt. Native context IDs cannot be serialized as portable identities.

The first implementation need not solve semantic equivalence of differently structured controllers. Exact syntactic/native-spec deduplication is sufficient; report this limit. Do not claim all candidate IDs represent distinct economic policies.

## 5. Test before broad search

Create one tiny native seed, one coordinated two-phase candidate, one unequal binding, and negative cases for unpaid reset, missing failure branch, fake goal, hidden option choice and aliasing legality. Verify from the original root. These fixtures decide whether the control representation has a real consumer before adding a larger search frontier.
