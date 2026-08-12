import type { EconomyIdentity } from "../engine-protocol";

export type Prices = Record<string, number>;
export type PublishedPriceSource =
    | "quote"
    | "recipe"
    | "zero"
    | "owner_default";
export type PriceSource = PublishedPriceSource | "override" | "fallback";
export interface PriceResolution {
    value: number | undefined;
    source: PriceSource | null;
}
export type EconomyStatus =
    | "loading"
    | "fresh"
    | "stale"
    | "offline"
    | "manual-only";

export interface EconomySnapshotMetadata {
    schema_version: 1;
    game: "poe1";
    realm: string;
    league_key: string;
    league_name: string;
    source: string;
    source_cutoff_at_utc: string | null;
    created_at_utc: string | null;
    game_data_hash: string | null;
    price_count: number;
    missing_keys: string[];
    low_confidence_keys: string[];
    content_sha256: string | null;
}

export interface EconomySnapshot {
    version: "v1";
    id: string;
    metadata: EconomySnapshotMetadata;
    prices: Prices;
    /** Publisher-certified provenance; absent on pre-S7.2R cached snapshots. */
    sources?: Record<string, PublishedPriceSource>;
}

export interface LeagueIndexEntry {
    league_key: string;
    display_name: string;
    realm: string;
    ruleset: string;
    temporary: boolean;
    active: boolean;
    archived: boolean;
    latest_snapshot_id: string | null;
    latest_snapshot_url: string | null;
    content_sha256: string | null;
    source_cutoff_at_utc: string | null;
    stale: boolean;
    error: string | null;
    coverage: {
        priced: number;
        missing: number;
        low_confidence: number;
    };
}

export interface LeagueIndex {
    schema_version: 1;
    generated_at_utc: string;
    leagues: LeagueIndexEntry[];
}

export interface EconomyState {
    initialized: boolean;
    status: EconomyStatus;
    switchingTo: string | null;
    selectedProfile: string;
    index: LeagueIndex | null;
    sourceSnapshot: EconomySnapshot | null;
    effectiveSnapshotId: string;
    error: string | null;
}

export interface PinnedEconomy {
    snapshot: EconomySnapshot;
    profile: string;
    sourceSnapshotId: string;
    sourceContentSha256: string | null;
    sourceCutoffAtUtc: string | null;
    priceSources: Record<string, PriceSource>;
    fallbackPrice: number | null;
    identity: EconomyIdentity;
}

export interface EconomyCache {
    get<T>(key: string): Promise<T | null>;
    set<T>(key: string, value: T): Promise<void>;
}

interface StorageLike {
    getItem(key: string): string | null;
    setItem(key: string, value: string): void;
    removeItem(key: string): void;
}

interface FetchLike {
    (input: string | URL, init?: RequestInit): Promise<Response>;
}

export interface EconomyServiceOptions {
    indexUrl?: string;
    fetch?: FetchLike;
    cache?: EconomyCache;
    storage?: StorageLike;
    now?: () => number;
    broadcast?: boolean;
}

const INDEX_CACHE_KEY = "league-index";
const SELECTED_KEY = "poecraft.economy.selected.v1";
const OVERRIDES_KEY = "poecraft.economy.overrides.v1";
const FALLBACKS_KEY = "poecraft.economy.fallbacks.v1";
const MIGRATED_KEY = "poecraft.economy.legacy-prices-migrated.v1";
const LEGACY_PRICES_KEY = "poecraft.prices";
const MANUAL_PROFILE = "manual";

function defaultIndexUrl(): string {
    const configured = (globalThis as typeof globalThis & {
        POECRAFT_ECONOMY_INDEX_URL?: string;
    }).POECRAFT_ECONOMY_INDEX_URL;
    return configured || "/economy/league-index.json";
}

class MemoryStorage implements StorageLike {
    private readonly values = new Map<string, string>();
    getItem(key: string): string | null {
        return this.values.get(key) ?? null;
    }
    setItem(key: string, value: string): void {
        this.values.set(key, value);
    }
    removeItem(key: string): void {
        this.values.delete(key);
    }
}

