import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { selectedRuntime, verifiedGameAssets } from '../../../scripts/build-data-bundle.mjs';
import { publicAsset, verifiedRuntime } from '../src/app/public-assets';
import { beginDiagnosticRun, diagnostics } from '../src/app/tester-diagnostics';
import { economyCompatibility } from '../src/app/workspace/economy-service';

test('runtime paths support root, project and explicit external URLs', () => {
    assert.equal(publicAsset('economy/league-index.json'), '/economy/league-index.json');
    assert.equal(publicAsset('economy/league-index.json', '/poecraft2/'), '/poecraft2/economy/league-index.json');
    assert.equal(publicAsset('https://example.org/economy/index.json', '/poecraft2/'), 'https://example.org/economy/index.json');
});

test('runtime rejects corruption, HTML and missing assets before the native loader', async () => {
    const body = Buffer.from('{"data":1}');
    const expected = { bundle_sha256: createHash('sha256').update(body).digest('hex'), bundle_bytes: body.length };
    const response = (text: string, status = 200, type = 'application/json') => new Response(text, { status, headers: { 'content-type': type } });
    assert.deepEqual(await verifiedRuntime(response(body.toString()), expected), new Uint8Array(body));
    await assert.rejects(verifiedRuntime(response('{"data":2}'), expected), /integrity/);
    await assert.rejects(verifiedRuntime(response('<html/>', 200, 'text/html'), expected), /not JSON/);
    await assert.rejects(verifiedRuntime(response('missing', 404), expected), /404/);
});

test('selected runtime validates payload bytes and selection separately', () => {
    const root = new URL('../../../', import.meta.url);
    const scratch = mkdtempSync(join(tmpdir(), 'poecraft-runtime-test-'));
    const lock = JSON.parse(readFileSync(new URL('apps/web/runtime.lock.json', root), 'utf8'));
    cpSync(new URL('apps/web/runtime.lock.json', root), join(scratch, 'apps/web/runtime.lock.json'), { recursive: true });
    cpSync(new URL(`${lock.runtime_directory}/`, root), join(scratch, lock.runtime_directory), { recursive: true });
    assert.equal(selectedRuntime(scratch).lock.manifest_sha256, lock.manifest_sha256);
    const payload = join(scratch, lock.runtime_directory, 'strings.json');
    writeFileSync(payload, '{}');
    assert.throws(() => selectedRuntime(scratch), /payload identity mismatch/);
    lock.manifest_sha256 = '0'.repeat(64);
    writeFileSync(join(scratch, 'apps/web/runtime.lock.json'), JSON.stringify(lock));
    assert.throws(() => selectedRuntime(scratch), /Invalid product runtime selection/);
});

test('generic diagnostics freeze a non-Allflame request including checkpoint, flags and effective prices', () => {
    const request = { base: 'stable/base', state: { checkpoint: { mods: ['stable/mod'] } },
        goal: { slots: [] }, options: { seed: 42, mode: 'example' },
        economy: { profile: 'standard', prices: { chaos: 1, base: 7 }, overrides: { base: 7 }, fallback: null } };
    const run = beginDiagnosticRun('contract-test', request);
    request.economy.prices.base = 99;
    run.status = 'completed';
    const exported = diagnostics() as { runs: { record: typeof run }[] };
    assert.equal((exported.runs.at(-1)!.record.request as typeof request).economy.prices.base, 7);
    assert.deepEqual((exported.runs.at(-1)!.record.request as typeof request).state.checkpoint.mods, ['stable/mod']);
    assert.equal(economyCompatibility({ metadata: { game_data_hash: null } } as never), 'manual');
    assert.equal(economyCompatibility({ metadata: { game_data_hash: 'wrong' } } as never), 'unqualified');
});

test('artwork packaging rejects wrong runtime, dangling references and corrupt image bytes', () => {
    const scratch = mkdtempSync(join(tmpdir(), 'poecraft-art-test-'));
    const bytes = Buffer.from('image fixture');
    const sha = createHash('sha256').update(bytes).digest('hex');
    const file = sha + '.png';
    const catalog = {schema_version: 1, source: {runtime_manifest_sha256: 'runtime', canonical_data_hash: 'source'},
        items: {base: {image: file, width: 32, height: 64}}, actions: {chaos: 'base'}, influences: {},
        images: {fixture: {file, sha256: sha, bytes: bytes.length, width: 32, height: 64}}};
    const write = () => writeFileSync(join(scratch, 'catalog.json'), JSON.stringify(catalog));
    writeFileSync(join(scratch, file), bytes); write();
    assert.equal(verifiedGameAssets(scratch, 'runtime', 'source').catalog.items.base.image, file);
    assert.throws(() => verifiedGameAssets(scratch, 'different-runtime', 'source'), /does not match/);
    catalog.items.base.image = 'missing.png'; write();
    assert.throws(() => verifiedGameAssets(scratch, 'runtime', 'source'), /unverified image/);
    catalog.items.base.image = file; write();
    writeFileSync(join(scratch, file), 'corrupt');
    assert.throws(() => verifiedGameAssets(scratch, 'runtime', 'source'), /integrity mismatch/);
});
