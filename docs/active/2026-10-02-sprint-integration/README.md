# Sprint integration checkpoint

This separate worktree/branch starts at `f08facbb` and leaves main and the
previous overnight integration untouched. Only reviewed passing pins are
selected. The [receipt](qualification.json) binds source pins, native tree,
compatible test evidence, isolated candidates and explicit unrun checks.

Integrated: independent finite reforge/uniform-removal safety regressions,
original-root caller-operation scope repair, matching-identity stale-certificate
refusal tests, and the read-only cluster catalogue resolver. Native source is
identical to tested review `1566719`; no combined native rebuild was needed to
reuse that evidence. The integration configuration succeeds and its 53 focused
Python tests pass. The inherited release WASM is not qualified for the repaired
source; no browser-parity claim is made.

Latency `87d3977` remains isolated after automatic approval rejected its merge
for failed 250 ms qualification and increased memory. Parent explicitly chose
isolation; do not retry. Its measured improvement and unchanged selected policies
remain valid experimental evidence, with the receipt's limitations intact.

Foulborn `448ef4a` has passing finite owner evidence, but its functional run is
pending. Parsed optional grammar metadata fixes whitespace laundering; removing
both grammar/scope fields still appears to bypass the private supplementary
opt-in when all three primitives are caller-admitted. The generic operation gate
cannot infer controller grammar from primitive membership. Require the bounded
stripped-metadata negative control and parent disposition before promotion.
This is a source-review finding, not a newly demonstrated runtime failure.

Dominance's bounded authored identity gate was reviewed; await its final expanded
qualification pin. Mismatch's proposal-versus-native difference alone is not an
execution-law defect; await its retained capture/veto/resume authority controls.
Do not merge untested WIP to meet the clock. Target source freeze 16:25 UTC,
wrap substantive work 16:45, all processes stopped by 16:55. No usage reset.

All owned commands have exited at this checkpoint. New native candidate merges
would require fresh combined validation; this receipt does not prequalify them.