export class MemoryEconomyCache implements EconomyCache {
    private readonly values = new Map<string, unknown>();
    async get<T>(key: string): Promise<T | null> {
        return (this.values.get(key) as T | undefined) ?? null;
    }
    async set<T>(key: string, value: T): Promise<void> {
        this.values.set(key, structuredClone(value));
    }
}

class IndexedDbEconomyCache implements EconomyCache {
    private readonly fallback = new MemoryEconomyCache();
    private database: Promise<IDBDatabase> | null = null;

    private open(): Promise<IDBDatabase> {
        if (this.database) return this.database;
        this.database = new Promise((resolve, reject) => {
            const request = indexedDB.open("poecraft.economy", 1);
            request.onupgradeneeded = () => {
                if (!request.result.objectStoreNames.contains("artifacts")) {
                    request.result.createObjectStore("artifacts");
                }
            };
            request.onsuccess = () => resolve(request.result);
            request.onerror = () => reject(request.error);
        });
        return this.database;
    }

    async get<T>(key: string): Promise<T | null> {
        if (typeof indexedDB === "undefined") return this.fallback.get<T>(key);
        try {
            const database = await this.open();
            return await new Promise<T | null>((resolve, reject) => {
                const request = database
                    .transaction("artifacts", "readonly")
                    .objectStore("artifacts")
                    .get(key);
                request.onsuccess = () => resolve((request.result as T) ?? null);
                request.onerror = () => reject(request.error);
            });
        } catch {
            return this.fallback.get<T>(key);
        }
    }

    async set<T>(key: string, value: T): Promise<void> {
        if (typeof indexedDB === "undefined") {
            await this.fallback.set(key, value);
            return;
        }
        try {
            const database = await this.open();
            await new Promise<void>((resolve, reject) => {
                const transaction = database.transaction("artifacts", "readwrite");
                transaction.objectStore("artifacts").put(value, key);
                transaction.oncomplete = () => resolve();
                transaction.onerror = () => reject(transaction.error);
            });
        } catch {
            await this.fallback.set(key, value);
        }
    }
}

function parsePrices(value: unknown): Prices {
    if (!value || typeof value !== "object") return {};
    return Object.fromEntries(
        Object.entries(value as Record<string, unknown>).filter(
            (entry): entry is [string, number] =>
                typeof entry[1] === "number" &&
                Number.isFinite(entry[1]) &&
                entry[1] >= 0,
        ),
    );
}

function compareCanonicalKeys(left: string, right: string): number {
    return left < right ? -1 : left > right ? 1 : 0;
}

function sortCanonical(value: unknown): unknown {
    if (Array.isArray(value)) return value.map(sortCanonical);
    if (value && typeof value === "object") {
        return Object.fromEntries(
            Object.entries(value as Record<string, unknown>)
                .sort(([left], [right]) => compareCanonicalKeys(left, right))
                .map(([key, child]) => [key, sortCanonical(child)]),
        );
    }
    return value;
}

function priceToken(value: number): string {
    if (!Number.isFinite(value) || value < 0) {
        throw new Error("economy prices must be finite and non-negative");
    }
    return value.toFixed(12).replace(/(?:\.0+|(?:(\.\d*?)0+))$/, "$1");
}

function snapshotHashContent(snapshot: EconomySnapshot): unknown {
    const content: Record<string, unknown> = {
        schema_version: 1,
        league_key: snapshot.metadata.league_key,
        game_data_hash: snapshot.metadata.game_data_hash,
        prices: Object.fromEntries(
            Object.entries(snapshot.prices)
                .sort(([left], [right]) => compareCanonicalKeys(left, right))
                .map(([key, value]) => [key, priceToken(value)]),
        ),
        missing_keys: [...snapshot.metadata.missing_keys].sort(),
        low_confidence_keys: [...snapshot.metadata.low_confidence_keys].sort(),
    };
    if (snapshot.sources) {
        content.sources = Object.fromEntries(
            Object.entries(snapshot.sources).sort(([left], [right]) =>
                compareCanonicalKeys(left, right),
            ),
        );
    }
    return content;
}

