import { useSyncExternalStore } from "react";
import { harvestMaterials } from "../harvest-crafts";
import { useGameAsset } from "../game-assets";
import { economyService, getActionPriceResolution, onPricesChange } from "../workspace/prices";
import { formatChaosValue, priceSourceLabel } from "../odds-presentation";
import { GameIcon } from "./pc-game-icon";
import { disconnectReact, renderReact } from "../react-host";

export function useCraftPrices(): void {
    useSyncExternalStore(onPricesChange, () => economyService.getState());
}

function Material({part, multiplier}: {part: ReturnType<typeof harvestMaterials>[number]; multiplier: number}) {
    const asset = useGameAsset(part.assetKey);
    return <span className="pc-craft-material" data-material-key={part.key}>
        <GameIcon assetKey={part.assetKey} /><span>{(part.quantity * multiplier).toLocaleString("en-US")} × {asset?.name ?? part.key.split(":")[1]}</span>
    </span>;
}

export function HarvestMaterials({costKey, multiplier = 1}: {costKey: string; multiplier?: number}) {
    return <div className="pc-craft-materials" aria-label="Required materials">
        {harvestMaterials(costKey).map(part => <Material key={part.key} part={part} multiplier={multiplier} />)}
    </div>;
}

export function HarvestCost({costKey}: {costKey: string}) {
    useCraftPrices();
    const price = getActionPriceResolution(costKey);
    return <div className="pc-craft-cost" data-craft-cost={costKey}>
        <HarvestMaterials costKey={costKey} />
        <span className="pc-craft-cost-value">{price.value === undefined ? "Price unavailable" : formatChaosValue(price.value)}
            {price.source && <small> · {priceSourceLabel(price.source)}</small>}</span>
    </div>;
}

/** The odds report still uses imperative markup; share the same materials view. */
class PcHarvestMaterials extends HTMLElement {
    static observedAttributes = ["cost-key"];
    connectedCallback(): void { this.render(); }
    disconnectedCallback(): void { disconnectReact(this); }
    attributeChangedCallback(): void { if (this.isConnected) this.render(); }
    private render(): void { renderReact(this, <HarvestMaterials costKey={this.getAttribute("cost-key") ?? ""} />); }
}
customElements.define("pc-harvest-materials", PcHarvestMaterials);
