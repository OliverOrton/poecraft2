import assert from "node:assert/strict";
import { EngineError } from "../src/app/engine-protocol";

import {
    EngineClient,
    type EngineTransport,
} from "../src/app/engine-client";
import type {
    ClientMessage,
    WorkerMessage,
} from "../src/app/engine-protocol";
import { createDefaultStrategy } from "../src/app/strategy-model";

const decoder = new TextDecoder();
let receive: ((message: WorkerMessage) => void) | null = null;
const methods: string[] = [];

const transport: EngineTransport = {
    postMessage(message: ClientMessage, transfer?: Transferable[]): void {
        if (message.kind !== "request") return;
        methods.push(message.method);
        const strategyJson = message.params.strategyJson;
        if (
            message.method === "compileStrategy" ||
            message.method === "strategyEvaluate"
        ) {
            assert.ok(
                strategyJson instanceof Uint8Array,
                `${message.method} should send encoded strategy bytes`,
            );
            assert.equal(transfer?.length, 1);
            assert.strictEqual(transfer?.[0], strategyJson.buffer);
            assert.equal(
                JSON.parse(decoder.decode(strategyJson)).version,
                "v1",
            );
        }
        const result =
            message.method === "compileStrategy"
                ? { strategy: 17 }
                : message.method === "strategyEvaluate"
                  ? { marker: "evaluated" }
                  : message.method === "solverCompileStrategy"
                    ? {
                          strategyJson: new TextEncoder().encode(
                              JSON.stringify(createDefaultStrategy()),
                          ),
                      }
                    : {};
        queueMicrotask(() =>
            receive?.({
                kind: "response",
                id: message.id,
                ok: true,
                result,
            }),
        );
    },
    onMessage(handler): void {
        receive = handler;
        queueMicrotask(() =>
            handler({ kind: "ready", abiVersion: 2 }),
        );
    },
};

const client = new EngineClient(transport);
await client.whenReady();

const document = createDefaultStrategy();
assert.equal(await client.compileStrategy(3, document), 17);
assert.equal(
    (await client.strategyEvaluate(3, document) as unknown as { marker: string })
        .marker,
    "evaluated",
);
const transferred = await client.solverCompileStrategy(9);
assert.deepEqual(transferred, createDefaultStrategy());
assert.deepEqual(methods, [
    "compileStrategy",
    "strategyEvaluate",
    "solverCompileStrategy",
]);

client.dispose();
console.log(
    "  ok - strategy graphs cross client/worker boundaries as transferable JSON bytes",
);

// Force cross-thread ordering without relying on worker/main scheduling.
for (const mode of ["cancel_before_dispatch", "cancel_before_success", "ordinary_success",
    "native_error_after_abort", "native_error_before_abort"] as const) {
    let receiveRace!: (message: WorkerMessage) => void;
    const sent: ClientMessage[] = [];
    const raceClient = new EngineClient({
        postMessage: (message) => { sent.push(message); },
        onMessage: (handler) => { receiveRace = handler; },
    });
    const preAbort = mode === "cancel_before_dispatch";
    const nativeError = mode === "native_error_after_abort" || mode === "native_error_before_abort";
    const abortOnProgress = mode === "cancel_before_success" || mode === "native_error_after_abort";
    if (!preAbort) receiveRace({kind: "ready", abiVersion: 2});
    const controller = new AbortController();
    let progressCalls = 0;
    const evaluation = raceClient.strategyEvaluate(3, document, undefined, {
        signal: controller.signal,
        onProgress: () => {
            ++progressCalls;
            if (abortOnProgress) controller.abort();
        },
    });
    if (preAbort) {
        controller.abort();
        await Promise.resolve();
        assert.equal(sent.length, 0, "no request may dispatch before worker readiness");
        receiveRace({kind: "ready", abiVersion: 2});
    }
    await Promise.resolve();
    const request = sent[0];
    assert.ok(request?.kind === "request");
    assert.equal(request.method, "strategyEvaluate");
    assert.equal(request.cancelled, preAbort ? true : undefined);
    const progress: WorkerMessage = {kind: "progress", id: request.id, done: 1, total: 32,
        evaluation: {phase: "discovery", done: false, discovered_pairs: 1,
            pending_pairs: 31, solved_sccs: 0, total_sccs: 0,
            fallback_sweeps: 0, residual: 0}};
    if (!preAbort) receiveRace(progress);
    assert.equal(controller.signal.aborted, preAbort || abortOnProgress);
    assert.deepEqual(sent.filter(message => message.kind === "cancel"),
        preAbort || abortOnProgress ? [{kind: "cancel", id: request.id}] : []);
    const rejected = mode !== "ordinary_success";
    const completion = rejected
        ? assert.rejects(evaluation, (error: unknown) => {
            assert.ok(error instanceof EngineError);
            assert.equal(error.code, nativeError ? 8 : 1);
            assert.equal(error.detail, nativeError ? "max_transitions" : "strategy evaluation cancelled");
            return true;
        })
        : evaluation.then(result => assert.equal(
            (result as unknown as {marker: string}).marker, "computed"));
    const reply: WorkerMessage = nativeError || preAbort
        ? {kind: "response", id: request.id, ok: false,
            error: {code: nativeError ? 8 : 1,
                detail: nativeError ? "max_transitions" : "strategy evaluation cancelled"}}
        : {kind: "response", id: request.id, ok: true, result: {marker: "computed"}};
    receiveRace(reply);
    await completion;
    // Terminal observation removes this invocation before a duplicate reply,
    // stale progress, or a later abort can reach the old callbacks/control.
    const progressBefore = progressCalls;
    const cancelCount = sent.filter(message => message.kind === "cancel").length;
    receiveRace(reply);
    receiveRace(progress);
    controller.abort();
    assert.equal(progressCalls, progressBefore);
    assert.equal(sent.filter(message => message.kind === "cancel").length, cancelCount);
    raceClient.dispose();
}
console.log("  ok - evaluation cancellation ordering, native errors and terminal callback cleanup remain intact");
