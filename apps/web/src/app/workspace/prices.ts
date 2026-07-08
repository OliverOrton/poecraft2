/*
 * Workspace-level price table: user-entered chaos-equivalent prices per
 * economy cost key ("chaos", "essence:<key>", "base", ...). This is the
 * manual-override layer of the Economy service planned in
 * docs/desktop-workspace-ui.md; the fetched-snapshot layer comes later.
 * Persisted in localStorage so every tab and document shares one table.
 */

const PRICES_KEY = "poecraft.prices";

type Prices = Record<string, number>;

const listeners = new Set<() => void>();
let cache: Prices | null = null;

function read(): Prices {
    if (cache) {
        return cache;
    }
    try {
        const raw = localStorage.getItem(PRICES_KEY);
        const parsed = raw ? (JSON.parse(raw) as unknown) : null;
        cache =
            parsed && typeof parsed === "object"
                ? Object.fromEntries(
                      Object.entries(parsed as Record<string, unknown>).filter(
                          (entry): entry is [string, number] =>
                              typeof entry[1] === "number" &&
                              Number.isFinite(entry[1]),
                      ),
                  )
                : {};
    } catch {
        cache = {};
    }
    return cache;
}

export function getPrice(key: string): number | undefined {
    return read()[key];
}

export function getPrices(): Readonly<Prices> {
    return read();
}

/** Set a price; null/NaN clears the key. Notifies subscribers. */
export function setPrice(key: string, value: number | null): void {
    const prices = { ...read() };
    if (value === null || !Number.isFinite(value)) {
        delete prices[key];
    } else {
        prices[key] = value;
    }
    cache = prices;
    try {
        localStorage.setItem(PRICES_KEY, JSON.stringify(prices));
    } catch {
        /* storage full or unavailable — prices stay in-memory */
    }
    listeners.forEach((listener) => listener());
}

/** Subscribe to price edits (this tab and others); returns unsubscribe. */
export function onPricesChange(listener: () => void): () => void {
    listeners.add(listener);
    return () => listeners.delete(listener);
}

// Cross-browser-tab edits invalidate the cache and notify.
if (typeof window !== "undefined") {
    window.addEventListener("storage", (event) => {
        if (event.key === PRICES_KEY) {
            cache = null;
            listeners.forEach((listener) => listener());
        }
    });
}
