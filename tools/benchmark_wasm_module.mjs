import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { performance } from "node:perf_hooks";

const [
    moduleArg,
    artifactArg,
    runsArg = "100000",
    actionsArg = "10",
    actionType = "chaos",
] =
    process.argv.slice(2);
if (!moduleArg || !artifactArg) {
    throw new Error(
        "usage: node tools/benchmark_wasm_module.mjs <engine.mjs> <artifact-dir> [runs] [actions-per-run] [action-type]",
    );
}
const runs = Number(runsArg);
const actionsPerRun = Number(actionsArg);
const modulePath = resolve(moduleArg);
const artifactDir = resolve(artifactArg);
const { default: createEngine } = await import(pathToFileURL(modulePath).href);
const engine = await createEngine();

function call(name, argTypes, args) {
    const response = JSON.parse(
        engine.ccall(name, "string", argTypes, args),
    );
    if (!response.ok) {
        throw new Error(`${name}: ${response.code} ${response.message}`);
    }
    return response;
}

const manifest = readFileSync(resolve(artifactDir, "manifest.json"), "utf8");
const strings = readFileSync(resolve(artifactDir, "strings.json"), "utf8");
const gameData = readFileSync(resolve(artifactDir, "game-data.json"), "utf8");
const bundle = new TextEncoder().encode(
    `{"manifest":${manifest},"strings":${strings},"game_data":${gameData}}`,
);
const ptr = engine._malloc(bundle.length);
engine.HEAPU8.set(bundle, ptr);
const data = call(
    "pcw_data_open",
    ["number", "number"],
    [ptr, bundle.length],
).data;
engine._free(ptr);

const session = call("pcw_session_open", ["number", "string"], [
    data,
    JSON.stringify({
        base: "Metadata/Items/Armours/BodyArmours/BodyInt17",
        item_level: 86,
    }),
]).session;
const actionNodes = Array.from({ length: actionsPerRun }, (_, index) => ({
    id: `${actionType}_${index + 1}`,
    kind: "operation",
    operation: { type: actionType, params: {} },
}));
const strategySpec = {
    version: "v1",
    name: "Direct WASM throughput benchmark",
    start_node_id: "start",
    base_state: {
        base_key: "Metadata/Items/Armours/BodyArmours/BodyInt17",
        item_level: 86,
        rarity: actionType === "alteration" ? "magic" : "rare",
    },
    nodes: [
        { id: "start", kind: "start" },
        ...actionNodes,
        { id: "success", kind: "terminal", terminal: "success" },
    ],
    edges: [
        {
            id: "begin",
            from: "start",
            to: actionNodes[0].id,
            priority: 0,
            condition: { type: "always" },
        },
        ...actionNodes.map((node, index) => ({
            id: `step_${index + 1}`,
            from: node.id,
            to: actionNodes[index + 1]?.id ?? "success",
            priority: 0,
            condition: { type: "always" },
        })),
    ],
};
const strategy = call("pcw_strategy_compile", ["number", "string"], [
    session,
    JSON.stringify(strategySpec),
]).strategy;
const simulator = call(
    "pcw_simulator_open",
    ["number", "number", "number"],
    [session, strategy, 0],
).simulator;
const options = {
    target_runs: runs,
    seed: 31,
    retained_trace_count: 0,
    retained_success_count: 0,
    retained_failure_count: 0,
};

const started = performance.now();
let progress;
do {
    progress = call(
        "pcw_simulator_run_chunk",
        ["number", "string", "number"],
        [simulator, JSON.stringify(options), 5_000],
    ).progress;
} while (!progress.finished);
const elapsedMs = performance.now() - started;
const result = call("pcw_simulator_result", ["number"], [simulator]);
const totalActions = Number(result.summary.total_actions);
console.log(
    JSON.stringify(
        {
            engine_module: modulePath,
            runs,
            actions_per_run: actionsPerRun,
            action_type: actionType,
            total_actions: totalActions,
            elapsed_ms: Number(elapsedMs.toFixed(3)),
            actions_per_second: Math.round(
                (totalActions * 1000) / elapsedMs,
            ),
        },
        null,
        2,
    ),
);

engine.ccall("pcw_simulator_close", null, ["number"], [simulator]);
engine.ccall("pcw_strategy_close", null, ["number"], [strategy]);
engine.ccall("pcw_session_close", null, ["number"], [session]);
engine.ccall("pcw_data_close", null, ["number"], [data]);
