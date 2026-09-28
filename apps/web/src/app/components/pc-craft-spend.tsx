import type { Catalog } from "../engine-protocol";
import type { CraftSpend } from "../craft-costs";
import { craftActionLabel } from "../craft-choices";
import { useGameAsset } from "../game-assets";
import { formatChaosValue, priceSourceLabel } from "../odds-presentation";
import { getActionPriceResolution, setPrice } from "../workspace/prices";
import { disconnectReact, renderReact } from "../react-host";
import { HarvestMaterials, useCraftPrices } from "./craft-cost";

function SpendRow({costKey, quantity, catalog}: {costKey: string; quantity: number; catalog: Catalog}) {
    const [kind, ...parts] = costKey.split(":");
    const parameter = parts.join(":");
    const asset = useGameAsset(kind === "essence" || kind === "fossil" ? parameter : `action:${costKey}`);
    const entries = kind === "bench" ? catalog.bench : kind === "essence" ? catalog.essences : kind === "fossil" ? catalog.fossils : [];
    const label = asset?.name ?? entries.find(entry => entry.key === parameter)?.name ??
        craftActionLabel(kind === "beast" ? parameter.replace(/-/g, " ") : costKey.replace(/:/g, " · "));
    const price = getActionPriceResolution(costKey);
    return <li data-spend-key={costKey}>
        <div className="pc-spend-row"><span>{quantity.toLocaleString("en-US")} × {label}</span>
            <strong>{price.value === undefined ? "Unpriced" : formatChaosValue(price.value * quantity)}</strong></div>
        {kind.startsWith("harvest_") && <HarvestMaterials costKey={costKey} multiplier={quantity} />}
        <label className="pc-spend-price"><span>Chaos each</span>
            <input type="number" min="0" step="any" aria-label={`Chaos price for ${label}`} value={price.value ?? ""}
                placeholder="Set price" onChange={event => setPrice(costKey, event.currentTarget.value === "" ? null : Number(event.currentTarget.value))} />
            <small>{price.source ? priceSourceLabel(price.source) : "Missing price"}</small>
        </label>
    </li>;
}

export function CraftSpendView({spend, catalog}: {spend: CraftSpend; catalog: Catalog}) {
    useCraftPrices();
    const entries = Object.entries(spend.counts);
    let total = 0;
    let missing = 0;
    for (const [key, quantity] of entries) {
        const price = getActionPriceResolution(key).value;
        if (price === undefined) missing++;
        else total += price * quantity;
    }
    return <section aria-label="Craft spend">
        <div className="pc-spend-heading"><h3>Craft spend</h3><strong data-spend-total>
            {formatChaosValue(total)}{missing || spend.untracked ? " + unpriced" : ""}
        </strong></div>
        <p className="pc-spend-note">Current prices · follows Undo / Redo. Base and manual item edits excluded.</p>
        {spend.untracked && <p className="pc-spend-incomplete">Some history steps have no cost data.</p>}
        {missing > 0 && <p className="pc-spend-incomplete">{missing} material {missing === 1 ? "price missing" : "prices missing"}.</p>}
        {!!entries.length && <details><summary>Materials &amp; prices</summary><ul className="pc-spend-materials">
            {entries.map(([key, quantity]) => <SpendRow key={key} costKey={key} quantity={quantity} catalog={catalog} />)}
        </ul></details>}
    </section>;
}

export class PcCraftSpend extends HTMLElement {
    private model: {spend: CraftSpend; catalog: Catalog} | null = null;
    connectedCallback(): void { if (this.model) this.setModel(this.model); }
    disconnectedCallback(): void { disconnectReact(this); }
    setModel(model: {spend: CraftSpend; catalog: Catalog}): void { this.model = model; renderReact(this, <CraftSpendView {...model} />); }
}
customElements.define("pc-craft-spend", PcCraftSpend);
