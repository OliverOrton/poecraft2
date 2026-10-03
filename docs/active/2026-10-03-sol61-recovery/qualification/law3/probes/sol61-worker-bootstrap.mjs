// Isolated qualification instrumentation; no production worker source change.
import { register } from "tsx/esm/api";
import { parentPort } from "node:worker_threads";
register();
globalThis.WorkerGlobalScope = Object;
const { EngineBindings } = await import("../src/app/engine-wasm.ts");
let engine, initial = 0, peak = 0, rssPeakObserved = 0;
const originalAbi = EngineBindings.prototype.abiVersion;
EngineBindings.prototype.abiVersion = function (...args) {
    engine = this;
    initial ||= engine.module.HEAPU8.buffer.byteLength;
    return originalAbi.apply(this, args);
};
const originalPost = parentPort.postMessage.bind(parentPort);
parentPort.postMessage = function (message, ...args) {
    if (engine && message && typeof message === "object") {
        const bytes = engine.module.HEAPU8.buffer.byteLength;
        peak = Math.max(peak, bytes);
        const rss = process.memoryUsage().rss;
        rssPeakObserved = Math.max(rssPeakObserved, rss);
        message.__sol61_memory = {
            initial_linear_memory_bytes: initial, current_linear_memory_bytes: bytes,
            peak_observed_linear_memory_bytes: peak,
            linear_memory_growth_only: true, configured_maximum_linear_memory_bytes: 4294967296,
            process_rss_bytes: rss, process_rss_peak_observed_bytes: rssPeakObserved,
            process_lifetime_max_rss_bytes: process.resourceUsage().maxRSS * 1024,
            rss_scope: "fresh Node process, including worker; observations at product response boundaries",
        };
    }
    return originalPost(message, ...args);
};
await import("../src/app/engine-worker.ts");
