import type { SolveOptions, SolveSummary } from "./engine-protocol";
import { formatChaosValue } from "./odds-presentation";

export interface SolveVerificationPresentation {
    completedRuns: number;
    empiricalCost: number;
    delta: number;
}

export interface SolveResultMarkupOptions {
    summary: SolveSummary;
    admittedActionIds: readonly string[];
    excludedActions: number;
    missingPriceKeys: readonly string[];
    economyLabel: string | null;
    terminationDetail: string;
    productActionScope: "goal_relevant" | "explicit_requested";
    goalProgressGatedReforges: boolean;
    considerImprintPrograms?: boolean;
    hasCompiledStrategy: boolean;
    compiledOperationTypes: readonly string[];
    busy: boolean;
    verification: SolveVerificationPresentation | null;
    telemetry?: unknown;
}

interface ActionObligationPresentation {
    available: boolean;
    enabled: boolean | null;
    closed: boolean | null;
    total: number;
    unevaluated: number | null;
    evaluating: number | null;
    unresolved: number | null;
}

interface AutomaticFamilyPresentation {
    available: boolean;
    enabled: boolean | null;
    admitted: Array<{ kind: string; candidates: number; rows: number }>;
    deferred: Array<{ kind: string; candidates: number }>;
    missingPrices: Array<{ kind: string; candidates: number }>;
}

function objectRecord(value: unknown): Record<string, unknown> | null {
    return value !== null && typeof value === "object"
        ? (value as Record<string, unknown>)
        : null;
}

function recordString(
    record: Record<string, unknown> | null,
    key: string,
): string | null {
    const value = record?.[key];
    return typeof value === "string" && value.length > 0 ? value : null;
}

function nonnegativeNumber(
    record: Record<string, unknown> | null,
    key: string,
): number | null {
    const value = record?.[key];
    return typeof value === "number" && Number.isFinite(value) && value >= 0
        ? value
        : null;
}

function readableStageStatus(value: string): string {
    return value.replace(/_/g, " ");
}

function certificationStageDetail(
    label: string,
    stage: Record<string, unknown> | null,
): string | null {
    const status = recordString(stage, "status");
    if (status === null || status === "not_attempted" || status === "none") {
        return null;
    }
    const resourceCap = recordString(stage, "resource_cap");
    const failureReason = recordString(stage, "failure_reason");
    const outcome = resourceCap !== null
        ? `${readableStageStatus(status)} (${resourceCap})`
        : readableStageStatus(status);
    return `${label} reached ${outcome}${failureReason !== null ? `: ${failureReason}` : ""}`;
}

function retainedCoreCertificationDetail(telemetry: unknown): string | null {
    const root = objectRecord(telemetry);
    const refinement = objectRecord(root?.policy_refinement);
    const core = objectRecord(refinement?.core_policy);
    if (core?.candidate_present !== true) {
        return null;
    }

    const direct = certificationStageDetail(
        "direct certification",
        objectRecord(refinement?.direct_certification),
    );
    const strict = certificationStageDetail(
        "strict refinement",
        objectRecord(refinement?.strict_lift),
    );
    const stages = [direct, strict].filter((value): value is string => value !== null);
    return stages.length > 0
        ? `Core policy found; executable certification incomplete. ${stages.join("; ")}. No executable policy was published.`
        : "Core policy found; executable certification did not produce a publishable artifact. No executable policy was published.";
}

function finiteCost(value: number | null, unavailable: string): string {
    return value !== null && Number.isFinite(value) && value < 1e12
        ? formatChaosValue(value)
        : unavailable;
}

function conservativeFixed(value: number, digits: number): string {
    const scale = 10 ** digits;
    const rounded = Math.ceil((value - Number.EPSILON) * scale) / scale;
    return rounded.toFixed(digits);
}

function conservativePercent(value: number): string {
    return conservativeFixed(value * 100, 2).replace(/\.00$/, "").replace(/(\.\d)0$/, "$1");
}

