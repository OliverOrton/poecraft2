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

// Force the cross-thread ordering without relying on worker/main scheduling:
// a progress listener aborts before an already-computed terminal reply is read.
for (const mode of ["cancel_before_success", "ordinary_success", "native_error"] as const) {
    let receiveRace!: (message: WorkerMessage) => void;
    const sent: ClientMessage[] = [];
    const raceClient = new EngineClient({
        postMessage: (message) => { sent.push(message); },
        onMessage: (handler) => { receiveRace = handler; },
    });
    receiveRace({kind: "ready", abiVersion: 2});
    const controller = new AbortController();
    const evaluation = raceClient.strategyEvaluate(3, document, undefined, {
        signal: controller.signal,
        onProgress: () => {
            if (mode !== "ordinary_success") controller.abort();
        },
    });
    await Promise.resolve();
    const request = sent[0];
    assert.ok(request?.kind === "request");
    assert.equal(request.method, "strategyEvaluate");
    receiveRace({kind: "progress", id: request.id, done: 1, total: 32,
        evaluation: {phase: "discovery", done: false, discovered_pairs: 1,
            pending_pairs: 31, solved_sccs: 0, total_sccs: 0,
            fallback_sweeps: 0, residual: 0}});
    assert.equal(controller.signal.aborted, mode !== "ordinary_success");
    assert.deepEqual(sent.filter(message => message.kind === "cancel"),
        mode === "ordinary_success" ? [] : [{kind: "cancel", id: request.id}]);
    const completion = mode === "ordinary_success"
        ? evaluation.then(result => assert.equal(
            (result as unknown as {marker: string}).marker, "computed"))
        : assert.rejects(evaluation, (error: unknown) => {
            assert.ok(error instanceof EngineError);
            assert.equal(error.code, mode === "native_error" ? 8 : 1);
            assert.equal(error.detail, mode === "native_error"
                ? "max_transitions" : "strategy evaluation cancelled");
            return true;
        });
    if (mode === "native_error") {
        receiveRace({kind: "response", id: request.id, ok: false,
            error: {code: 8, detail: "max_transitions"}});
    } else {
        receiveRace({kind: "response", id: request.id, ok: true,
            result: {marker: "computed"}});
    }
    await completion;
    // After completion the listener is removed; late abort cannot cancel a
    // settled success or send control for a reused/future invocation.
    const cancelCount = sent.filter(message => message.kind === "cancel").length;
    controller.abort();
    assert.equal(sent.filter(message => message.kind === "cancel").length, cancelCount);
    raceClient.dispose();
}
console.log("  ok - evaluation abort discards a queued success; ordinary success, native errors and settled cleanup remain intact");
