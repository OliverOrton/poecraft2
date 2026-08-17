# Five-T1 Restart-Monotone Strategy Recovery

**Status: streamed exact-evaluator closure recovery stopped at Gate 2.
Oliver must select the next transition-streaming scope.**

Parent: [Active work](../README.md)

This milestone makes Oliver's real priced-base five-natural-T1 Calculator
request retain a useful Restart-free incumbent, restores prioritized
state-local automatic admission in the high-impact scheduler, makes bounded
policy certification fail closed, and scales exact graph evaluation under the
existing memory and responsiveness limits.

- [Stopped original execution plan](plan.md)
- [Selected exact-evaluator successor plan](successor-plan.md)
- [Selected streamed evaluator closure plan](streamed-evaluator-closure-plan.md)
- [Streamed closure Gate 0 census](evidence/streamed-closure-gate0.md)
- [Streamed closure Gate 1 result and Gate 2 stop](evidence/streamed-closure-gate1-gate2-stop.md)
- [Gate 1 result and Gate 4 stop evidence](evidence/gate1-gate4-stop.md)
- [Pair-discovery follow-up audit](evidence/pair-discovery-follow-up-audit.md)
- [Successor Gate 0 result](evidence/successor-gate0.md)
- [Current handoff](../../../HANDOFF.md)

The retained transition and segmented-pair checkpoints reduce Witness B's
10-million evaluator peak to 600,881,764 bytes and safely cross the former
16.7-million pair allocation cliff. Discovery remains open at 17,998,209
pairs with only 24.8 MB of peak headroom at the temporary 18-million probe, so
the checked cap remains 10 million. Oliver selected a new audited plan that
found 9,987,873 router pairs in the checked prefix and selected exact online
deterministic routing. Gate 1 removes that frontier: the checked case now has
zero router pairs and a 362,706,844-byte peak. The sole 20-million probe still
stops with 27,588 operation pairs pending and a 650,793,188-byte peak; its
measured slope reaches one GiB before closure. A broader replayable operation-
row transition authority requires a newly selected plan. Release WASM and the
full acceptance pipeline remain deliberately unrun.
