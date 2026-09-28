// Product selection only. Research retains its own pinned artifact.
import { readFileSync, writeFileSync, mkdirSync, readdirSync, unlinkSync, existsSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

export const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
export const hash = bytes => createHash('sha256').update(bytes).digest('hex');
export function selectedRuntime(root = repoRoot) {
    const lock = JSON.parse(readFileSync(join(root, 'apps/web/runtime.lock.json')));
    if (lock.schema_version !== 1 || !/^[a-f0-9]{64}$/.test(lock.manifest_sha256) ||
        lock.runtime_directory !== `data/runtime-snapshots/${lock.manifest_sha256}`) {
        throw new Error('Invalid product runtime selection');
    }
    const directory = join(root, lock.runtime_directory);
    const manifestBytes = readFileSync(join(directory, 'manifest.json'));
    if (hash(manifestBytes) !== lock.manifest_sha256) throw new Error('Selected manifest hash mismatch');
    const manifest = JSON.parse(manifestBytes);
    if (!manifest.engine_loadable || !manifest.complete_dataset || manifest.artifact_schema_version !== 4) {
        throw new Error('Unsupported runtime artifact');
    }
    const payloads = {};
    for (const name of ['strings.json', 'game-data.json']) {
        const bytes = readFileSync(join(directory, name));
        const declared = manifest.files[name];
        if (bytes.length !== declared.byte_size || hash(bytes) !== declared.sha256) {
            throw new Error(`Selected runtime payload identity mismatch: ${name}`);
        }
        payloads[name] = bytes.toString('utf8');
    }
    const bundle = Buffer.from(`{"manifest":${manifestBytes},"strings":${payloads['strings.json']},"game_data":${payloads['game-data.json']}}`);
    return { lock, directory, manifest, bundle };
}

export function deploymentBase(value = '/') {
    if (!/^\/(?:[A-Za-z0-9_-]+\/)*$/.test(value)) throw new Error('Base must be / or an absolute directory path ending in /');
    return value;
}

export function verifiedGameAssets(directory, runtimeHash, canonicalHash) {
    const bytes = readFileSync(join(directory, 'catalog.json'));
    const catalog = JSON.parse(bytes);
    if (catalog.schema_version !== 1 || catalog.source.runtime_manifest_sha256 !== runtimeHash ||
        catalog.source.canonical_data_hash !== canonicalHash) throw new Error('Artwork catalogue does not match the selected runtime');
    const images = new Map();
    for (const image of Object.values(catalog.images)) {
        if (!/^[a-f0-9]{64}\.png$/.test(image.file) || image.file !== `${image.sha256}.png`) throw new Error('Unsafe artwork filename');
        const png = readFileSync(join(directory, image.file));
        if (hash(png) !== image.sha256 || png.length !== image.bytes) throw new Error(`Artwork integrity mismatch: ${image.file}`);
        images.set(image.file, image);
    }
    for (const item of Object.values(catalog.items)) {
        if (item.image && (!images.has(item.image) || images.get(item.image).width !== item.width || images.get(item.image).height !== item.height)) {
            throw new Error('Artwork item refers to an unverified image');
        }
    }
    for (const key of [...Object.values(catalog.actions), ...Object.values(catalog.influences), ...Object.values(catalog.harvest ?? {})]) {
        if (!catalog.items[key]) throw new Error('Artwork alias refers to an unknown item');
    }
    return {bytes, catalog};
}

export async function buildData(root = repoRoot) {
    const { lock, manifest, bundle } = selectedRuntime(root);
    const base = deploymentBase(process.env.POECRAFT_BASE_PATH || '/');
    const {bytes: assetCatalogBytes, catalog: assetCatalog} = verifiedGameAssets(
        join(root, 'apps/web/public/game-assets'), lock.manifest_sha256, manifest.source.data_hash);
    const mode = process.env.POECRAFT_ECONOMY_MODE || lock.economy.mode;
    const indexUrl = process.env.POECRAFT_ECONOMY_INDEX_URL || lock.economy.index;
    if (!['bundled', 'live'].includes(mode) || (mode === 'bundled' && indexUrl !== 'economy/league-index.json') ||
        (mode === 'live' && !/^https:\/\//.test(indexUrl))) throw new Error('Invalid economy delivery configuration');
    const wasmDirectory = join(root, 'bindings/wasm/dist');
    const createModule = (await import(pathToFileURL(join(wasmDirectory, 'poecraft_engine.mjs')).href)).default;
    const module = await createModule({ wasmBinary: readFileSync(join(wasmDirectory, 'poecraft_engine.wasm')) });
    const abi = module.ccall('pcw_abi_version', 'number', [], []);
    const head = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).trim();
    if (process.env.POECRAFT_SELECTED_COMMIT && process.env.POECRAFT_SELECTED_COMMIT !== head) throw new Error('Selected commit differs from checkout');
    const scopes = ['apps/web', 'scripts', 'bindings/wasm', 'data/runtime-snapshots', '.github', '.gitattributes', '.gitignore'];
    const dirty = execFileSync('git', ['status', '--porcelain', '--', ...scopes], { cwd: root, encoding: 'utf8' }).trim().length > 0;
    if (process.env.CI && dirty) throw new Error('Deployment inputs must be committed');
    const sourcePaths = [...new Set(execFileSync('git', ['ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', ...scopes], { cwd: root, encoding: 'utf8' }).split('\0').filter(Boolean))].sort();
    // A local rename leaves the old path in Git's index until staging. Record
    // its deletion explicitly rather than failing before a reviewable build.
    const sourceIdentity = hash(JSON.stringify(sourcePaths.map(path => [path,
        existsSync(join(root, path)) ? hash(readFileSync(join(root, path))) : null])));
    const inputs = {
        schema_version: 1, repository_commit: head, working_tree_dirty: dirty,
        source_tree_sha256: sourceIdentity,
        game_assets: { catalog_sha256: hash(assetCatalogBytes), image_count: Object.keys(assetCatalog.images).length },
        base, engine: { abi_version: abi, source_commit: null,
            loader_sha256: hash(readFileSync(join(wasmDirectory, 'poecraft_engine.mjs'))),
            wasm_sha256: hash(readFileSync(join(wasmDirectory, 'poecraft_engine.wasm'))) },
        runtime: { manifest_sha256: lock.manifest_sha256, source: manifest.source,
            artifact_schema_version: manifest.artifact_schema_version, files: manifest.files,
            bundle_sha256: hash(bundle), bundle_bytes: bundle.length, url: `poecraft-data.${hash(bundle)}.json` },
        economy: { mode, index_url: indexUrl,
            bundled_index_sha256: hash(readFileSync(join(root, 'apps/web/public/economy/league-index.json'))) },
    };
    const build = { ...inputs, build_id: hash(JSON.stringify(inputs)) };
    const publicDir = join(root, 'apps/web/public');
    for (const name of readdirSync(publicDir)) {
        if (/^poecraft-data(?:\.[a-f0-9]{64})?\.json$/.test(name)) unlinkSync(join(publicDir, name));
    }
    writeFileSync(join(publicDir, build.runtime.url), bundle);
    const generated = join(root, 'apps/web/src/generated');
    mkdirSync(generated, { recursive: true });
    writeFileSync(join(generated, 'build-info.json'), JSON.stringify(build, null, 2) + '\n');
    console.log(`Verified runtime ${lock.manifest_sha256}; ABI ${abi}; bundle ${bundle.length} bytes; build ${build.build_id}`);
    return build;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await buildData();
