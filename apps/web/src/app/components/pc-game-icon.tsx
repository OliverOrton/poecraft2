import { useState } from "react";
import { gameAssetUrl, useGameAsset } from "../game-assets";
import { disconnectReact, renderReact } from "../react-host";

export function GameIcon({assetKey, size = "control"}: {assetKey: string; size?: "control" | "item"}) {
    const asset = useGameAsset(assetKey);
    const url = asset && gameAssetUrl(asset);
    const [failed, setFailed] = useState<string | null>(null);
    if (!url || url === failed) return null;
    return <img className={"pc-game-art pc-game-art-" + size} src={url} alt="" aria-hidden="true"
        width={asset?.width} height={asset?.height} loading="lazy" decoding="async" onError={() => setFailed(url)} />;
}

export function GameItemName({assetKey, fallback}: {assetKey: string; fallback: string}) {
    return <>{useGameAsset(assetKey)?.name ?? fallback}</>;
}

/** Bridge for the remaining imperative document controllers. */
export class PcGameIcon extends HTMLElement {
    static observedAttributes = ["asset-key", "size"];
    connectedCallback(): void { this.render(); }
    disconnectedCallback(): void { disconnectReact(this); }
    attributeChangedCallback(): void { if (this.isConnected) this.render(); }
    private render(): void {
        renderReact(this, <GameIcon assetKey={this.getAttribute("asset-key") ?? ""} size={this.getAttribute("size") === "item" ? "item" : "control"} />);
    }
}
customElements.define("pc-game-icon", PcGameIcon);
