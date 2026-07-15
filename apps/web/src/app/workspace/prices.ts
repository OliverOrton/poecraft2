/*
 * Compatibility facade over the workspace EconomyService. Existing price
 * consumers stay synchronous while source snapshots, per-league overrides,
 * offline cache, and pinning live in one service.
 */

import {
    economyService,
    type PinnedEconomy,
    type Prices,
} from "./economy-service";

export function getPrice(key: string): number | undefined {
    return economyService.getPrice(key);
}

export function getPrices(): Readonly<Prices> {
    return economyService.getPrices();
}

/** Set a per-profile override; null/NaN/negative clears the key. */
export function setPrice(key: string, value: number | null): void {
    economyService.setPrice(key, value);
}

export function pinEconomy(): PinnedEconomy {
    return economyService.pin();
}

export function onPricesChange(listener: () => void): () => void {
    return economyService.onChange(listener);
}

export { economyService };
