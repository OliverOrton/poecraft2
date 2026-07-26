import assert from "node:assert/strict";

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