function hasExplicitlyOpenObligations(
    obligations: ActionObligationPresentation,
): boolean {
    return obligations.available &&
        obligations.enabled !== false &&
        (obligations.total > 0 || obligations.closed === false);
}

function hasExactExecutablePolicy(
    summary: SolveSummary,
    obligations: ActionObligationPresentation,
): boolean {
    return summary.policy_available &&
        summary.policy_status === "exact" &&
        summary.converged &&
        summary.termination === "exact_closed" &&
        !hasExplicitlyOpenObligations(obligations);
}

function policyQualityLabel(
    summary: SolveSummary,
    exactExecutablePolicy: boolean,
): string {
    if (!summary.policy_available) {
        return "No executable policy";
    }
    if (exactExecutablePolicy) {
        return "Exact optimal policy";
    }
    switch (summary.policy_status) {
        case "bounded_near_optimal":
            return "Bounded near-optimal policy";
        case "bounded_feasible":
            return "Bounded feasible policy";
        case "exact":
            return "Executable policy; exactness not established";
        default:
            return "Executable policy; status unavailable";
    }
}

function terminationLabel(
    summary: SolveSummary,
    exactExecutablePolicy: boolean,
): string {
    switch (summary.termination) {
        case "exact_closed":
            if (!summary.policy_available) {
                return "Closed numeric bounds without executable policy";
            }
            return exactExecutablePolicy
                ? "Exact proof closed"
                : "Coarse discovery closed";
        case "target_gap":
            return "Requested gap target met";
        case "refused_resource_cap":
            return "Resource cap reached";
        case "no_executable_policy":
            return "No executable policy could be certified";
        case "numerical_stability":
            return "Numerical stability boundary reached";
        case "requested_bounded_finish":
            return "Product discovery budget reached";
        default:
            return "Solve stopped without a classified termination";
    }
}

function requestedTargetLabel(summary: SolveSummary): string {
    const targets: string[] = [];
    if (summary.requested_absolute_optimality_gap > 0) {
        targets.push(
            `absolute gap <= ${formatChaosValue(summary.requested_absolute_optimality_gap)}`,
        );
    }
    if (summary.requested_relative_optimality_gap > 0) {
        const relative = summary.requested_relative_optimality_gap;
        targets.push(
            `relative gap <= ${conservativePercent(relative)}% (${conservativeFixed(1 + relative, 2)}x)`,
        );
    }
    return targets.length > 0 ? targets.join(" or ") : "No gap target requested";
}

function firedTargetLabel(summary: SolveSummary): string {
    if (!summary.policy_available) {
        return "No executable-policy gap target fired";
    }
    if (!summary.target_met || summary.target_fired === "none") {
        return "No gap target fired";
    }
    switch (summary.target_fired) {
        case "absolute":
            return "Absolute-gap criterion fired";
        case "relative":
            return "Relative-gap criterion fired";
        case "both":
            return "Absolute- and relative-gap criteria fired together";
        default:
            return "No gap target fired";
    }
}

function certificateText(
    summary: SolveSummary,
    exactExecutablePolicy: boolean,
): string {
    if (!summary.policy_available) {
        return "No executable policy certificate was returned.";
    }
    if (exactExecutablePolicy) {
        return "Exact optimum certified within the displayed product scope.";
    }
    if (summary.policy_status === "exact") {
        return "An executable policy was returned, but the exactness metadata is inconsistent; no exact claim is presented.";
    }
    const relative = summary.relative_optimality_gap;
    if (relative === null || !Number.isFinite(relative) || relative < 0) {
        return "An absolute bound is available. A multiplicative certificate requires a positive optimal-cost lower bound.";
    }
    if (relative === 0) {
        return "The reported numeric bounds are equal, but the engine classified this executable policy as bounded; exactness is not claimed.";
    }
    const factor = conservativeFixed(1 + relative, 2);
    const percent = conservativePercent(relative);
    return `Certified within ${factor}x of optimal. At most ${percent}% more expensive than optimal. A weak lower bound can make this certificate pessimistic.`;
}

