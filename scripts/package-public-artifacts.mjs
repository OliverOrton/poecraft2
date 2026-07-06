import {
    copyFileSync,
    existsSync,
    mkdirSync,
    readFileSync,
    readdirSync,
    rmSync,
    statSync,
    writeFileSync,
} from "node:fs";
import { createHash } from "node:crypto";
import { basename, dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const outputRoot = join(root, "dist", "public-artifacts");
const dataDir = join(root, "data", "compiled", "current");
const webDist = join(root, "apps", "web", "dist");
const economyFile = join(root, "data", "economy", "public-none.json");

function sha256(path) {
    return createHash("sha256").update(readFileSync(path)).digest("hex");
}

function filesBelow(path) {
    if (!existsSync(path)) return [];
    if (statSync(path).isFile()) return [path];
    return readdirSync(path, { withFileTypes: true })
        .flatMap((entry) => filesBelow(join(path, entry.name)))
        .sort();
}

function firstExisting(paths) {
    return paths.find((path) => existsSync(path));
}

const nativeEngine = firstExisting([
    join(root, "build", "engine", "Release", "poecraft_engine.dll"),
    join(root, "build", "engine", "poecraft_engine.dll"),
]);
const required = [
    nativeEngine,
    join(root, "bindings", "wasm", "dist", "poecraft_engine.mjs"),
    join(root, "bindings", "wasm", "dist", "poecraft_engine.wasm"),
    join(dataDir, "manifest.json"),
    join(dataDir, "game-data.json"),
    join(dataDir, "strings.json"),
    economyFile,
];
if (required.some((path) => !path || !existsSync(path))) {
    throw new Error(
        "Public packaging requires native/WASM builds, compiled data, and the economy snapshot.",
    );
}
if (!existsSync(webDist)) {
    throw new Error("Web production build is absent; run npm run build in apps/web.");
}

const inputs = [
    { kind: "engine-native", source: nativeEngine, target: "engine/native/poecraft_engine.dll" },
    {
        kind: "engine-wasm",
        source: join(root, "bindings", "wasm", "dist", "poecraft_engine.mjs"),
        target: "engine/wasm/poecraft_engine.mjs",
    },
    {
        kind: "engine-wasm",
        source: join(root, "bindings", "wasm", "dist", "poecraft_engine.wasm"),
        target: "engine/wasm/poecraft_engine.wasm",
    },
    ...filesBelow(dataDir).map((source) => ({
        kind: "game-data",
        source,
        target: `data/${relative(dataDir, source).replaceAll("\\", "/")}`,
    })),
    ...filesBelow(webDist).map((source) => ({
        kind: "ui-cold-data",
        source,
        target: `ui/${relative(webDist, source).replaceAll("\\", "/")}`,
    })),
    {
        kind: "economy",
        source: economyFile,
        target: `economy/${basename(economyFile)}`,
    },
];

const components = inputs
    .map((entry) => ({
        kind: entry.kind,
        path: entry.target,
        bytes: statSync(entry.source).size,
        sha256: sha256(entry.source),
    }))
    .sort((a, b) => a.path.localeCompare(b.path));
const dataManifest = JSON.parse(readFileSync(join(dataDir, "manifest.json"), "utf8"));
const identity = JSON.stringify({
    schema_version: 1,
    abi_version: 1,
    data_hash: dataManifest.source?.data_hash ?? "",
    components,
});
const bundleId = createHash("sha256").update(identity).digest("hex");
const output = join(outputRoot, bundleId);
rmSync(output, { recursive: true, force: true });
mkdirSync(output, { recursive: true });
for (const entry of inputs) {
    const target = join(output, entry.target);
    mkdirSync(dirname(target), { recursive: true });
    copyFileSync(entry.source, target);
}
for (const component of components) {
    const copied = join(output, component.path);
    if (
        statSync(copied).size !== component.bytes ||
        sha256(copied) !== component.sha256
    ) {
        throw new Error(`Packaged artifact failed hash verification: ${component.path}`);
    }
}

const manifest = {
    schema_version: 1,
    bundle_id: bundleId,
    generated_at_utc: dataManifest.generated_at_utc,
    engine: {
        abi_version: 1,
        python_package_version: "0.1.0",
    },
    game_data: {
        artifact_schema_version: dataManifest.artifact_schema_version,
        source_version: dataManifest.source?.source_version,
        source_hash: dataManifest.source?.source_hash,
        data_hash: dataManifest.source?.data_hash,
    },
    economy: {
        snapshot_id: "public-none",
        cost_status: "incomplete",
    },
    components,
};
writeFileSync(
    join(output, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
);
mkdirSync(outputRoot, { recursive: true });
writeFileSync(
    join(outputRoot, "latest.json"),
    `${JSON.stringify(
        {
            schema_version: 1,
            bundle_id: bundleId,
            manifest_sha256: sha256(join(output, "manifest.json")),
        },
        null,
        2,
    )}\n`,
);
console.log(`packaged ${components.length} immutable artifacts as ${bundleId}`);
console.log(output);