async function sha256(value: string): Promise<string> {
    if (!globalThis.crypto?.subtle) {
        throw new Error("Web Crypto is required to verify economy snapshots");
    }
    const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));
    return [...new Uint8Array(digest)]
        .map((byte) => byte.toString(16).padStart(2, "0"))
        .join("");
}

async function readJson<T>(response: Response, label: string): Promise<T> {
    const contentType = response.headers.get("content-type")?.toLowerCase() ?? "";
    if (contentType && !contentType.includes("json")) {
        throw new Error(`${label} response was not JSON`);
    }
    try {
        return (await response.json()) as T;
    } catch {
        throw new Error(`${label} response was not valid JSON`);
    }
}

export async function verifyEconomySnapshot(
    snapshot: EconomySnapshot,
    expectedHash: string,
): Promise<void> {
    if (snapshot.version !== "v1" || snapshot.metadata.schema_version !== 1) {
        throw new Error("unsupported economy snapshot schema");
    }
    const prices = parsePrices(snapshot.prices);
    if (Object.keys(prices).length !== Object.keys(snapshot.prices).length) {
        throw new Error("economy snapshot contains an invalid price");
    }
    const actual = await sha256(JSON.stringify(sortCanonical(snapshotHashContent(snapshot))));
    if (actual !== expectedHash || snapshot.metadata.content_sha256 !== expectedHash) {
        throw new Error("economy snapshot content hash mismatch");
    }
}

function quickIdentity(value: string): string {
    let hash = 0xcbf29ce484222325n;
    for (const byte of new TextEncoder().encode(value)) {
        hash ^= BigInt(byte);
        hash = BigInt.asUintN(64, hash * 0x100000001b3n);
    }
    return hash.toString(16).padStart(16, "0");
}

function manualSnapshot(): EconomySnapshot {
    return {
        version: "v1",
        id: "public-none",
        metadata: {
            schema_version: 1,
            game: "poe1",
            realm: "pc",
            league_key: MANUAL_PROFILE,
            league_name: "Custom / manual",
            source: "none",
            source_cutoff_at_utc: null,
            created_at_utc: null,
            game_data_hash: null,
            price_count: 0,
            missing_keys: [],
            low_confidence_keys: [],
            content_sha256: null,
        },
        prices: {},
    };
}

function snapshotCacheKey(id: string): string {
    return `snapshot:${id}`;
}

export class EconomyService {
    private readonly indexUrl: string;
    private readonly fetcher: FetchLike | null;
    private readonly cache: EconomyCache;
    private readonly storage: StorageLike;
    private readonly now: () => number;
    private readonly listeners = new Set<() => void>();
    private readonly channel: BroadcastChannel | null;
    private initialization: Promise<void> | null = null;
    private lastDownloadUsedCache = false;
    private overrides: Record<string, Prices> = {};
    private fallbacks: Record<string, number> = {};
    private state: EconomyState = {
        initialized: false,
        status: "loading",
        switchingTo: null,
        selectedProfile: MANUAL_PROFILE,
        index: null,
        sourceSnapshot: null,
        effectiveSnapshotId: "public-none:manual",
        error: null,
    };

    constructor(options: EconomyServiceOptions = {}) {
        this.indexUrl = options.indexUrl ?? defaultIndexUrl();
        this.fetcher = options.fetch ?? (typeof fetch === "function" ? fetch.bind(globalThis) : null);
        this.cache = options.cache ?? new IndexedDbEconomyCache();
        this.storage =
            options.storage ??
            (typeof localStorage !== "undefined" ? localStorage : new MemoryStorage());
        this.now = options.now ?? (() => Date.now());
        this.channel =
            options.broadcast !== false &&
            typeof window !== "undefined" &&
            typeof BroadcastChannel !== "undefined"
                ? new BroadcastChannel("poecraft.economy")
                : null;
        this.channel?.addEventListener("message", () => this.reloadLocalState());
        if (typeof window !== "undefined") {
            window.addEventListener("storage", (event) => {
                if (
                    event.key === OVERRIDES_KEY ||
                    event.key === FALLBACKS_KEY ||
                    event.key === SELECTED_KEY
                ) {
                    void this.reloadLocalState();
                }
            });
        }
    }

