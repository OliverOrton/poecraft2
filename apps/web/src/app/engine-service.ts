/*
 * Process-wide engine service. Spawns the single WASM worker, loads the
 * compiled data bundle once, and hands the shared EngineClient (plus the loaded
 * data handle) to UI components. Components never construct EngineClient
 * directly so there is exactly one worker and one loaded dataset per tab.
 */

import { EngineClient } from "./engine-client";

export interface Engine {
    client: EngineClient;
    dataId: number;
    summary: Record<string, unknown>;
}

const DATA_URL = "/poecraft-data.json";

let enginePromise: Promise<Engine> | null = null;

async function boot(): Promise<Engine> {
    const client = EngineClient.spawn();
    await client.whenReady();
    const response = await fetch(DATA_URL);
    if (!response.ok) {
        throw new Error(
            `failed to fetch engine data (${response.status}); run "npm run build:data"`,
        );
    }
    const bytes = new Uint8Array(await response.arrayBuffer());
    const dataId = await client.loadData(bytes);
    const summary = await client.dataSummary(dataId);
    return { client, dataId, summary };
}

export function getEngine(): Promise<Engine> {
    if (!enginePromise) {
        enginePromise = boot();
    }
    return enginePromise;
}
