# Supplied advisory input — not implementation authority

Oliver supplied the following note from another session and asked that it be
considered, not treated as gospel. The current review independently checks its
scope and source claims; it does not treat the conclusion as measured benefit.

> The G0 timing result should be treated as a negative result only for the proposed low-level reforge template granularity, not for the broader heterogeneous goal-role symmetry hypothesis. The current solver already filters some actions before row construction using legality, incumbent bounds, and scheduling, but once a broad reforge action survives, it materializes the complete exact successor distribution and then enqueues essentially every non-self successor for later expansion. That means the much larger opportunity may sit after reforge generation: states such as “A+B held, need C”, “A+C held, need B”, and “B+C held, need A” remain distinct because their weights, tiers, blockers, probabilities, and optimal actions can differ, yet they may share the same role-level decision/computation structure under different bindings. A useful next diagnostic would therefore measure recurrence of these heterogeneous goal-role structural signatures across discovered states and expensive downstream action/Bellman work, without merging values or probability mass; if recurrence is substantial, the research target becomes exact reuse of computation structure across bindings rather than caching the ~1% bucket/exclusion setup that G0 falsified.

Disposition: scope warning incorporated; unconditional queue implication corrected;
downstream substantial reuse remains a hypothesis. Recurrence alone is not an
advance gate. The programme requires served-work attribution, pre-reuse validity
conditions, and an actual expensive operation avoided under the existing caches.
