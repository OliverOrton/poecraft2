import assert from "node:assert/strict";
import vm from "node:vm";
import { build } from "esbuild";
import { EngineClient } from "../src/app/engine-client";
import type { ClientMessage, WorkerMessage } from "../src/app/engine-protocol";

// Run the real worker dispatch/solve loop with only its native boundary mocked.
// Native artifact eligibility and costs are qualified in native/WASM fixtures.
const bundle = await build({entryPoints: ["src/app/engine-worker.ts"], bundle: true,
    write: false, platform: "browser", format: "iife", external: ["node:worker_threads"],
    plugins: [{name: "native-fixture", setup(b) {
        b.onResolve({filter: /^\.\/engine-wasm$/}, () => ({path: "native", namespace: "fixture"}));
        b.onLoad({filter: /.*/, namespace: "fixture"}, () => ({contents:
            "export async function createEngineBindings(){return globalThis.fixtureBindings;}"}));
    }}]});

for (const mode of ["manual", "manual_sparse", "missing", "automatic", "exact", "cancel", "cancel_done",
    "adaptive_eight", "fixed_eight", "legacy_json_eight"] as const) {
    let steps = 0, finishes = 0, abandoned = 0, exported = 0, inHandler = false;
    const quanta: number[] = [];
    let clock = 0;
    let lastProgress: any;
    const runStep = (_solver: number, quantum: number): any => {
        ++steps;
        quanta.push(quantum);
        clock += 10;
        if (mode === "manual_sparse" && steps === 3)
            setTimeout(() => send({kind: "finish", id: 7}), 0);
        if (mode === "cancel_done" && steps === 2)
            setTimeout(() => send({kind: "cancel", id: 7}), 0);
        lastProgress = {phase: "expanding", phase_owner: "ladder_scheduling", lifecycle_sequence: steps,
            expanded_states: steps, done: finishes > 0 || steps === 4 || (mode === "cancel_done" && steps === 2)};
        return lastProgress;
    };
    const messages: WorkerMessage[] = [];
    let complete!: () => void;
    const terminal = new Promise<void>(r => { complete = r; });
    let scope: Record<string, any>;
    const send = (message: ClientMessage): void => {
        inHandler = true; scope.onmessage({data: message}); inHandler = false;
    };
    scope = {
        performance: {now: () => clock}, setTimeout, clearTimeout, TextDecoder, TextEncoder,
        // Model a MessageChannel queue that can outrun an inbound timer-based
        // control. Explicit yieldEveryStep must still service that control,
        // including cancellation queued by the last native work unit.
        MessageChannel: mode === "manual_sparse" ? undefined : class {
            port1: {onmessage?: () => void} = {};
            port2 = {postMessage: () => queueMicrotask(() => this.port1.onmessage?.())};
        },
        fixtureBindings: {
            abiVersion: () => 2,
            beginSolverSolve: () => {},
            stepSolverSolve: runStep,
            stepSolverSolveCompact: (solver: number, quantum: number) => {
                const progress = runStep(solver, quantum);
                return 1 + (7 << 3) + (progress.done ? 128 : 0);
            },
            solverCachedProgress: () => lastProgress,
            solverProgressTrace: () => ({sequence: steps, current: {verified_artifact_available: steps >= 2}, events: []}),
            requestSolverSolveBoundedFinish: () => { assert.equal(inHandler, false); ++finishes; },
            finishSolverSolve: () => { ++exported; return {policy_available: true}; },
            abandonSolverSolve: () => { ++abandoned; },
        },
        postMessage(message: WorkerMessage) {
            messages.push(message);
            if (message.kind === "ready") return;
            if (message.kind === "response") { complete(); return; }
            if (message.kind === "progress" && message.solve?.lifecycle_sequence === 2) {
                if (mode === "manual") {
                    send({kind: "finish", id: 7}); send({kind: "finish", id: 7});
                }
                if (mode === "cancel") {
                    send({kind: "finish", id: 7}); send({kind: "cancel", id: 7});
                }
            }
        },
    };
    vm.runInNewContext(bundle.outputFiles[0].text, scope);
    await new Promise(r => setTimeout(r, 0));
    send({kind: "finish", id: 6}); send({kind: "cancel", id: 6}); // stale identity
    send({kind: "request", id: 7, method: "solverSolve", params: {
        solver: 1, item: 2, economy: 3,
        chunkSize: mode === "manual_sparse" || mode === "adaptive_eight" || mode === "fixed_eight" || mode === "legacy_json_eight" ? 8 : 1,
        diagnosticWorkPolicy: mode === "fixed_eight" ? "fixed_eight" : undefined,
        diagnosticStepTransport: mode === "legacy_json_eight" ? "legacy_json" : undefined,
        yieldEveryStep: mode !== "manual_sparse" && mode !== "adaptive_eight" && mode !== "fixed_eight" && mode !== "legacy_json_eight",
        boundedFinishAfterMs: mode === "automatic" ? 0.000001 : undefined,
    }});
    if (mode === "missing") send({kind: "finish", id: 7});
    await terminal;
    const response = messages.find(m => m.kind === "response") as any;
    assert.equal(messages.filter(m => m.kind === "response").length, 1);
    const cancelled = mode === "cancel" || mode === "cancel_done";
    assert.equal(response.result.cancelled, cancelled);
    assert.equal(abandoned, cancelled ? 1 : 0);
    assert.equal(exported, cancelled ? 0 : 1);
    assert.equal(finishes, mode === "manual" || mode === "manual_sparse" || mode === "automatic" ? 1 : 0);
    send({kind: "finish", id: 7}); send({kind: "cancel", id: 7});
    assert.equal(finishes, mode === "manual" || mode === "manual_sparse" || mode === "automatic" ? 1 : 0);
    assert.equal(response.result.worker.milestones.at(-1).stage, "terminal_response_committed");
    if (mode === "adaptive_eight") {
        assert.deepEqual(quanta, [4, 5, 6, 7]);
        assert.equal(response.result.worker.work_policy, "adaptive");
        assert.equal(response.result.worker.step_transport, "compact");
    }
    if (mode === "fixed_eight") {
        assert.deepEqual(quanta, [8, 8, 8, 8]);
        assert.equal(response.result.worker.work_policy, "fixed_eight");
        assert.equal(response.result.worker.step_transport, "compact");
        assert.equal(response.result.worker.requested_quantum_histogram["8"], 4);
    }
    if (mode === "legacy_json_eight") {
        assert.deepEqual(quanta, [4, 5, 6, 7]);
        assert.equal(response.result.worker.step_transport, "json");
    }
    if (cancelled) assert.ok(response.result.worker.milestones.some((x: any) => x.stage === "abandon_cleanup_completed"));
}

// The client closure follows the invocation ID, even when a handle is reused.
let receive!: (message: WorkerMessage) => void;
const sent: ClientMessage[] = [];
const client = new EngineClient({postMessage: m => { sent.push(m); }, onMessage: h => { receive = h; }});
receive({kind: "ready", abiVersion: 2});
let finish!: () => void;
const first = client.solverSolve(9, 1, 2, {}, {onControl: c => { finish = c.requestFinish; }});
await Promise.resolve(); finish(); finish();
const request = sent[0]; assert.equal(request.kind, "request");
receive({kind: "response", id: request.id, ok: true, result: {cancelled: false}});
await first;
const second = client.solverSolve(9, 1, 2);
await Promise.resolve(); finish();
assert.equal(sent.filter(m => m.kind === "finish").length, 1);
receive({kind: "response", id: sent.at(-1)!.id, ok: true, result: {cancelled: false}});
await second;
console.log("  ok - actual worker Finish/Cancel dispatch, stale IDs, one terminal, release and client handle reuse");
