// Faithful browser adapter of calculator-delivery-probe.ts. Uses an isolated
// Vite listener and actual Chromium DOM/client/worker/WASM; no dev-server restart.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { createServer } from 'vite';
import { chromium } from 'playwright';

const [casePath, output] = process.argv.slice(2);
assert.ok(casePath && output);
const root = resolve('../..');
const spec = JSON.parse(readFileSync(casePath, 'utf8'));
const snapshotBytes = readFileSync(resolve(root, spec.economy.snapshot_path));
const snapshot = JSON.parse(snapshotBytes.toString('utf8'));
assert.equal(snapshot.metadata.content_sha256, spec.economy.content_sha256);
const economy = {...snapshot, prices: {...snapshot.prices, ...spec.economy.manual_overrides}};
const build = JSON.parse(readFileSync('src/generated/build-info.json', 'utf8'));
const server = await createServer({server: {host: '127.0.0.1', port: 0, strictPort: true}, logLevel: 'error'});
let browser;
const receipt = {case_path: casePath, case_sha256: createHash('sha256').update(readFileSync(casePath)).digest('hex'),
    source_commit: build.repository_commit, wasm_sha256: build.engine.wasm_sha256,
    runtime: build.runtime, economy_sha256: createHash('sha256').update(snapshotBytes).digest('hex'),
    economy_content_sha256: spec.economy.content_sha256, status: 'running',
    scope: 'fresh Chromium actual Calculator SOLVE; default profile and default 240s Finish; detached fixture DOM as existing Calculator probe; no rendered visual review',
    parent_app_watchdog_ms: 300000, page_errors: [], console_errors: []};
const began = performance.now();
try {
    await server.listen();
    const origin = `http://127.0.0.1:${server.httpServer.address().port}`;
    const channel = process.env.POECRAFT_TEST_BROWSER_CHANNEL;
    assert.ok(channel === undefined || channel === 'chrome');
    browser = await chromium.launch({headless: true, ...(channel ? {channel} : {})});
    receipt.browser_channel = channel ?? 'pinned-chromium';
    receipt.browser_version = browser.version();
    const page = await browser.newPage();
    page.setDefaultTimeout(30000);
    page.on('pageerror', error => receipt.page_errors.push(error.message));
    page.on('console', message => {if (message.type() === 'error') receipt.console_errors.push(message.text());});
    await page.goto(origin + build.base);
    await page.waitForFunction(() => document.querySelector('pc-emulator')?.dataId > 0);
    await page.waitForFunction(() => document.querySelector('pc-economy-selector')?.textContent.includes('Bundled snapshot'));
    const result = await page.evaluate(async ({spec, economy}) => {
        const {PcCalculator} = await import('/src/app/components/pc-calculator.tsx');
        const prices = await import('/src/app/workspace/prices.ts');
        const emulator = document.querySelector('pc-emulator');
        const client = emulator.client;
        const baselineMemory = await client.memoryStats();
        const session = await client.createSession(emulator.dataId, spec.session.base_metadata_path, spec.session.item_level);
        const item = await client.createItem(session, {rarity: spec.start.rarity, withImplicits: spec.start.with_implicits});
        let solver = 0;
        const measured = {baseline: baselineMemory};
        let finalTelemetry;
        const originalSolve = client.solverSolve.bind(client);
        client.solverSolve = async (...args) => {
            measured.before_solve = await client.memoryStats();
            const result = await originalSolve(...args);
            measured.after_solve_before_cleanup = await client.memoryStats();
            finalTelemetry = await client.solverTelemetry(args[0]);
            return result;
        };
        const calculator = new PcCalculator();
        try {
            for (const mod of spec.start.mods) await client.addMod(item, session, {key: mod.key,
                fractured: mod.flags.includes('fractured') || undefined, crafted: mod.flags.includes('crafted') || undefined,
                veiled: mod.flags.includes('veiled') || undefined});
            solver = await client.openSolver(session, spec.product_action_envelope.envelope_goal);
            const actions = await client.solverActions(solver);
            prices.setFallbackPrice(null);
            for (const key of Object.keys(prices.getPrices())) prices.setPrice(key, null);
            for (const [key, value] of Object.entries(economy.prices)) prices.setPrice(key, value);
            calculator.innerHTML = '<div class="pc-calc-solve-panel"></div>';
            Object.assign(calculator, {client, session, item, solver, pickerActions: actions,
                base: spec.session.base_metadata_path, itemLevel: spec.session.item_level, itemRarity: spec.start.rarity,
                goalRarity: spec.goal.rarity, minSatisfiedSlots: spec.goal.min_satisfied_slots ?? spec.goal.slots.length,
                allowExtraModifiers: spec.goal.allow_extra_modifiers ?? false,
                solveDisabledActionFamilies: new Set(spec.goal.disabled_action_families ?? []),
                slots: spec.goal.slots.map(s => ({...('family_mod_key' in s ? {familyModKey: s.family_mod_key} : {group: s.group}), minTier: s.min_tier ?? 1})),
                solveAllowEconomicRestart: false, solveConsiderImprintPrograms: false, solveMode: 'current'});
            // Normal connectedCallback clears busy after its item/goal load.
            // This faithful fixture adapter supplies those handles directly.
            calculator.setBusy(false);
            calculator.renderSolvePanel();
            const button = calculator.querySelector('[data-solve-cmd="start"]');
            if (!button || button.disabled) throw new Error('Actual Calculator SOLVE button is not enabled');
            let completion;
            const originalStart = calculator.startSolve.bind(calculator);
            calculator.startSolve = (...args) => { completion = originalStart(...args); return completion; };
            button.click();
            if (!completion) throw new Error('SOLVE click did not dispatch actual Calculator startSolve');
            await completion;
            measured.after_calculator_cleanup = await client.memoryStats();
            return {trace: calculator.solveProgressExport, summary: calculator.solveSummary,
                error: calculator.solveError, usable_strategy: Boolean(calculator.solvedStrategy),
                graph: calculator.solvedStrategy, final_telemetry: finalTelemetry, memory: measured};
        } finally {
            client.solverSolve = originalSolve;
            if (solver) await client.closeSolver(solver);
            await client.closeItem(item);
            await client.closeSession(session);
        }
    }, {spec, economy});
    Object.assign(receipt, result);
    assert.equal(result.trace?.status, 'completed');
    assert.equal(result.error, null);
    assert.equal(result.usable_strategy, true);
    assert.equal(result.trace.request.bounded_finish_after_ms, 240000);
    assert.equal(result.trace.request.solve_options.solve_profile, 'calculator_product_v1');
    for (const cap of ['max_states', 'max_discovered_states', 'max_expanded_states', 'max_solver_owned_bytes'])
        assert.equal(result.trace.request.solve_options[cap], undefined, `product default cap omission: ${cap}`);
    assert.ok(!result.trace.ui_milestones.some(entry => entry.stage === 'finish_intent'));
    assert.equal(result.trace.worker.work_policy, 'adaptive');
    assert.equal(result.trace.worker.step_transport, 'compact');
    assert.deepEqual(receipt.page_errors, []);
    assert.deepEqual(receipt.console_errors, []);
    receipt.status = 'passed';
} catch (error) {
    receipt.status = 'failed';
    receipt.error = String(error);
    throw error;
} finally {
    if (browser) await browser.close();
    await server.close();
    receipt.wall_ms = performance.now() - began;
    writeFileSync(output, JSON.stringify(receipt, null, 2) + '\n');
    console.log(JSON.stringify({status: receipt.status, wall_ms: receipt.wall_ms, output,
        summary: receipt.summary, memory: receipt.memory, error: receipt.error}));
}