    getState(): Readonly<EconomyState> {
        return this.state;
    }

    initialize(): Promise<void> {
        if (!this.initialization) this.initialization = this.initializeOnce();
        return this.initialization;
    }

    private async initializeOnce(): Promise<void> {
        this.migrateLegacyPrices();
        this.readOverrides();
        this.readFallbacks();
        const storedSelection = this.storage.getItem(SELECTED_KEY);
        const cachedIndex = await this.cache.get<LeagueIndex>(INDEX_CACHE_KEY);
        if (cachedIndex?.schema_version === 1) {
            this.state = { ...this.state, index: cachedIndex };
            const profile = storedSelection || this.defaultProfile(cachedIndex);
            await this.activateCached(profile, cachedIndex);
        } else if (storedSelection === MANUAL_PROFILE) {
            this.activateManual();
        }
        try {
            await this.refresh();
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            if (
                !this.state.sourceSnapshot ||
                this.state.selectedProfile === MANUAL_PROFILE
            ) {
                this.activateManual();
                this.state = { ...this.state, error: message };
            } else {
                this.state = {
                    ...this.state,
                    status: "offline",
                    error: message,
                };
            }
        }
        this.state = { ...this.state, initialized: true };
        this.persistSelection();
        this.emit();
    }

    async refresh(): Promise<void> {
        if (!this.fetcher) throw new Error("economy index fetch is unavailable");
        const response = await this.fetcher(this.indexUrl, { cache: "no-cache" });
        if (!response.ok) throw new Error(`economy index request failed (${response.status})`);
        const index = await readJson<LeagueIndex>(response, "economy index");
        this.validateIndex(index);
        await this.cache.set(INDEX_CACHE_KEY, index);
        const stored = this.storage.getItem(SELECTED_KEY);
        let target = stored || this.defaultProfile(index);
        if (
            target !== MANUAL_PROFILE &&
            !index.leagues.some((league) => league.league_key === target)
        ) {
            target = this.defaultProfile(index);
        }
        if (target === MANUAL_PROFILE) {
            this.state = { ...this.state, index };
            this.activateManual();
            this.persistSelection();
            return;
        }
        const entry = index.leagues.find((league) => league.league_key === target)!;
        const snapshot = await this.downloadSnapshot(entry, true);
        this.activate(
            entry,
            snapshot,
            index,
            this.lastDownloadUsedCache ? "offline" : entry.stale ? "stale" : "fresh",
        );
        this.persistSelection();
    }

    async selectProfile(profile: string): Promise<void> {
        if (profile === MANUAL_PROFILE) {
            this.activateManual();
            this.persistSelection();
            return;
        }
        const index = this.state.index;
        const entry = index?.leagues.find((league) => league.league_key === profile);
        if (!entry || !index) throw new Error(`unknown economy league: ${profile}`);
        const previous = this.state;
        this.state = { ...this.state, switchingTo: profile, error: null };
        this.emit();
        try {
            const snapshot = await this.downloadSnapshot(entry, true);
            this.activate(
                entry,
                snapshot,
                index,
                this.lastDownloadUsedCache
                    ? "offline"
                    : entry.stale
                      ? "stale"
                      : "fresh",
            );
            this.persistSelection();
        } catch (error) {
            this.state = {
                ...previous,
                switchingTo: null,
                error: error instanceof Error ? error.message : String(error),
            };
            this.emit();
            throw error;
        }
    }

    private async activateCached(profile: string, index: LeagueIndex): Promise<void> {
        if (profile === MANUAL_PROFILE) {
            this.activateManual();
            return;
        }
        const entry = index.leagues.find((league) => league.league_key === profile);
        if (!entry?.latest_snapshot_id || !entry.content_sha256) return;
        const snapshot = await this.cache.get<EconomySnapshot>(
            snapshotCacheKey(entry.latest_snapshot_id),
        );
        if (!snapshot) return;
        try {
            await verifyEconomySnapshot(snapshot, entry.content_sha256);
            this.activate(entry, snapshot, index, "offline");
        } catch {
            // Invalid cache entries are never activated.
        }
    }