export function certifiedFactorLabel(relativeGap: number | null): string {
    return relativeGap !== null &&
        Number.isFinite(relativeGap) &&
        relativeGap >= 0
        ? `${conservativeFixed(1 + relativeGap, 2)}x`
        : "Not certified";
}

export function shouldCompileSolvePolicy(summary: SolveSummary): boolean {
    return summary.policy_available;
}

/** Build the complete solve-option contract for normal Calculator solves. */
export function calculatorSolveOptions(
    absoluteGap: number,
    relativeGapPercent: number,
    allowEconomicRestart = false,
    considerImprintPrograms = true,
): SolveOptions {
    const options: SolveOptions = {
        goal_progress_gated_reforges: true,
        high_impact_executable_uppers: true,
        allow_economic_restart: allowEconomicRestart,
        consider_imprint_programs: considerImprintPrograms,
        // The four-T1 direct graph closes at about 163k exact carrier pairs.
        // This separate allowance lets that cheaper policy be independently
        // evaluated without widening the main solve or requiring strict lift.
        max_policy_refinement_states: 200_000,
    };
    if (Number.isFinite(absoluteGap) && absoluteGap > 0) {
        options.max_absolute_optimality_gap = absoluteGap;
    }
    if (Number.isFinite(relativeGapPercent) && relativeGapPercent > 0) {
        options.max_relative_optimality_gap = relativeGapPercent / 100;
    }
    return options;
}

/** Describe a stop without turning a bounded policy into an exactness claim. */
export function solveTerminationDetail(
    summary: SolveSummary,
    telemetry: unknown,
): string {
    const root = objectRecord(telemetry);
    const optimization = objectRecord(root?.optimization);
    const capHits = Array.isArray(optimization?.cap_hits)
        ? optimization.cap_hits.filter(
              (value): value is string => typeof value === "string",
          )
        : [];
    if (summary.termination === "refused_resource_cap") {
        const cap = capHits.length > 0 ? ` (${capHits.join(", ")})` : "";
        const retainedCoreDetail = summary.policy_available
            ? null
            : retainedCoreCertificationDetail(telemetry);
        return summary.policy_available
            ? `The solve reached a resource cap${cap}. The returned executable policy remains bounded; the cap is not an exactness claim.`
            : retainedCoreDetail ??
                  `The solve reached a resource cap${cap} before it could certify an executable policy.`;
    }
    if (summary.termination === "target_gap") {
        return "The solve stopped after a completed lower/upper round satisfied the requested certificate target.";
    }
    if (summary.termination === "exact_closed") {
        if (!summary.policy_available) {
            return "The solve reported closed numeric bounds, but no executable policy was published; no policy upper bound or exactness claim is presented.";
        }
        const exactExecutablePolicy = hasExactExecutablePolicy(
            summary,
            actionObligationPresentation(telemetry),
        );
        if (exactExecutablePolicy) {
            return "The lower and upper bounds closed within the named numerical proof tolerance.";
        }
        return summary.policy_status === "exact"
            ? "The solve reported exact closure, but executable-policy or action-obligation metadata did not establish exact authority. The policy remains non-exact in this presentation."
            : "Broad coarse discovery closed, then exact state refinement produced the returned bounded executable policy. Its displayed lower and upper bounds are authoritative.";
    }
    if (summary.termination === "no_executable_policy") {
        return retainedCoreCertificationDetail(telemetry) ??
            "The solver could not establish a proper executable fallback, so no finite policy upper bound is claimed.";
    }
    if (summary.termination === "numerical_stability") {
        return summary.policy_available
            ? "The selected policy stopped changing, but strict comparisons inside the numerical stability tolerance could not be reconciled. The independently evaluated executable policy is published as bounded; exactness is not claimed."
            : "The selected policy stopped changing before strict comparisons inside the numerical stability tolerance could be reconciled, and no executable fallback was certified.";
    }
    if (summary.termination === "requested_bounded_finish") {
        return summary.policy_available
            ? "The solve reached its product discovery budget. Open actions remain unresolved, so the independently evaluated executable policy is published as bounded rather than exact."
            : "The solve reached its product discovery budget before it could certify an executable policy.";
    }
    return "The solve stopped without a classified termination reason.";
}

