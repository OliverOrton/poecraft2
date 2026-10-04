import type { ReactNode } from "react";
import { genericInfluenceDisplayName } from "../influence-presentation";
import { GameIcon } from "./pc-game-icon";

/** Presentation context is explicit; badges never add an item/goal constraint. */
export function InfluenceBadge({name, context = "actual"}: {
    name: string; context?: "actual" | "required" | "exact" | "choice";
}) {
    const label = genericInfluenceDisplayName(name);
    const prefix = context === "required" ? "Required: " : context === "exact" ? "Exactly: " : "";
    const description = context === "actual" ? "Actual influence" : context === "choice" ? "Influence choice" : context === "exact" ? "Exact influence requirement" : "Required influence";
    return <span className={`pc-item-influence is-${context}`} data-influence-context={context}
        aria-label={`${description}: ${label}`}>
        <GameIcon assetKey={"influence:" + label} />{prefix}{label}
    </span>;
}

/** Native snapshots and card adapters can differ in enum casing. Presentation
 * uses one label/class vocabulary without changing the stored/native value.
 */
export function RarityBadge({rarity}: {rarity: string}) {
    const key = rarity.toLowerCase();
    return <span className={`pc-rarity pc-rarity-${key}`}>{key}</span>;
}

/** State wording comes from the owning item or goal adapter, not from colour. */
export function ItemStateBadge({state, children}: {
    state: "corrupted" | "foreseeing" | "memory" | "consumed" | "destroyed" | "fractured" | "crafted";
    children: ReactNode;
}) {
    const className = state === "corrupted" ? "pc-item-corrupted"
        : state === "fractured" || state === "crafted" ? `pc-mod-state is-${state}` : `pc-item-state is-${state}`;
    return <span className={className} data-item-state={state}>{children}</span>;
}
