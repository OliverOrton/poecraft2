import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";
import { readFileSync } from "node:fs";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, "..", "..");
const buildInfo = JSON.parse(readFileSync(resolve(here, 'src/generated/build-info.json'), 'utf8'));

export default defineConfig({
    base: buildInfo.base,
    plugins: [{ name: 'tester-build-receipt', generateBundle() {
        this.emitFile({ type: 'asset', fileName: 'build-info.json', source: JSON.stringify(buildInfo, null, 2) + '\n' });
    } }],
    // Browser/Electron shells may expose a partial global process object inside
    // workers. The Emscripten module must still select its web-worker loader in
    // the production bundle; the unbundled Node test module keeps native Node
    // detection.
    define: {
        "globalThis.process": "undefined",
    },
    // The engine worker imports the generated WASM module from
    // bindings/wasm/dist, which sits outside the app root; allow the dev server
    // to read from the repo root so that import resolves.
    server: {
        // Honour PORT (set by the preview tooling); default to Vite's usual 5173.
        port: process.env.PORT ? Number(process.env.PORT) : 5173,
        fs: { allow: [repoRoot] },
    },
    worker: {
        format: "es",
    },
    build: {
        target: "es2022",
    },
});