    private async downloadSnapshot(
        entry: LeagueIndexEntry,
        allowCached: boolean,
    ): Promise<EconomySnapshot> {
        if (!entry.latest_snapshot_id || !entry.latest_snapshot_url || !entry.content_sha256) {
            throw new Error(entry.error || `no economy snapshot for ${entry.display_name}`);
        }
        let networkError: unknown = null;
        this.lastDownloadUsedCache = false;
        if (this.fetcher) {
            try {
                const base = new URL(
                    this.indexUrl,
                    typeof location !== "undefined" ? location.href : "http://localhost/",
                );
                const url = new URL(entry.latest_snapshot_url, base).toString();
                const response = await this.fetcher(url, { cache: "no-cache" });
                if (!response.ok) {
                    throw new Error(`economy snapshot request failed (${response.status})`);
                }
                const snapshot = await readJson<EconomySnapshot>(
                    response,
                    "economy snapshot",
                );
                if (snapshot.id !== entry.latest_snapshot_id) {
                    throw new Error("economy snapshot identity mismatch");
                }
                await verifyEconomySnapshot(snapshot, entry.content_sha256);
                await this.cache.set(snapshotCacheKey(snapshot.id), snapshot);
                return snapshot;
            } catch (error) {
                networkError = error;
            }
        }
        if (allowCached) {
            const cached = await this.cache.get<EconomySnapshot>(
                snapshotCacheKey(entry.latest_snapshot_id),
            );
            if (cached) {
                await verifyEconomySnapshot(cached, entry.content_sha256);
                this.lastDownloadUsedCache = true;
                return cached;
            }
        }
        throw networkError || new Error(`economy snapshot unavailable: ${entry.display_name}`);
    }

    private validateIndex(index: LeagueIndex): void {
        if (index.schema_version !== 1 || !Array.isArray(index.leagues)) {
            throw new Error("unsupported economy league index");
        }
        const keys = new Set<string>();
        for (const league of index.leagues) {
            if (!league.league_key || keys.has(league.league_key)) {
                throw new Error("economy league index contains duplicate or empty keys");
            }
            keys.add(league.league_key);
        }
    }

    private defaultProfile(index: LeagueIndex): string {
        return (
            index.leagues.find(
                (league) =>
                    league.active && league.temporary && league.ruleset === "softcore",
            )?.league_key ??
            index.leagues.find(
                (league) => league.active && league.display_name === "Standard",
            )?.league_key ??
            MANUAL_PROFILE
        );
    }

    private activate(
        entry: LeagueIndexEntry,
        snapshot: EconomySnapshot,
        index: LeagueIndex,
        status: EconomyStatus,
    ): void {
        this.state = {
            initialized: this.state.initialized,
            status,
            switchingTo: null,
            selectedProfile: entry.league_key,
            index,
            sourceSnapshot: snapshot,
            effectiveSnapshotId: this.effectiveId(snapshot, entry.league_key),
            error: entry.error,
        };
        this.emit();
    }

    private activateManual(): void {
        const snapshot = manualSnapshot();
        this.state = {
            ...this.state,
            status: "manual-only",
            switchingTo: null,
            selectedProfile: MANUAL_PROFILE,
            sourceSnapshot: snapshot,
            effectiveSnapshotId: this.effectiveId(snapshot, MANUAL_PROFILE),
            error: null,
        };
        this.emit();
    }

    private persistSelection(): void {
        this.storage.setItem(SELECTED_KEY, this.state.selectedProfile);
        this.channel?.postMessage("selection");
    }

    private readOverrides(): void {
        try {
            const raw = this.storage.getItem(OVERRIDES_KEY);
            const parsed = raw ? (JSON.parse(raw) as unknown) : null;
            this.overrides =
                parsed && typeof parsed === "object"
                    ? Object.fromEntries(
                          Object.entries(parsed as Record<string, unknown>).map(
                              ([profile, prices]) => [profile, parsePrices(prices)],
                          ),
                      )
                    : {};
        } catch {
            this.overrides = {};
        }
    }

