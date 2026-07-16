/*
 * Compatibility facade over the workspace EconomyService. Existing price
 * consumers stay synchronous while source snapshots, per-league overrides,
 * offline cache, and pinning live in one service.
 */

import {
    economyService,
    type PinnedEconomy,
    type PriceResolution,
    type Prices,
} from "./economy-service";

export function getPrice(key: string): number | undefined {
    return economyService.getPrice(key);
}

export function getPrices(): Readonly<Prices> {
    return economyService.getPrices();
}

/** Resolve a cost key that came from the engine action registry. */
export function getActionPrice(key: string): number | undefined {
    return economyService.resolveActionPrice(key).value;
}

export function getActionPriceResolution(key: string): PriceResolution {
    return economyService.resolveActionPrice(key);
}

export function getFallbackPrice(): number | undefined {
    return economyService.getFallbackPrice();
}

export function setFallbackPrice(value: number | null): void {
    economyService.setFallbackPrice(value);
}

/** Set a per-profile override; null/NaN/negative clears the key. */
export function setPrice(key: string, value: number | null): void {
    economyService.setPrice(key, value);
}

export function pinEconomy(actionKeys: Iterable<string> = []): PinnedEconomy {
    return economyService.pin(actionKeys);
}

export function onPricesChange(listener: () => void): () => void {
    return economyService.onChange(listener);
}

export { economyService };
