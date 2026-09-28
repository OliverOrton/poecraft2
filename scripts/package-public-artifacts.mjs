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
import { selectedRuntime, verifiedGameAssets, hash } from './build-data-bundle.mjs';

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

// The website has no native DLL or canonical database. Keep legacy packaging separate.
function webComponents(directory) {
    return filesBelow(directory).filter(path => basename(path) !== 'deployment-manifest.json').map(path => {
        const name = relative(directory, path).replaceAll('\\', '/');
        if (!/^(index\.html|build-info\.json|THIRD_PARTY_NOTICES\.txt|poecraft-data\.[a-f0-9]{64}\.json|assets\/[\w.-]+|game-assets\/(catalog\.json|[a-f0-9]{64}\.png)|economy\/league-index\.json|economy\/snapshots\/[a-f0-9]{64}\.json)$/.test(name)) {
            throw new Error(`Unexpected file in static deployment: ${name}`);
        }
        return { kind: 'web', path: name, bytes: statSync(path).size, sha256: sha256(path) };
    }).sort((a, b) => a.path.localeCompare(b.path));
}

function verifyWeb(directory) {
    const receipt = JSON.parse(readFileSync(join(directory, 'deployment-manifest.json')));
    const components = webComponents(directory);
    if (JSON.stringify(components) !== JSON.stringify(receipt.components) || hash(JSON.stringify({ build: receipt.build, components })) !== receipt.bundle_id) {
        throw new Error('Deployment manifest integrity mismatch');
    }
    const build = JSON.parse(readFileSync(join(directory, 'build-info.json')));
    if (JSON.stringify(build) !== JSON.stringify(receipt.build)) throw new Error('Deployment build identity mismatch');
    const { build_id, ...inputs } = build;
    if (hash(JSON.stringify(inputs)) !== build_id) throw new Error('Build input identity mismatch');
    const data = components.find(c => c.path === build.runtime.url);
    if (data?.sha256 !== build.runtime.bundle_sha256 || data?.bytes !== build.runtime.bundle_bytes) throw new Error('Runtime bundle identity mismatch');
    if (components.find(c => c.path === 'economy/league-index.json')?.sha256 !== build.economy.bundled_index_sha256) throw new Error('Bundled economy index differs from build identity');
    if (build.game_assets && components.find(c => c.path === 'game-assets/catalog.json')?.sha256 !== build.game_assets.catalog_sha256) throw new Error('Artwork catalogue differs from build identity');
    if (build.game_assets) verifiedGameAssets(join(directory, 'game-assets'), build.runtime.manifest_sha256, build.runtime.source.data_hash);
    for (const required of ['index.html', 'THIRD_PARTY_NOTICES.txt']) {
        if (!components.some(c => c.path === required)) throw new Error(`Required static file missing: ${required}`);
    }
    if (!components.some(c => c.path.endsWith('.wasm') && c.sha256 === build.engine.wasm_sha256)) throw new Error('Emitted WASM differs from selected engine');
    console.log(`Verified web archive ${receipt.bundle_id} (${components.length} files)`);
    return receipt;
}

function packageWeb() {
    const selected = selectedRuntime(root);
    const build = JSON.parse(readFileSync(join(webDist, 'build-info.json')));
    const generated = JSON.parse(readFileSync(join(root, 'apps/web/src/generated/build-info.json')));
    if (JSON.stringify(build) !== JSON.stringify(generated) || build.runtime.manifest_sha256 !== selected.lock.manifest_sha256 || hash(selected.bundle) !== build.runtime.bundle_sha256) throw new Error('Build is stale relative to selected inputs');
    const components = webComponents(webDist);
    const bundleId = hash(JSON.stringify({ build, components }));
    const output = join(outputRoot, 'web', bundleId);
    mkdirSync(output, { recursive: true });
    for (const component of components) {
        const target = join(output, component.path);
        mkdirSync(dirname(target), { recursive: true });
        if (existsSync(target) && sha256(target) !== component.sha256) throw new Error(`Immutable package collision: ${component.path}`);
        copyFileSync(join(webDist, component.path), target);
    }
    const manifestPath = join(output, 'deployment-manifest.json');
    if (!existsSync(manifestPath)) writeFileSync(manifestPath, JSON.stringify({
        schema_version: 1, target: 'web', bundle_id: bundleId, build, components,
        packaged_at_utc: new Date().toISOString(), workflow_revision: process.env.POECRAFT_WORKFLOW_REVISION || null,
    }, null, 2) + '\n');
    verifyWeb(output);
    if (process.env.GITHUB_OUTPUT) writeFileSync(process.env.GITHUB_OUTPUT, `directory=${output}\nbundle_id=${bundleId}\n`, { flag: 'a' });
    console.log(output);
}

if (process.argv.includes('--verify-web')) {
    const directory = process.argv[process.argv.indexOf('--verify-web') + 1];
    if (!directory) throw new Error('--verify-web requires an archive directory');
    verifyWeb(directory);
} else if (process.argv.includes('--web')) {
    packageWeb();
} else {

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
}
