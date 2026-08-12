import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
    EconomyService,
    MemoryEconomyCache,
    type EconomySnapshot,
    type LeagueIndex,
    type LeagueIndexEntry,
    verifyEconomySnapshot,
} from "../src/app/workspace/economy-service";

class TestStorage {
    readonly values = new Map<string, string>();
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

function canonical(value: unknown): unknown {
    if (Array.isArray(value)) return value.map(canonical);
    if (value && typeof value === "object") {
        return Object.fromEntries(
            Object.entries(value as Record<string, unknown>)
                .sort(([left], [right]) =>
                    left < right ? -1 : left > right ? 1 : 0,
                )
                .map(([key, child]) => [key, canonical(child)]),
        );
    }
    return value;
}

function token(value: number): string {
    return value.toFixed(12).replace(/(?:\.0+|(?:(\.\d*?)0+))$/, "$1");
}

function makeSnapshot(
    leagueKey: string,
    leagueName: string,
    prices: Record<string, number>,
    sources?: EconomySnapshot["sources"],
): EconomySnapshot {
    const contentValue: Record<string, unknown> = {
        schema_version: 1,
        league_key: leagueKey,
        game_data_hash: "game-hash",
        prices: Object.fromEntries(
            Object.entries(prices)
                .sort(([left], [right]) => left.localeCompare(right))
                .map(([key, value]) => [key, token(value)]),
        ),
        missing_keys: ["base"],
        low_confidence_keys: [],
    };
    if (sources) contentValue.sources = sources;
    const content = canonical(contentValue);
    const hash = createHash("sha256").update(JSON.stringify(content)).digest("hex");
    return {
        version: "v1",
        id: `economy:${leagueKey}:${hash}`,
        metadata: {
            schema_version: 1,
            game: "poe1",
            realm: "pc",
            league_key: leagueKey,
            league_name: leagueName,
            source: "poe-ninja",
            source_cutoff_at_utc: "2026-07-15T00:00:00Z",
            created_at_utc: "2026-07-15T00:00:00Z",
            game_data_hash: "game-hash",
            price_count: Object.keys(prices).length,
            missing_keys: ["base"],
            low_confidence_keys: [],
            content_sha256: hash,
        },
        prices,
        ...(sources ? { sources } : {}),
    };
}

function entry(
    snapshot: EconomySnapshot,
    options: Partial<LeagueIndexEntry> = {},
): LeagueIndexEntry {
    return {
        league_key: snapshot.metadata.league_key,
        display_name: snapshot.metadata.league_name,
        realm: "pc",
        ruleset: "softcore",
        temporary: true,
        active: true,
        archived: false,
        latest_snapshot_id: snapshot.id,
        latest_snapshot_url: `snapshots/${snapshot.metadata.content_sha256}.json`,
        content_sha256: snapshot.metadata.content_sha256,
        source_cutoff_at_utc: snapshot.metadata.source_cutoff_at_utc,
        stale: false,
        error: null,
        coverage: { priced: Object.keys(snapshot.prices).length, missing: 1, low_confidence: 0 },
        ...options,
    };
}

function fixture() {
    const mirage = makeSnapshot(
        "mirage",
        "Mirage",
        { chaos: 1, alteration: 0.18, "beast:rare": 1 },
        { chaos: "quote", alteration: "quote", "beast:rare": "owner_default" },
    );
    const hardcore = makeSnapshot("hardcore-mirage", "Hardcore Mirage", {
        chaos: 1,
        alteration: 0.28,
    });
    const standard = makeSnapshot("standard", "Standard", {
        chaos: 1,
        alteration: 0.1,
    });
    const bad = makeSnapshot("bad-league", "Bad League", { chaos: 1 });
    const index: LeagueIndex = {
        schema_version: 1,
        generated_at_utc: "2026-07-15T00:00:00Z",
        leagues: [
            entry(mirage),
            entry(hardcore, { ruleset: "hardcore" }),
            entry(standard, { temporary: false }),
            entry(bad, { active: false, archived: true }),
        ],
    };
    const routes = new Map<string, unknown>([
        ["https://economy.test/league-index.json", index],
        ...[mirage, hardcore, standard].map(
            (snapshot) => [
                `https://economy.test/snapshots/${snapshot.metadata.content_sha256}.json`,
                snapshot,
            ] as const,
        ),
        [
            `https://economy.test/snapshots/${bad.metadata.content_sha256}.json`,
            { ...bad, prices: { chaos: 999 } },
        ],
    ]);
    const fetcher = async (input: string | URL): Promise<Response> => {
        const value = routes.get(String(input));
        return value
            ? new Response(JSON.stringify(value), {
                  status: 200,
                  headers: { "content-type": "application/json" },
              })
            : new Response("missing", { status: 404 });
    };
    return { mirage, hardcore, standard, bad, index, fetcher };
}

test("first startup selects current softcore and migrates legacy prices only to manual", async () => {
    const data = fixture();
    const storage = new TestStorage();
    storage.setItem("poecraft.prices", JSON.stringify({ base: 12, chaos: -1 }));
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: data.fetcher,
        storage,
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    assert.equal(service.getState().selectedProfile, "mirage");
    assert.equal(service.getState().status, "fresh");
    assert.equal(service.getPrice("base"), undefined);
    assert.equal(storage.getItem("poecraft.prices"), null);
    await service.selectProfile("manual");
    assert.equal(service.getPrice("base"), 12);
});

test("league overrides remain separate and pinned work keeps its original identity", async () => {
    const data = fixture();
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: data.fetcher,
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    service.setPrice("alteration", 2);
    const pinned = service.pin();
    await service.selectProfile("hardcore-mirage");
    assert.equal(service.getPrice("alteration"), 0.28);
    assert.notEqual(pinned.snapshot.id, service.pin().snapshot.id);
    service.setPrice("alteration", 3);
    await service.selectProfile("mirage");
    assert.equal(service.getPrice("alteration"), 2);
    assert.equal(pinned.profile, "mirage");
    assert.equal(pinned.snapshot.prices.alteration, 2);
});

test("engine-key fallback is non-zero, lower priority than overrides, and pinned", async () => {
    const data = fixture();
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: data.fetcher,
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    service.setFallbackPrice(7);
    assert.deepEqual(service.resolveActionPrice("unquoted-real-action"), {
        value: 7,
        source: "fallback",
    });
    assert.deepEqual(service.resolveActionPrice("alteration"), {
        value: 0.18,
        source: "quote",
    });
    service.setPrice("unquoted-real-action", 2);
    assert.deepEqual(service.resolveActionPrice("unquoted-real-action"), {
        value: 2,
        source: "override",
    });
    service.setPrice("unquoted-real-action", null);
    const pinned = service.pin(["alteration", "unquoted-real-action"]);
    assert.equal(pinned.snapshot.prices["unquoted-real-action"], 7);
    assert.equal(pinned.identity.fallback_price, 7);
    assert.equal(
        pinned.identity.price_sources?.["unquoted-real-action"],
        "fallback",
    );
    service.setFallbackPrice(0);
    assert.equal(
        service.resolveActionPrice("unquoted-real-action").value,
        undefined,
    );
});

test("owner defaults retain provenance and clear overrides back to the default", async () => {
    const data = fixture();
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: data.fetcher,
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    assert.deepEqual(service.resolveActionPrice("beast:rare"), {
        value: 1,
        source: "owner_default",
    });
    assert.equal(
        service.pin(["beast:rare"]).identity.price_sources?.["beast:rare"],
        "owner_default",
    );
    service.setPrice("beast:rare", 2);
    assert.deepEqual(service.resolveActionPrice("beast:rare"), {
        value: 2,
        source: "override",
    });
    service.setPrice("beast:rare", null);
    assert.deepEqual(service.resolveActionPrice("beast:rare"), {
        value: 1,
        source: "owner_default",
    });
});

test("offline startup uses the last verified cached snapshot", async () => {
    const data = fixture();
    const storage = new TestStorage();
    const cache = new MemoryEconomyCache();
    const online = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: data.fetcher,
        storage,
        cache,
        broadcast: false,
    });
    await online.initialize();

