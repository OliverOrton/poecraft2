import type {StrategyTraceEntry} from "./engine-protocol";
import type {ItemSnapshot} from "./workspace/persistence";

export type TraceResource = NonNullable<StrategyTraceEntry["resources"]>[number];

/** Native resource envelope only. No authored base/template or entry.item fallback. */
export function traceResourceSnapshot(resource: TraceResource): ItemSnapshot | null {
    if (typeof resource.identity !== "string" || !resource.identity ||
        typeof resource.base_key !== "string" || !resource.base_key ||
        !Number.isInteger(resource.item_level) || !resource.item ||
        typeof resource.item !== "object" || Array.isArray(resource.item)) return null;
    return {resourceIdentity: resource.identity, base: resource.base_key,
        itemLevel: resource.item_level!, state: resource.item};
}
