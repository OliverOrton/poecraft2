# Setup owner disposition

The A5 matched Conquest worker attributes its 12,336.19 ms synchronous begin
to 8,088.96 ms in `prepare_goal_cover_cost` and 4,244.31 ms in retention
preparation. The adjacent default arm begins in 12,407.07 ms. These are measured
source spans within begin, separate from the stepped-call maximum and export.
Raw reports: `out/verified-delivery/A5-{manual,default}-conquest.json`.

Initialization is **blocked and unqualified**, with zero cooperative-conversion
variants attempted. The dominant goal-cover owner in `solver_solve_bounds.cpp`
sets its ready flag before synchronous table/contract construction. Existing
bound consumers and snapshot paths can call preparation themselves. Inserting
yields there without staged storage, atomic publication and removing getter
reentry could expose unfinished lower evidence. Its nested native probability
construction is not made cooperative by moving the constructor call to a step.

Retention preparation is a second synchronous owner, about 4.24 seconds here,
using full native probability and quotient checks in `solver_phase_probability.cpp`.
Even eliminating all dominant-owner time would leave begin over the 250 ms gate
and setup cancellation over one second. Converting both owners, their validation
and pre-ledger accounting would exceed the selected one-owner repair. No bounded
change with a credible path to these gates was identified. Preparation order,
lower authority, caps and browser activation remain unchanged. This is a scoped
engineering stop, not evidence that cooperative preparation is impossible.

The A5 473.16 ms stepped outlier is separately attributed to publication:
input `policy_assembly`, cursor 308; output `compilation`, cursor 311; quantum 1.
The lifecycle log shows a new candidate capture/compile after Finish acknowledgement.
A6 addresses that specific finalization defect by taking completed evidence and
interrupting optional unfinished checking while preserving spent work. It does
not claim to fix every long ordinary step or either setup owner. Final measured
step/cancellation dispositions belong to the qualification record.

The canonical staged-work and memory premises are maintained at
[search and resumption](../../solver/mathematics/search-and-resumption.md#cooperative-preparation).
They describe the requirements for a future conversion, not an implemented setup
task or a latency theorem. No scheduler rewrite or blanket batch reduction was made.