    const offline = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: async () => {
            throw new Error("offline");
        },
        storage,
        cache,
        broadcast: false,
    });
    await offline.initialize();
    assert.equal(offline.getState().selectedProfile, "mirage");
    assert.equal(offline.getState().status, "offline");
    assert.equal(offline.getPrice("alteration"), 0.18);
});

test("offline first startup without a cache stays explicitly manual-only", async () => {
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: async () => {
            throw new Error("offline");
        },
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    assert.equal(service.getState().selectedProfile, "manual");
    assert.equal(service.getState().status, "manual-only");
    assert.match(service.getState().error ?? "", /offline/);
});

test("hash mismatch rejects a switch and preserves the active league", async () => {
    const data = fixture();
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: data.fetcher,
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    await assert.rejects(
        () => service.selectProfile("bad-league"),
        /content hash mismatch/,
    );
    assert.equal(service.getState().selectedProfile, "mirage");
    assert.equal(service.getPrice("alteration"), 0.18);
});

test("refresh failure retains the active verified snapshot", async () => {
    const data = fixture();
    let online = true;
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: async (input) => {
            if (!online) throw new Error("provider unavailable");
            return data.fetcher(input);
        },
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await service.initialize();
    online = false;
    await assert.rejects(() => service.refresh(), /provider unavailable/);
    assert.equal(service.getState().selectedProfile, "mirage");
    assert.equal(service.getPrice("alteration"), 0.18);
});