    private readFallbacks(): void {
        try {
            const raw = this.storage.getItem(FALLBACKS_KEY);
            const parsed = raw ? (JSON.parse(raw) as unknown) : null;
            this.fallbacks =
                parsed && typeof parsed === "object"
                    ? Object.fromEntries(
                          Object.entries(parsed as Record<string, unknown>).filter(
                              (entry): entry is [string, number] =>
                                  typeof entry[1] === "number" &&
                                  Number.isFinite(entry[1]) &&
                                  entry[1] > 0,
                          ),
                      )
                    : {};
        } catch {
            this.fallbacks = {};
        }
    }

    private migrateLegacyPrices(): void {
        if (this.storage.getItem(MIGRATED_KEY)) return;
        try {
            const legacy = this.storage.getItem(LEGACY_PRICES_KEY);
            const prices = legacy ? parsePrices(JSON.parse(legacy)) : {};
            if (Object.keys(prices).length) {
                this.readOverrides();
                this.overrides = { ...this.overrides, [MANUAL_PROFILE]: prices };
                this.storage.setItem(OVERRIDES_KEY, JSON.stringify(this.overrides));
            }
            this.storage.removeItem(LEGACY_PRICES_KEY);
        } catch {
            // A malformed legacy table is intentionally not applied to any league.
        }
        this.storage.setItem(MIGRATED_KEY, "1");
    }

    private async reloadLocalState(): Promise<void> {
        this.readOverrides();
        this.readFallbacks();
        const selected = this.storage.getItem(SELECTED_KEY) || MANUAL_PROFILE;
        if (selected !== this.state.selectedProfile) {
            try {
                await this.selectProfile(selected);
            } catch {
                // Another tab may have selected a league not present in our stale index.
            }
            return;
        }
        const source = this.state.sourceSnapshot || manualSnapshot();
        this.state = {
            ...this.state,
            effectiveSnapshotId: this.effectiveId(source, selected),
        };
        this.emit();
    }

    getPrice(key: string): number | undefined {
        return this.getPrices()[key];
    }

    /** Resolve an engine-certified action key, optionally using the visible
     * non-zero fallback. Callers must not use this for arbitrary user keys. */
    resolveActionPrice(key: string): PriceResolution {
        const profile = this.state.selectedProfile;
        const override = this.overrides[profile] ?? {};
        if (Object.hasOwn(override, key)) {
            return { value: override[key], source: "override" };
        }
        const source = this.state.sourceSnapshot;
        if (source && Object.hasOwn(source.prices, key)) {
            const value = source.prices[key];
            return {
                value,
                source:
                    source.sources?.[key] ?? (value === 0 ? "zero" : "quote"),
            };
        }
        const fallback = this.fallbacks[profile];
        return fallback > 0
            ? { value: fallback, source: "fallback" }
            : { value: undefined, source: null };
    }

    getFallbackPrice(): number | undefined {
        return this.fallbacks[this.state.selectedProfile];
    }

    setFallbackPrice(value: number | null): void {
        const profile = this.state.selectedProfile;
        const next = { ...this.fallbacks };
        if (value === null || !Number.isFinite(value) || value <= 0) {
            delete next[profile];
        } else {
            next[profile] = value;
        }
        this.fallbacks = next;
        this.storage.setItem(FALLBACKS_KEY, JSON.stringify(this.fallbacks));
        const source = this.state.sourceSnapshot || manualSnapshot();
        this.state = {
            ...this.state,
            effectiveSnapshotId: this.effectiveId(source, profile),
        };
        this.channel?.postMessage("fallbacks");
        this.emit();
    }

    getPrices(): Readonly<Prices> {
        const source = this.state.sourceSnapshot?.prices ?? {};
        const override = this.overrides[this.state.selectedProfile] ?? {};
        return { ...source, ...override };
    }