function stopCauseLabel(
    summary: SolveSummary,
    telemetry: unknown,
    exactExecutablePolicy: boolean,
): string {
    const optimization = objectRecord(objectRecord(telemetry)?.optimization);
    const capHits = Array.isArray(optimization?.cap_hits)
        ? optimization.cap_hits.filter(
              (value): value is string => typeof value === "string",
          )
        : [];
    const namedCaps = capHits.length > 0 ? ` (${capHits.join(", ")})` : "";
    if (summary.stop_cause === "none" && capHits.length > 0) {
        return `Resource cap${namedCaps}`;
    }
    switch (summary.stop_cause) {
        case "exact_closed":
            if (!summary.policy_available) {
                return "Closed numeric bounds without executable policy";
            }
            return exactExecutablePolicy
                ? "Exact proof closed"
                : "Coarse discovery closed";
        case "target_gap":
            return "Requested gap target met";
        case "state_cap":
            return `State cap${namedCaps}`;
        case "transition_cap":
            return `Transition cap${namedCaps}`;
        case "memory_cap":
            return `Memory cap${namedCaps}`;
        case "sweep_cap":
            return `Sweep cap${namedCaps}`;
        case "reforge_work_cap":
            return `Reforge-work cap${namedCaps}`;
        case "state_action_row_cap":
            return `State/action-row cap${namedCaps}`;
        case "compiled_output_cap":
            return `Compiled-output cap${namedCaps}`;
        case "other_resource_cap":
            return `Other resource cap${namedCaps}`;
        case "no_executable_policy":
            return "No executable policy";
        case "numerical_stability":
            return "Numerical stability boundary";
        case "requested_bounded_finish":
            return "Product discovery budget";
        default:
            return "No stopping cause reported";
    }
}

function actionFamily(id: string): string {
    if (id.startsWith("essence:")) return "Essence";
    if (id.startsWith("fossil:")) return "Fossil";
    if (id.startsWith("harvest_")) return "Harvest";
    if (id.startsWith("eldritch_")) return "Eldritch";
    if (id.startsWith("veiled_") || id === "unveil") return "Veiled";
    if (id.startsWith("bench:") || id === "remove_crafted_modifiers") {
        return "Bench";
    }
    if (id.startsWith("bestiary:")) return "Bestiary";
    if (id === "fracture") return "Fracture";
    if (id.startsWith("influence_exalt:")) return "Influence";
    return "Currency / structural";
}

function admittedFamilyLabel(ids: readonly string[]): string {
    const counts = new Map<string, number>();
    for (const id of ids) {
        const family = actionFamily(id);
        counts.set(family, (counts.get(family) ?? 0) + 1);
    }
    return [...counts.entries()]
        .sort(([left], [right]) => left.localeCompare(right))
        .map(([family, count]) => `${family} ${count.toLocaleString()}`)
        .join(" / ") || "None";
}

function actionObligationPresentation(
    telemetry: unknown,
): ActionObligationPresentation {
    const envelope = objectRecord(
        objectRecord(telemetry)?.incremental_action_envelope,
    );
    if (envelope === null) {
        return {
            available: false,
            enabled: null,
            closed: null,
            total: 0,
            unevaluated: null,
            evaluating: null,
            unresolved: null,
        };
    }
    const actions = objectRecord(envelope.actions);
    const unevaluated = nonnegativeNumber(actions, "unevaluated");
    const evaluating = nonnegativeNumber(actions, "evaluating");
    const unresolved = nonnegativeNumber(actions, "unresolved");
    const detailed = [unevaluated, evaluating, unresolved].every(
        (value) => value !== null,
    );
    const declaredRemaining = nonnegativeNumber(
        envelope,
        "remaining_action_envelope",
    );
    const detailedTotal = detailed
        ? (unevaluated ?? 0) + (evaluating ?? 0) + (unresolved ?? 0)
        : 0;
    const total = Math.max(detailedTotal, declaredRemaining ?? 0);
    return {
        available: true,
        enabled:
            typeof envelope.enabled === "boolean" ? envelope.enabled : null,
        closed: typeof envelope.closed === "boolean" ? envelope.closed : null,
        total,
        unevaluated,
        evaluating,
        unresolved,
    };
}

