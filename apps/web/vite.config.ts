import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, "..", "..");

export default defineConfig({
    // The engine worker imports the generated WASM module from
    // bindings/wasm/dist, which sits outside the app root; allow the dev server
    // to read from the repo root so that import resolves.
    server: {
        fs: { allow: [repoRoot] },
    },
    worker: {
        format: "es",
    },
    build: {
        target: "es2022",
    },
});
