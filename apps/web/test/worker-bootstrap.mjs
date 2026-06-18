// Test-only worker bootstrap: register tsx's ESM loader in this worker thread
// so it can import the TypeScript engine worker. Production (Vite) compiles the
// worker ahead of time and does not need this.
import { register } from "tsx/esm/api";

register();
await import("../src/app/engine-worker.ts");
