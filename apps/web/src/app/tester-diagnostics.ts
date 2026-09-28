import build from '../generated/build-info.json';

export const runtimeDiagnostics: { status: string; abi_version: number | null; error: string | null } = {
    status: 'not-loaded', abi_version: null, error: null,
};
const runs = new Map<string, { feature: string; read: () => unknown }>();
export function beginDiagnosticRun(feature: string, request: unknown) {
    const record = { request: structuredClone(request), status: 'running', result: null as unknown, error: null as string | null };
    retainDiagnosticRun(crypto.randomUUID(), feature, () => record);
    return record;
}
/** Request owners retain their frozen inputs; export never reads a live form or price selector. */
export function retainDiagnosticRun(id: string, feature: string, read: () => unknown): void {
    runs.set(id, { feature, read });
    if (runs.size > 5) runs.delete(runs.keys().next().value!);
}
export function diagnostics(): unknown {
    return {
        schema: 'poecraft_tester_diagnostics_v1', application: build,
        loaded_runtime: { ...runtimeDiagnostics },
        economy_delivery: (globalThis as typeof globalThis & { POECRAFT_ECONOMY_INDEX_URL?: string }).POECRAFT_ECONOMY_INDEX_URL
            ? { mode: 'live', index_url: (globalThis as typeof globalThis & { POECRAFT_ECONOMY_INDEX_URL?: string }).POECRAFT_ECONOMY_INDEX_URL }
            : build.economy,
        browser: typeof navigator === 'undefined' ? null : navigator.userAgent,
        runs: [...runs].map(([id, run]) => ({ id, feature: run.feature, record: structuredClone(run.read()) })),
        limitations: ['Only the five most recent recorded operations in this tab are included.',
            'Emulator history, other documents, and unrelated browser storage are not collected.',
            'For unrecorded operations attach the relevant item or strategy export; no reproduction is inferred.'],
    };
}

export function downloadDiagnostics(): void {
    const url = URL.createObjectURL(new Blob([JSON.stringify(diagnostics(), null, 2)], { type: 'application/json' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = `poecraft-diagnostics-${build.build_id.slice(0, 12)}.json`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
}