function actionObligationText(
    obligations: ActionObligationPresentation,
): string {
    if (!obligations.available) {
        return "Action-obligation telemetry unavailable.";
    }
    if (obligations.enabled === false) {
        return "Incremental action-obligation tracking was not active for this solve.";
    }
    if (obligations.total > 0) {
        const detail = obligations.unevaluated !== null &&
            obligations.evaluating !== null &&
            obligations.unresolved !== null
            ? ` (${obligations.unevaluated.toLocaleString()} unevaluated, ${obligations.evaluating.toLocaleString()} evaluating, ${obligations.unresolved.toLocaleString()} unresolved)`
            : "";
        return `${obligations.total.toLocaleString()} action obligations remained open when the solve stopped${detail}.`;
    }
    if (obligations.closed === false) {
        return "The incremental action envelope remained open; no open-obligation count was reported.";
    }
    return obligations.closed === true
        ? "Action obligations closed; no unevaluated, evaluating, or unresolved alternatives remain."
        : "No unresolved action obligations were reported.";
}

function automaticFamilyPresentation(
    telemetry: unknown,
): AutomaticFamilyPresentation {
    const actionControl = objectRecord(objectRecord(telemetry)?.action_control);
    const automatic = objectRecord(actionControl?.automatic_candidates);
    const byKind = objectRecord(automatic?.by_kind);
    if (automatic === null || byKind === null) {
        return {
            available: false,
            enabled:
                typeof automatic?.enabled === "boolean"
                    ? automatic.enabled
                    : null,
            admitted: [],
            deferred: [],
            missingPrices: [],
        };
    }
    const admitted: AutomaticFamilyPresentation["admitted"] = [];
    const deferred: AutomaticFamilyPresentation["deferred"] = [];
    const missingPrices: AutomaticFamilyPresentation["missingPrices"] = [];
    for (const [kind, raw] of Object.entries(byKind)) {
        const values = objectRecord(raw);
        const candidates = nonnegativeNumber(values, "eligible") ?? 0;
        const rows = nonnegativeNumber(values, "rows") ?? 0;
        const deferredCandidates =
            nonnegativeNumber(values, "deferred") ?? 0;
        const missing = nonnegativeNumber(values, "missing_price") ?? 0;
        if (candidates > 0 || rows > 0) {
            admitted.push({ kind, candidates, rows });
        }
        if (deferredCandidates > 0) {
            deferred.push({ kind, candidates: deferredCandidates });
        }
        if (missing > 0) missingPrices.push({ kind, candidates: missing });
    }
    admitted.sort((left, right) => left.kind.localeCompare(right.kind));
    deferred.sort((left, right) => left.kind.localeCompare(right.kind));
    missingPrices.sort((left, right) => left.kind.localeCompare(right.kind));
    return {
        available: true,
        enabled:
            typeof automatic.enabled === "boolean" ? automatic.enabled : null,
        admitted,
        deferred,
        missingPrices,
    };
}

function readableAutomaticKind(kind: string): string {
    return kind
        .split("_")
        .filter(Boolean)
        .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
        .join(" ") || "Unknown";
}

function automaticFamilyText(
    automatic: AutomaticFamilyPresentation,
): string {
    if (!automatic.available) {
        return automatic.enabled === false
            ? "Automatic candidate planning was disabled."
            : "Admitted automatic-family telemetry unavailable.";
    }
    if (automatic.enabled === false) {
        return "Automatic candidate planning was disabled.";
    }
    if (automatic.admitted.length === 0) {
        return "Admitted automatic families: none reported.";
    }
    return `Admitted automatic families: ${automatic.admitted
        .map(({ kind, candidates, rows }) =>
            `${readableAutomaticKind(kind)} ${candidates.toLocaleString()} ${candidates === 1 ? "eligible candidate" : "eligible candidates"}, ${rows.toLocaleString()} ${rows === 1 ? "completed row" : "completed rows"}`,
        )
        .join(" / ")}.`;
}

