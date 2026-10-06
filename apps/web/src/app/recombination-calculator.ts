import type {EngineClient} from "./engine-client";
import type {CalcResult, CalculatorGoalSet} from "./engine-protocol";
import type {ItemSnapshot} from "./workspace/persistence";

/** Read-only analysis owns temporary resources, never the authored inputs. */
export async function calculateAuthoredRecombination(client: EngineClient, data: number,
    inputs: readonly [ItemSnapshot, ItemSnapshot], goals: CalculatorGoalSet): Promise<CalcResult> {
    const sessions: number[] = [], items: number[] = [];
    let pair = 0;
    try {
        for (const input of inputs) {
            const session = await client.createSession(data, input.base, input.itemLevel, input.cluster);
            sessions.push(session);
            items.push(await client.importItem(input.state, session));
        }
        pair = await client.openRecombinationPair({resources: [
            {identity: "calculator-input-a", role: "input_a", session: sessions[0], item: items[0]},
            {identity: "calculator-input-b", role: "input_b", session: sessions[1], item: items[1]},
        ]});
        return await client.recombinationCalculate(pair, goals);
    } finally {
        // Attempt every release even when another release rejects.
        try { if (pair) await client.closeRecombinationPair(pair); }
        finally {
            try { await Promise.all(items.map(item => client.closeItem(item))); }
            finally { await Promise.all(sessions.map(session => client.closeSession(session))); }
        }
    }
}

export interface RecombinationOdds {
    success: number;
    carriers: Array<{carrier: 0 | 1; base: string; itemLevel: number; probability: number; goalMass: number}>;
}

/** Display-only terminal filtering. Native rows own both probability and the
 * complete goal predicate. A base requirement never selects a carrier. */
export function recombinationOdds(result: CalcResult, requiredBase?: string): RecombinationOdds {
    if (!result.supported || !result.legal) throw new Error("This pair or goal is unsupported by the native model.");
    if (result.pair_version !== 1 || !result.model_projection_exact || !result.carriers || result.carriers.length !== 2)
        throw new Error("The engine did not return a complete random pair projection.");
    const finiteProbability = (value: number) => Number.isFinite(value) && value >= 0 && value <= 1;
    if (new Set(result.carriers.map(carrier => carrier.carrier)).size !== 2 ||
        result.carriers.some(carrier => !finiteProbability(carrier.probability) || !carrier.base_metadata_path))
        throw new Error("Incomplete native carrier observations; odds cannot be displayed.");
    const mass = result.outcomes.reduce((sum, row) => sum + row.probability, 0);
    if (!finiteProbability(result.success_probability) || Math.abs(mass - 1) > 1e-9 ||
        result.outcomes.some(row => !finiteProbability(row.probability) || typeof row.is_goal !== "boolean" ||
            !result.carriers!.some(carrier => carrier.carrier === row.carrier && carrier.base_metadata_path === row.base_metadata_path)))
        throw new Error("Incomplete native terminal observations; odds cannot be displayed.");
    const carriers = result.carriers.map(carrier => ({carrier: carrier.carrier,
        base: carrier.base_metadata_path, itemLevel: carrier.item_level, probability: carrier.probability,
        goalMass: result.outcomes.reduce((sum, row) => sum +
            (row.carrier === carrier.carrier && row.is_goal ? row.probability : 0), 0)}));
    if (Math.abs(carriers.reduce((sum, carrier) => sum + carrier.goalMass, 0) - result.success_probability) > 1e-9 ||
        carriers.some(carrier => Math.abs(result.outcomes.reduce((sum, row) => sum +
            (row.carrier === carrier.carrier ? row.probability : 0), 0) - carrier.probability) > 1e-9))
        throw new Error("Native carrier and goal totals disagree; odds cannot be displayed.");
    return {success: requiredBase === undefined ? result.success_probability :
        carriers.reduce((sum, carrier) => sum + (carrier.base === requiredBase ? carrier.goalMass : 0), 0), carriers};
}
