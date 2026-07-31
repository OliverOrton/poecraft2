# Active Work

**Status: one implementation plan is active.**

Parent: [Documentation map](../README.md)

Current boundary:
[Policy-Guided Exact State Refinement](policy-guided-exact-refinement.md).
It preserves broad coarse discovery while lazily restoring exact identity
only where a selected action or downstream policy observes it, then performs
witness-local Bellman re-optimization when exact subclasses need different
choices.

The completed
[Cross-Base And Compiled-Strategy Reliability Pass](../archive/2026-07-30-cross-base-strategy-reliability/README.md)
is the source boundary. It proved every published policy exact, but refused
most solved coarse policies when selected actions observed discarded
exclusion identity. This milestone converts those ordinary solutions into
executable exact strategy regions rather than manufacturing a fallback.

The qualified
[Fracture-Local Coarse-Parent Prototype](../archive/2026-07-29-fracture-local-coarse-parent/README.md)
remains a frozen non-regression boundary: six parent junk classes, 217 root
Chaos successors, 927 states, and its qualified transition, policy, and
compiled-strategy hashes must remain unchanged.

The deferred executable-anchor library remains unselected and outside this
boundary.

Older completed milestones are indexed in the
[documentation archive](../archive/README.md). Unselected possibilities remain
in [Future work](../future/README.md); neither archive nor future documents
are current execution authority.