function automaticMissingPriceText(
    automatic: AutomaticFamilyPresentation,
): string | null {
    if (automatic.missingPrices.length === 0) return null;
    return `Automatic candidates rejected for missing prices: ${automatic.missingPrices
        .map(({ kind, candidates }) =>
            `${readableAutomaticKind(kind)} ${candidates.toLocaleString()}`,
        )
        .join(" / ")}.`;
}

function automaticDeferredText(
    automatic: AutomaticFamilyPresentation,
): string | null {
    if (automatic.deferred.length === 0) return null;
    return `Automatic candidates left deferred: ${automatic.deferred
        .map(({ kind, candidates }) =>
            `${readableAutomaticKind(kind)} ${candidates.toLocaleString()}`,
        )
        .join(" / ")}.`;
}

function compiledOperationLabel(type: string): string {
    return type
        .split("_")
        .filter(Boolean)
        .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
        .join(" ") || "Unknown";
}

function policyAuthorityText(
    summary: SolveSummary,
    exactExecutablePolicy: boolean,
    compiledOperationTypes: readonly string[],
): string {
    if (!summary.policy_available) {
        return "No executable policy was published. Numeric search bounds cannot authorize compilation, a policy upper bound, a gap certificate, or an exactness claim.";
    }
    const operations = [...new Set(compiledOperationTypes)];
    const chaosOnly = operations.length === 1 && operations[0] === "chaos";
    if (exactExecutablePolicy) {
        return chaosOnly
            ? "Chaos-only policy authority: exact within the displayed scope because an executable exact policy was published and every relevant admitted action obligation closed."
            : "Executable exact-policy authority: the published policy and every relevant admitted action obligation closed within the displayed scope.";
    }
    return chaosOnly
        ? "Chaos-only policy authority: executable incumbent only. The reported bounded status or stopping condition leaves no basis for an optimality claim."
        : "The published executable policy is a bounded incumbent. Its certified upper bound is authoritative, but it is not proof of exact optimality.";
}

