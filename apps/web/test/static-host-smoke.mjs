// Serve only real files, without Vite, proxying or SPA fallback.
import { createServer } from 'node:http';
import { readFileSync, statSync } from 'node:fs';
import { resolve, extname, sep } from 'node:path';
import { chromium, firefox } from 'playwright';
import assert from 'node:assert/strict';
import { checkUiContinuity } from './ui-continuity-checks.mjs';
import { captureUiCheckpoint } from './ui-presentation-checks.mjs';

const directory = resolve(process.argv[2] || 'dist');
const build = JSON.parse(readFileSync(resolve(directory, 'build-info.json')));
const mime = { '.html': 'text/html', '.json': 'application/json', '.js': 'text/javascript', '.wasm': 'application/wasm', '.css': 'text/css', '.txt': 'text/plain', '.png': 'image/png', '.woff2': 'font/woff2' };
const server = createServer((request, response) => {
    try {
        const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
        if (!pathname.startsWith(build.base)) throw new Error('outside base');
        const suffix = pathname.slice(build.base.length) || 'index.html';
        const path = resolve(directory, suffix);
        if (!path.startsWith(directory + sep) || !statSync(path).isFile()) throw new Error('missing');
        response.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' });
        response.end(readFileSync(path));
    } catch {
        response.writeHead(404, { 'Content-Type': 'text/plain' }); response.end('Not found');
    }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
try {
    assert.equal((await fetch(origin + build.base + 'missing.json')).status, 404);
    const selectedBrowsers = process.env.POECRAFT_SMOKE_BROWSERS?.split(',') || ['chromium', 'firefox'];
    if (selectedBrowsers.some(name => !['chromium', 'firefox'].includes(name))) throw new Error('Unknown smoke browser');
    for (const browserType of [chromium, firefox].filter(type => selectedBrowsers.includes(type.name()))) {
        const browser = await browserType.launch({ headless: true });
        try {
            const page = await browser.newPage();
            page.setDefaultTimeout(30_000);
            const failures = [], responses = [];
            page.on('pageerror', e => failures.push(e.message));
            page.on('console', message => { if (message.type() === 'error') failures.push(message.text()); });
            page.on('response', r => responses.push({ url: r.url(), status: r.status(), type: r.headers()['content-type'] }));
            await page.goto(origin + build.base);
            await page.waitForFunction(() => document.querySelector('pc-emulator')?.client?.getAbiVersion() > 0 && document.querySelector('pc-emulator')?.dataId > 0);
            await page.waitForFunction(() => document.querySelector('pc-economy-selector')?.textContent.includes('Bundled snapshot'));
            const result = await page.evaluate(async () => {
                const emulator = document.querySelector('pc-emulator');
                const client = emulator.client;
                const session = await client.createSession(emulator.dataId, 'Metadata/Items/Armours/BodyArmours/BodyInt17', 86);
                const context = await client.createContext(session, 42);
                const item = await client.createItem(session, { rarity: 'rare', withImplicits: false });
                const pool = await client.debugPool(context, item, { action: { type: 'exalt' }, side: 'prefix' });
                const mod = await client.modInfo(session, pool.entries[0].session_mod_id);
                const solver = await client.openSolver(session, { version: 'v1', rarity: 'rare', slots: [{ family_mod_key: mod.key, min_tier: 0 }], actions: ['exalt'] });
                const odds = await client.solverCalc(solver, item, 'exalt');
                await client.addMod(item, session, { key: mod.key, side: 'prefix' });
                const state = await client.exportItem(item);
                const imported = await client.importItem(state);
                const roundTrip = await client.exportItem(imported);
                const strategy = { version: 'v1', name: 'Hosted lifecycle', start_node_id: 'start',
                    base_state: { base_key: 'Metadata/Items/Armours/BodyArmours/BodyInt17', item_level: 86, rarity: 'rare' },
                    nodes: [{ id: 'start', kind: 'start' }, { id: 'success', kind: 'terminal', terminal: 'success' }],
                    edges: [{ id: 'finish', from: 'start', to: 'success', priority: 0, condition: { type: 'always' } }] };
                const strategyDocument = JSON.parse(JSON.stringify(strategy));
                const exact = await client.strategyEvaluate(session, strategyDocument, { include_success_normalized: true });
                const economy = await client.loadEconomy({ version: 'v1', id: 'smoke-manual', prices: { exalt: 1 } });
                const abort = new AbortController(); abort.abort();
                const cancelled = await client.solverSolve(solver, item, economy, { max_states: 8, max_sweeps: 1 }, { signal: abort.signal });
                await client.closeEconomy(economy); await client.closeSolver(solver);
                await client.closeItem(imported); await client.closeItem(item);
                await client.closeContext(context); await client.closeSession(session);
                return { abi: client.getAbiVersion(), roundTrip: JSON.stringify(state) === JSON.stringify(roundTrip), supported: odds.supported, legal: odds.legal,
                    probability: odds.outcomes.reduce((sum, row) => sum + row.probability, 0), exact, cancelled: cancelled.cancelled };
            });
            assert.equal(result.abi, build.engine.abi_version);
            assert.equal(result.roundTrip, true);
            assert.equal(result.supported, true); assert.equal(result.legal, true);
            assert.ok(Math.abs(result.probability - 1) < 1e-9);
            assert.equal(result.exact.converged, true); assert.equal(result.cancelled, true);
            const ui = build.game_assets ? await checkUiContinuity(page).catch(async error => {
                console.error(JSON.stringify({failures, status: await page.locator('.pc-emu-status, .pc-calc-status').allTextContents()}));
                throw error;
            }) : {skipped: 'archive predates React/asset migration'};
            assert.deepEqual(failures, []);
            for (const suffix of ['.wasm', build.runtime.url, 'league-index.json']) {
                assert.ok(responses.some(r => r.url.endsWith(suffix) && r.status === 200), `missing successful ${suffix}`);
            }
            assert.ok(responses.some(r => r.url.includes('engine-worker') && r.status === 200));
            assert.ok(responses.some(r => r.url.includes('/economy/snapshots/') && r.status === 200));
            assert.ok(responses.some(r => r.url.endsWith('.wasm') && r.type === 'application/wasm'));
            if (build.game_assets) {
                assert.ok(responses.some(r => r.status === 200 &&
                    r.url.endsWith(`/game-assets/catalog.json?sha256=${build.game_assets.catalog_sha256}`)),
                    'artwork catalogue request must be scoped to its content hash');
                const withoutArt = await browser.newPage();
                await withoutArt.route('**/game-assets/catalog.json*', route => route.fulfill({status: 404, body: 'missing'}));
                await withoutArt.goto(origin + build.base);
                await withoutArt.locator('pc-emulator .pc-bp-confirm:not(:disabled)').waitFor();
                assert.equal(await withoutArt.locator('.pc-game-art').count(), 0);
                await withoutArt.locator('pc-emulator .pc-bp-confirm').click();
                await withoutArt.locator('pc-emulator [data-simple-action="alchemy"]:not(:disabled)').waitFor();
                ui.artworkFallback = true;
                await withoutArt.close();
            }
            // The local font is optional for usability; the fallback keeps a working app.
            const withoutFont = await browser.newPage();
            await withoutFont.route('**/*.woff2', route => route.fulfill({status: 404, body: 'missing'}));
            await withoutFont.goto(origin + build.base);
            await withoutFont.locator('pc-emulator .pc-bp-confirm:not(:disabled)').click();
            await withoutFont.locator('pc-emulator [data-simple-action="alchemy"]:not(:disabled)').waitFor();
            await withoutFont.evaluate(() => document.fonts.ready);
            assert.equal(await withoutFont.evaluate(() => document.fonts.check('400 14px "Noto Sans"')), false);
            await captureUiCheckpoint(withoutFont, 'font-fallback');
            await withoutFont.close();
            // A tab pinned to old JS must fail safely if its data disappeared.
            const stale = await browser.newPage();
            await stale.addInitScript(() => localStorage.setItem('hosting-preserved-draft-marker', 'keep'));
            await stale.route(`**/${build.runtime.url}`, route => route.fulfill({ status: 404, body: 'gone', contentType: 'text/plain' }));
            await stale.goto(origin + build.base);
            await stale.waitForFunction(() => document.querySelector('pc-emulator')?.textContent.includes('reload to retry'));
            assert.equal(await stale.evaluate(() => localStorage.getItem('hosting-preserved-draft-marker')), 'keep');
            console.log(JSON.stringify({ browser: browserType.name(), version: browser.version(), base: build.base, build_id: build.build_id, result: 'passed', requests: responses.length, ui }));
        } finally { await browser.close(); }
    }
} finally { await new Promise(resolve => server.close(resolve)); }
