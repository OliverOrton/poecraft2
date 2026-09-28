import { useEffect, useSyncExternalStore } from "react";
import build from "../generated/build-info.json";
import { publicAsset } from "./public-assets";

export interface GameAsset {
    name: string; image?: string; width?: number; height?: number;
    description?: string;
    essence_mods?: Record<string, string>;
}
interface AssetCatalog {
    schema_version: number;
    source: {runtime_manifest_sha256: string};
    items: Record<string, GameAsset>;
    actions: Record<string, string>;
    influences: Record<string, string>;
    harvest?: Record<string, string>;
}
let catalog: AssetCatalog | null = null;
let pending: Promise<void> | null = null;
const subscribers = new Set<() => void>();

export function loadGameAssets(): Promise<void> {
    if (pending) return pending;
    pending = (async () => {
        // A new deployment must not reuse a still-fresh catalogue from an older build.
        const response = await fetch(publicAsset(`game-assets/catalog.json?sha256=${build.game_assets.catalog_sha256}`, build.base));
        if (!response.ok) throw new Error(`Artwork catalogue: HTTP ${response.status}`);
        const bytes = await response.arrayBuffer();
        const hash = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), b => b.toString(16).padStart(2, "0")).join("");
        if (hash !== build.game_assets.catalog_sha256) throw new Error("Artwork catalogue identity mismatch");
        const value: AssetCatalog = JSON.parse(new TextDecoder().decode(bytes));
        if (value.schema_version !== 1 || value.source.runtime_manifest_sha256 !== build.runtime.manifest_sha256) {
            throw new Error("Artwork catalogue belongs to another runtime");
        }
        catalog = value;
        subscribers.forEach(notify => notify());
    })().catch(error => { console.warn("Game artwork unavailable; text controls remain usable.", error); });
    return pending;
}

export function resolveGameAsset(key: string, value: AssetCatalog | null = catalog): GameAsset | undefined {
    if (!value || !key) return undefined;
    if (key.startsWith("action:")) return value.items[value.actions[key.slice(7)]];
    if (key.startsWith("harvest:")) return value.items[value.harvest?.[key.slice(8)] ?? ""];
    if (key.startsWith("influence:")) {
        const name = key.slice(10).toLowerCase().replace(/ t\d+$/, "");
        return value.items[value.influences[name]];
    }
    if (key.startsWith("name:")) return Object.values(value.items).find(item => item.name === key.slice(5));
    return value.items[key];
}

export function gameAssetUrl(asset: GameAsset): string | undefined {
    return asset.image && /^[a-f0-9]{64}\.png$/.test(asset.image)
        ? publicAsset(`game-assets/${asset.image}`, build.base) : undefined;
}

export function useGameAsset(key: string): GameAsset | undefined {
    const value = useSyncExternalStore(
        notify => { subscribers.add(notify); return () => { subscribers.delete(notify); }; },
        () => catalog,
        () => null,
    );
    useEffect(() => { void loadGameAssets(); }, []);
    return resolveGameAsset(key, value);
}
