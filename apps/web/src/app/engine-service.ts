/*
 * Process-wide engine service. Spawns the single WASM worker, loads the
 * compiled data bundle once, and hands the shared EngineClient (plus the loaded
 * data handle) to UI components. Components never construct EngineClient
 * directly so there is exactly one worker and one loaded dataset per tab.
 */

import { EngineClient } from "./engine-client";
import build from '../generated/build-info.json';
import { publicAsset, verifiedRuntime } from './public-assets';
import { runtimeDiagnostics } from './tester-diagnostics';

export interface Engine {
    client: EngineClient;
    dataId: number;
    summary: Record<string, unknown>;
}

const DATA_URL = publicAsset(build.runtime.url, build.base);

let enginePromise: Promise<Engine> | null = null;

async function boot(): Promise<Engine> {
    const client = EngineClient.spawn();
    try {
        await client.whenReady();
        runtimeDiagnostics.abi_version = client.getAbiVersion();
        if (client.getAbiVersion() !== build.engine.abi_version) throw new Error('Engine ABI differs from the selected build');
        const response = await fetch(DATA_URL);
        const bytes = await verifiedRuntime(response, build.runtime);
        const dataId = await client.loadData(bytes);
        const summary = await client.dataSummary(dataId);
        runtimeDiagnostics.status = 'loaded';
        return { client, dataId, summary };
    } catch (error) {
        client.dispose();
        runtimeDiagnostics.status = 'error';
        runtimeDiagnostics.error = error instanceof Error ? error.message : String(error);
        throw new Error(`${runtimeDiagnostics.error}. Copy diagnostics to report this, then reload to retry the latest build. Saved drafts are kept.`);
    }
}

export function getEngine(): Promise<Engine> {
    if (!enginePromise) {
        enginePromise = boot();
    }
    return enginePromise;
}