/** Stable DOM contract for exact and bounded solver results. */
export function solveResultMarkup(options: SolveResultMarkupOptions): string {
    const {
        summary,
        admittedActionIds,
        excludedActions,
        missingPriceKeys,
        economyLabel,
        terminationDetail,
        productActionScope,
        goalProgressGatedReforges,
        considerImprintPrograms = true,
        hasCompiledStrategy,
        compiledOperationTypes,
        busy,
        verification,
        telemetry,
    } = options;
    const obligations = actionObligationPresentation(telemetry);
    const automatic = automaticFamilyPresentation(telemetry);
    const exactExecutablePolicy = hasExactExecutablePolicy(
        summary,
        obligations,
    );
    const equalNumericBounds = summary.lower_bound !== null &&
        summary.upper_bound !== null &&
        Number.isFinite(summary.lower_bound) &&
        Number.isFinite(summary.upper_bound) &&
        summary.lower_bound === summary.upper_bound;
    const zeroGapWithoutExactAuthority = !exactExecutablePolicy &&
        (equalNumericBounds ||
            summary.absolute_optimality_gap === 0 ||
            summary.relative_optimality_gap === 0);
    const canPresentGapCertificate = summary.policy_available &&
        (exactExecutablePolicy ||
            (summary.policy_status !== "exact" &&
                !zeroGapWithoutExactAuthority));
    const evaluatedPolicyCost = summary.policy_available
        ? finiteCost(summary.evaluated_policy_cost, "Policy cost unavailable")
        : "No executable policy";
    const lowerBound = finiteCost(summary.lower_bound, "Not certified");
    const upperBound = summary.policy_available
        ? finiteCost(summary.upper_bound, "Not certified")
        : "Unavailable: no executable policy";
    const absoluteGap = canPresentGapCertificate
        ? finiteCost(summary.absolute_optimality_gap, "Not certified")
        : summary.policy_available
          ? "Unavailable: exactness not established"
          : "Unavailable: no executable policy";
    const multiplicativeFactor = canPresentGapCertificate
        ? certifiedFactorLabel(summary.relative_optimality_gap)
        : summary.policy_available
          ? "Unavailable: exactness not established"
          : "Unavailable: no executable policy";
    const statusClass = exactExecutablePolicy ? "is-success" : "is-warning";
    const admitted = admittedActionIds
        .map((id) => `<li><code>${escapeHtml(id)}</code></li>`)
        .join("");
    const admittedFamilies = admittedFamilyLabel(admittedActionIds);
    const automaticMissingPrices = automaticMissingPriceText(automatic);
    const automaticDeferred = automaticDeferredText(automatic);
    const uniqueMissingPriceKeys = [...new Set(missingPriceKeys)]
        .sort((left, right) => left.localeCompare(right));
    const missingPriceIdentity = uniqueMissingPriceKeys
        .map((key) => `<li><code>${escapeHtml(key)}</code></li>`)
        .join("");
    const compiledOperations = [...new Set(compiledOperationTypes)]
        .sort((left, right) => left.localeCompare(right))
        .map(compiledOperationLabel)
        .join(" / ");
    const scopeLabel = productActionScope === "goal_relevant"
        ? "Product goal-relevant, supported priced action scope"
        : "Explicit requested, supported priced action scope";
    const gatingLabel = goalProgressGatedReforges
        ? "Goal-progress-gated reforges enabled: zero-progress outcomes retry through destructive reforges only."
        : "Goal-progress-gated reforges disabled.";
    const imprintScopeLabel = considerImprintPrograms
        ? "Automatic Imprint checkpoint/retry programs are considered."
        : "Automatic Imprint checkpoint/retry programs are excluded by caller scope; bounds and exactness apply only without that family.";
    const obligationText = actionObligationText(obligations);
    const obligationWarning = hasExplicitlyOpenObligations(obligations);
    const canUseCompiledPolicy = summary.policy_available && hasCompiledStrategy;

    return `<section class="pc-calc-solve-result" data-policy-status="${summary.policy_status}" data-policy-available="${summary.policy_available}" data-exact-authority="${exactExecutablePolicy}" data-product-scope="${productActionScope}" data-goal-progress-gated-reforges="${goalProgressGatedReforges}" data-consider-imprint-programs="${considerImprintPrograms}" data-termination="${summary.termination}" data-stop-cause="${summary.stop_cause}">
        <div class="pc-calc-solve-headline">
            <span>Returned policy expected cost</span>
            <strong data-solve-result="evaluated-policy-cost">${evaluatedPolicyCost}</strong>
        </div>
        <div class="pc-calc-solve-state ${statusClass}">
            <strong data-solve-result="policy-quality">${policyQualityLabel(summary, exactExecutablePolicy)}</strong>
            &middot; ${summary.expanded_states.toLocaleString()} states
            &middot; ${summary.sweeps.toLocaleString()} sweeps
            &middot; residual ${summary.residual.toExponential(2)}
        </div>
        <p class="pc-calc-solve-certificate" data-solve-result="certificate">${escapeHtml(certificateText(summary, exactExecutablePolicy))}</p>
        <p class="pc-calc-solve-policy-authority" data-solve-result="policy-authority">${escapeHtml(policyAuthorityText(summary, exactExecutablePolicy, compiledOperationTypes))}</p>
        <div class="pc-calc-solve-scope" data-solve-result="scope">
            <strong>${scopeLabel}</strong>
            <span>${gatingLabel}</span>
            <span class="${considerImprintPrograms ? "" : "is-warning"}">${imprintScopeLabel}</span>
            <span>Admitted priced primitive families: ${escapeHtml(admittedFamilies)}.</span>
            <span>${escapeHtml(automaticFamilyText(automatic))}</span>
            ${compiledOperations ? `<span>Compiled policy operations: ${escapeHtml(compiledOperations)}.</span>` : ""}
            <span>Missing prices excluded ${excludedActions.toLocaleString()} primitive actions before Solve and ${summary.skipped_missing_price_actions.toLocaleString()} actions inside native planning.</span>
            ${automaticMissingPrices ? `<span class="is-warning">${escapeHtml(automaticMissingPrices)}</span>` : ""}
            ${automaticDeferred ? `<span class="is-warning">${escapeHtml(automaticDeferred)}</span>` : ""}
            <span class="${obligationWarning ? "is-warning" : ""}" data-solve-result="action-obligations">${escapeHtml(obligationText)}</span>
        </div>
        <dl class="pc-calc-solve-bounds">
            <div><dt>Optimal-cost lower bound</dt><dd data-solve-result="lower-bound">${lowerBound}</dd></div>
            <div><dt>Certified policy upper bound</dt><dd data-solve-result="upper-bound">${upperBound}</dd></div>
            <div><dt>Absolute optimality gap</dt><dd data-solve-result="absolute-gap">${absoluteGap}</dd></div>
            <div><dt>Certified multiplicative factor</dt><dd data-solve-result="multiplicative-factor">${multiplicativeFactor}</dd></div>
            <div><dt>Termination</dt><dd data-solve-result="termination-reason">${escapeHtml(terminationLabel(summary, exactExecutablePolicy))}</dd></div>
            <div><dt>Stopping resource / cause</dt><dd data-solve-result="stop-cause">${escapeHtml(stopCauseLabel(summary, telemetry, exactExecutablePolicy))}</dd></div>
            <div><dt>Requested target</dt><dd data-solve-result="requested-target">${escapeHtml(requestedTargetLabel(summary))}</dd></div>
            <div><dt>Firing criterion</dt><dd data-solve-result="firing-criterion">${escapeHtml(firedTargetLabel(summary))}</dd></div>
            <div><dt>Economy</dt><dd data-solve-result="economy">${economyLabel ? escapeHtml(economyLabel) : "No economy selected"}</dd></div>
        </dl>
        <p class="pc-calc-solve-termination-detail">${escapeHtml(terminationDetail)}</p>
        <strong class="pc-calc-solve-skipped">${excludedActions.toLocaleString()} unpriced actions excluded before Solve</strong>
        <span class="pc-calc-solve-native-skipped">${summary.skipped_missing_price_actions.toLocaleString()} missing-price and ${summary.skipped_unsupported_actions.toLocaleString()} unsupported actions skipped by the native solver; ${summary.supported_priced_actions.toLocaleString()} supported priced actions remained from ${summary.candidate_actions.toLocaleString()} candidates</span>
        <details class="pc-calc-solve-admitted-actions pc-calc-solve-missing-prices">
            <summary>Pre-solve primitive missing-price identity &middot; ${uniqueMissingPriceKeys.length.toLocaleString()}</summary>
            <ul>${missingPriceIdentity || "<li>None</li>"}</ul>
        </details>
        <details class="pc-calc-solve-admitted-actions">
            <summary>Admitted priced action identity &middot; ${admittedActionIds.length.toLocaleString()}</summary>
            <ul>${admitted || "<li>None</li>"}</ul>
        </details>
        ${
            canUseCompiledPolicy
                ? `<div class="pc-calc-solve-actions">
                    <button data-solve-cmd="open" ${busy ? "disabled" : ""}>Open in Strategy Board</button>
                    <button data-solve-cmd="verify" ${busy ? "disabled" : ""}>Verify 10,000 runs</button>
                </div>`
                : ""
        }
        ${
            summary.policy_available && verification
                ? `<div class="pc-calc-solve-verification">
                    <span>Evaluated policy <strong>${evaluatedPolicyCost}</strong></span>
                    <span>10,000-run empirical mean <strong>${formatChaosValue(verification.empiricalCost)}</strong></span>
                    <span>Delta <strong>${verification.delta >= 0 ? "+" : "&minus;"}${formatChaosValue(Math.abs(verification.delta))}</strong></span>
                    <small>Simulation is corroboration; exact policy evaluation is authoritative.</small>
                </div>`
                : ""
        }
    </section>`;
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}
