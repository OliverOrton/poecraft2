# Condition-Efficient Compilation Gate 1

**Status: complete; typed canonical route conditions are retained.**

`ConditionExpr` is now the compiler-side authority for composite route
conditions. Primitive conditions remain opaque v1 leaves owned by the native
strategy parser and simulator. Immutable typed constructors provide
deterministic `all`, `any`, `not`, and `at_least` composition.

The initial canonical rules are deliberately limited to exact boolean
identities: flatten equal composites, remove true from conjunctions and false
from disjunctions, collapse empty and singleton composites, and deduplicate
structurally identical children in first-seen order. No mechanics-aware
implication, distributive expansion, or public schema change was introduced.

On priced-base Witness B, typed route composition alone preserves 92 nodes and
338 edges while reducing condition bytes from 116,972 to 106,766 and strategy
JSON from 150,813 to 140,607. Independent exact evaluation remains
`16226566.773294946`, success one, and off-policy mass zero. The focused
compiler suite passes 823 checks, including typed identity, flattening,
deduplication, negation, and threshold construction controls plus the existing
native execution tests.

The remaining 30 same-target groups are unchanged, as intended. Gate 2 now
coalesces only siblings whose common split provenance proves mutual exclusion.