test("non-JSON index responses surface a concise error", async () => {
    const service = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: async () =>
            new Response("<!doctype html>", {
                status: 200,
                headers: { "content-type": "text/html" },
            }),
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await assert.rejects(() => service.refresh(), /response was not JSON/);
});

test("browser verification accepts the Python canonical hash fixture", async () => {
    const snapshot = JSON.parse(
        readFileSync(
            new URL("../../../fixtures/economy/runtime-snapshot-v1.json", import.meta.url),
            "utf8",
        ),
    ) as EconomySnapshot;
    await verifyEconomySnapshot(snapshot, snapshot.metadata.content_sha256!);
});

test("checked-in publication defaults to Allflame and preserves archived Mirage selection", async () => {
    const economyRoot = new URL("../public/economy/", import.meta.url);
    const index = JSON.parse(
        readFileSync(new URL("league-index.json", economyRoot), "utf8"),
    ) as LeagueIndex;
    const fetcher = async (input: string | URL): Promise<Response> => {
        const url = new URL(String(input));
        const file = url.pathname.endsWith("/league-index.json")
            ? new URL("league-index.json", economyRoot)
            : new URL(`snapshots/${url.pathname.split("/").at(-1)}`, economyRoot);
        try {
            return new Response(readFileSync(file, "utf8"), {
                status: 200,
                headers: { "content-type": "application/json" },
            });
        } catch {
            return new Response("missing", { status: 404 });
        }
    };

    const fresh = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: fetcher,
        storage: new TestStorage(),
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await fresh.initialize();
    assert.equal(fresh.getState().selectedProfile, "allflame");
    const pinned = fresh.pin(["chaos", "fracture"]);
    assert.equal(
        pinned.sourceSnapshotId,
        "economy:allflame:de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0",
    );
    assert.equal(pinned.snapshot.prices.chaos, 1);
    assert.equal(pinned.snapshot.prices.fracture, 405.1);
    assert.equal(pinned.snapshot.prices["beast:craicic-croaker"], 66);
    assert.equal(pinned.snapshot.sources?.["beast:craicic-croaker"], "quote");
    assert.equal(pinned.snapshot.prices["beast:rare"], 1);
    assert.equal(pinned.snapshot.sources?.["beast:rare"], "owner_default");

    const historicalStorage = new TestStorage();
    historicalStorage.setItem("poecraft.economy.selected.v1", "mirage");
    const historical = new EconomyService({
        indexUrl: "https://economy.test/league-index.json",
        fetch: fetcher,
        storage: historicalStorage,
        cache: new MemoryEconomyCache(),
        broadcast: false,
    });
    await historical.initialize();
    assert.equal(historical.getState().selectedProfile, "mirage");
    assert.equal(
        historical.pin().sourceSnapshotId,
        "economy:mirage:9175d37d83d90ab936e572f04c7599afbf18ff6cefc90786a5276da1759cd52f",
    );

    assert.ok(
        index.leagues.some(
            (league) =>
                league.league_key === "mirage" &&
                !league.active &&
                league.archived,
        ),
    );
});