    setPrice(key: string, value: number | null): void {
        const profile = this.state.selectedProfile;
        const prices = { ...(this.overrides[profile] ?? {}) };
        if (value === null || !Number.isFinite(value) || value < 0) delete prices[key];
        else prices[key] = value;
        this.overrides = { ...this.overrides, [profile]: prices };
        this.storage.setItem(OVERRIDES_KEY, JSON.stringify(this.overrides));
        const source = this.state.sourceSnapshot || manualSnapshot();
        this.state = {
            ...this.state,
            effectiveSnapshotId: this.effectiveId(source, profile),
        };
        this.channel?.postMessage("overrides");
        this.emit();
    }

    pin(actionKeys: Iterable<string> = []): PinnedEconomy {
        const source = this.state.sourceSnapshot || manualSnapshot();
        const prices = { ...this.getPrices() };
        const profile = this.state.selectedProfile;
        const priceSources: Record<string, PriceSource> = {};
        const validKeys = Array.from(new Set(actionKeys)).sort(compareCanonicalKeys);
        for (const key of validKeys) {
            const resolution = this.resolveActionPrice(key);
            if (resolution.value === undefined || !resolution.source) continue;
            prices[key] = resolution.value;
            priceSources[key] = resolution.source;
        }
        const fallbackPrice = this.getFallbackPrice() ?? null;
        const pinToken = JSON.stringify(
            sortCanonical({
                effective: this.state.effectiveSnapshotId,
                valid_action_keys: validKeys,
                action_price_sources: priceSources,
                fallback_price: fallbackPrice,
            }),
        );
        const pinnedId = validKeys.length
            ? `${this.state.effectiveSnapshotId}:pin:${quickIdentity(pinToken)}`
            : this.state.effectiveSnapshotId;
        const snapshot: EconomySnapshot = {
            version: "v1",
            id: pinnedId,
            metadata: {
                ...source.metadata,
                price_count: Object.keys(prices).length,
            },
            prices,
            sources: {
                ...(source.sources ?? {}),
                ...Object.fromEntries(
                    Object.entries(priceSources).flatMap(([key, priceSource]) =>
                        priceSource === "quote" ||
                        priceSource === "recipe" ||
                        priceSource === "zero" ||
                        priceSource === "owner_default"
                            ? [[key, priceSource]]
                            : [],
                    ),
                ),
            },
        };
        return {
            snapshot,
            profile,
            sourceSnapshotId: source.id,
            sourceContentSha256: source.metadata.content_sha256,
            sourceCutoffAtUtc: source.metadata.source_cutoff_at_utc,
            priceSources,
            fallbackPrice,
            identity: {
                profile,
                effective_snapshot_id: pinnedId,
                source_snapshot_id: source.id,
                source_content_sha256: source.metadata.content_sha256,
                source_cutoff_at_utc: source.metadata.source_cutoff_at_utc,
                league_name: source.metadata.league_name,
                status: this.state.status,
                low_confidence_keys: [...source.metadata.low_confidence_keys],
                price_sources: priceSources,
                fallback_price: fallbackPrice,
            },
        };
    }

    sourceAgeMs(): number | null {
        const cutoff = this.state.sourceSnapshot?.metadata.source_cutoff_at_utc;
        if (!cutoff) return null;
        const parsed = Date.parse(cutoff);
        return Number.isFinite(parsed) ? Math.max(0, this.now() - parsed) : null;
    }

    onChange(listener: () => void): () => void {
        this.listeners.add(listener);
        return () => this.listeners.delete(listener);
    }

    private effectiveId(source: EconomySnapshot, profile: string): string {
        const override = this.overrides[profile] ?? {};
        const identity = JSON.stringify(
            sortCanonical({
                source: source.id,
                profile,
                overrides: override,
                fallback: this.fallbacks[profile] ?? null,
            }),
        );
        return `${source.id}:effective:${quickIdentity(identity)}`;
    }

    private emit(): void {
        this.listeners.forEach((listener) => listener());
    }
}

export const economyService = new EconomyService();
export { MANUAL_PROFILE };
