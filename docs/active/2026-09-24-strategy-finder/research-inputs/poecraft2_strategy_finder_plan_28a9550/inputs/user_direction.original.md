Here’s the updated handoff I’d send to the Pro/Codex chat. I’ve deliberately framed the new ideas as **research directions to evaluate**, not as implementation decisions.

---

> We may be at a useful architectural inflection point for `poecraft2`. Before writing the next implementation plan, please **check current `main`, the newest HANDOFF/research records, and the actual existing solver/evaluator/data-generation infrastructure**, then critically evaluate the direction below. Do not assume every idea here belongs in the next system; decide which pieces are actually valuable based on source inspection, experiments, and relevant research.
>
> The strategic idea is to consider separating two jobs that the current solver has largely tried to do together:
>
> 1. **Find extremely good executable strategies.**
> 2. **Prove optimality / exact closure.**
>
> The current exact architecture has given us a very strong trustworthy backend, but we have repeatedly spent substantial effort on exact/search/proof machinery without always finding materially cheaper strategies. It may now be worthwhile to let a new policy-discovery lane be substantially more aggressive and approximate, while retaining exact native evaluation as the acceptance boundary.
>
> A possible high-level architecture is:
>
> ```
> best-strategy finder
>     ↓
> native policy construction
>     ↓
> exact independent evaluation
>     ↓
> verified executable strategy
> ```
>
> Initially, **stop there**.
>
> Do not immediately feed learned estimates into proof authority, exact pruning, ProofStore, lower certification, etc. Once this lane is demonstrably useful and trustworthy as a producer of strong verified policies, a later phase could feed only its **verified incumbent/upper bound** into the existing exact optimizer/prover.
>
> However, do **not** interpret this separation as “the strategy finder should not use bounds, pruning, or exact information.” Quite the opposite. Please investigate what combination produces the strongest strategies.
>
> The best-strategy finder could potentially use:
>
> - certified lower bounds where cheap/useful;
> - optimistic heuristic bounds;
> - learned values;
> - learned candidate-improvement predictions;
> - aggressive heuristic pruning;
> - beam pruning;
> - native legality filtering;
> - exact transition distributions for selected expansions;
> - macro actions/options;
> - incumbent continuation values;
> - recurrence/progress-retention features;
> - approximate role/symmetry representations.
>
> The important distinction is **authority**:
>
> - a certified lower can support mathematically safe pruning within its valid scope;
> - a heuristic/learned estimate can aggressively prune the approximate strategy search;
> - neither a learned value nor an approximate abstraction should silently become proof authority;
> - every policy reported to the user still passes the existing exact evaluator.
>
> So one plausible target is something like:
>
> ```
> learned / bound-guided anytime policy search
>              ↓
>       native exact expansion
>              ↓
>      complete policy candidates
>              ↓
>       exact native evaluator
>              ↓
>       best verified policy
> ```
>
> Please decide whether beam/AND-OR search is actually the best formulation, rather than taking that as predetermined. Other frameworks worth comparing include MCTS/PUCT, evolutionary/program search, offline-RL/Q guidance, heuristic best-first policy search, expert iteration, or a portfolio of several proposal generators. The choice should follow the structure of this SSP, its macro actions, branching distributions, horizons, and existing infrastructure.
>
> A particularly important idea for training/data generation is that **we already own an exact generative model**.
>
> The Calculator covers the primitive crafting actions, and the solver already contains higher-level macro/options. That may let us generate a very large supervised dataset over valid item states and actions/macros:
>
> ```
> item / goal / context
> candidate primitive or macro
>     →
> legality
> native cost
> expected primitive count
> exact outcome distribution
> progress/loss statistics
> resulting states
> ```
>
> Then augment this with more strategic labels from actual solver/evaluator work:
>
> ```
> state / entry + candidate patch
>     →
> complete?
> proper?
> exact original-root cost
> improvement over incumbent
> construction/check cost
> refusal/cap/failure reason
> ```
>
> Please investigate whether this is as cheap and scalable as it sounds in the actual code. In particular, distinguish:
>
> - arbitrary valid concrete items;
> - current solver abstract states;
> - role-normalized ML features;
> - states actually encountered by policies/search;
> - states where hidden information omitted from the ML representation changes native behaviour.
>
> Do **not** assume the learning representation must itself be an exact abstraction. It can deliberately discard information if the native backend remains authoritative. But retain enough source identity during dataset generation to detect cases where apparently identical ML inputs actually have different laws.
>
> The earlier heterogeneous symmetry idea may be especially valuable here. Instead of requiring exact equivalence between:
>
> ```
> Fire + Lightning, need Cold
> Cold + Lightning, need Fire
> Armour + Life, need Evasion
> ```
>
> the learned/search representation could express relative roles:
>
> - held/satisfied goal roles;
> - missing goal roles;
> - prefix/suffix side;
> - satisfying and below-tier weight structure;
> - tier counts;
> - blocker/capacity state;
> - fractured/crafted status;
> - suffix progress;
> - applicable macro/action families;
> - prices and relevant continuation context.
>
> Native probabilities and policy values remain binding-specific. This could allow transfer between structurally similar but numerically different crafting problems without making an unjustified exact merge.
>
> Please also investigate whether **macro actions are a major advantage for learning/search**. They may dramatically reduce effective planning horizon versus asking a model to rediscover setup → repeat → cleanup/recovery structures from primitives. But do not assume every current macro is a useful ML action or that the existing macro vocabulary is sufficient.
>
> Another idea to assess is **expert iteration/self-improvement**:
>
> ```
> model
>   ↓
> guided strategy search
>   ↓
> exact evaluator
>   ↓
> stronger verified strategies / candidate labels
>   ↓
> training data
>   ↓
> improved model
> ```
>
> The initial implementation may not need neural networks at all. It might be preferable to first build the approximate-search/data interface with handcrafted scoring, then compare:
>
> - current heuristics;
> - simple statistical/GBDT ranker;
> - small neural model;
>
> under the **same native candidate-check budget**. Let results determine whether GPU/model complexity is justified.
>
> GPU and parallelism should also be evaluated rather than assumed:
>
> - GPU may make sense for model training and large batched inference.
> - A whole CUDA rewrite of the exact native solver is probably a different and much larger project.
> - Parallel independent proposal workers may be much easier than fine-grained multithreading inside the current coroutine-heavy shared solver.
> - Exact evaluation of independent candidate graphs could potentially use a bounded evaluator pool if aggregate memory allows.
> - A portfolio could potentially run different search biases concurrently and let exact evaluation arbitrate.
>
> Please inspect the actual memory, ownership, coroutine and evaluator boundaries before deciding what concurrency is sensible.
>
> ### Frontend / product experimentation
>
> It would also be very useful if Calculator eventually exposed a **solver-mode selector**, so these approaches can be tested directly without maintaining separate frontend paths.
>
> Prefer one shared canonical request construction:
>
> ```
> base
> start item
> goal
> economy
> actions/macros
> limits
>     ↓
> canonical solve request
>     ↓
> solver-mode orchestration branch
> ```
>
> Possible experimental modes might include:
>
> - current exact solver;
> - best-strategy finder;
> - proposal portfolio;
> - later hybrid: strategy finder → verified incumbent → exact proof.
>
> These names/modes are illustrative. Decide what actually makes sense.
>
> Reuse the Calculator request path, native mechanics, registry, macros, compiler, evaluator, telemetry and corpus machinery as much as practical. At the same time, **keep the new search implementation sufficiently separate initially** that we can understand its behaviour and compare it cleanly. Do not prematurely weave learned ranking throughout the current exact scheduler simply because the two systems share backend components.
>
> ### What I want the next plan to determine
>
> Please do a fresh source/research audit and answer:
>
> 1. How much of the existing project can directly serve as the backend for a best-strategy-finder lane?
> 2. What should the clean interface be between canonical request, approximate search, native mechanics/macros, policy compiler, and exact evaluator?
> 3. Which search framework is most likely to find the **best strategies**, not merely be easiest to implement?
> 4. What training data can we generate cheaply and at what scale?
> 5. What should the first state/action/goal representation look like?
> 6. Which kinds of lower bounds, pruning, heuristic estimates, or learned estimates are likely useful inside the new finder?
> 7. Which components should explicitly **not** be included initially?
> 8. Is handcrafted beam/search a better first milestone than training a model immediately?
> 9. When, if ever, should GPU inference/training enter?
> 10. What parallelism is safe and useful?
> 11. How should Calculator expose and compare the different solver modes?
> 12. What cases and metrics best demonstrate that the new lane genuinely improves strategy quality?
> 13. If it succeeds, what is the safest later interface for giving its verified policy to the exact optimizer/prover?
>
> The main experimental metric should probably be something close to:
>
> **best independently verified original-root strategy found under a fixed wall-time/work/resource budget.**
>
> Compare at useful horizons such as tens of seconds, a few minutes, and longer native runs. Include structurally different targets rather than optimizing one benchmark.
>
> Please do not measure success primarily through model loss, candidate counts, approximate predicted value, or number of explored states. Those are diagnostics. The thing we ultimately care about is whether the system finds materially better **verified executable strategies**.
>
> Finally, do not treat this message as a request to implement ML + beam search + GPU + multithreading + UI modes all at once. These are ideas to investigate. **Use current-main source inspection, existing experiments, primary research and small discriminating tests to decide which pieces actually belong in the next programme.**
>
> The strategic question I want the next plan to take seriously is:
>
> > **Have we built enough exact infrastructure that the most valuable next move is to let policy discovery become aggressively heuristic/learned, while using the existing exact backend as its oracle and verifier—and only later reconnecting the resulting verified incumbent to optimality proof?**
>
> If yes, design the smallest architecture and experiment that can falsify or validate that direction without entangling it prematurely with the current proof solver.