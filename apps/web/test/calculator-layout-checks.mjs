import assert from 'node:assert/strict';

const sizes = [{width: 1366, height: 768}, {width: 1536, height: 864}, {width: 1728, height: 1000}];
const bounds = locator => locator.evaluate(element => {
    const rect = element.getBoundingClientRect();
    return {x: rect.x, y: rect.y, right: rect.right, bottom: rect.bottom};
});
async function resize(page, size, host) {
    await page.setViewportSize(size);
    await page.waitForFunction(host => Math.abs(document.querySelector(host).getBoundingClientRect().bottom - innerHeight) < 1, host);
}

/** Keep the full-width ledger and both editing surfaces usable in laptop windows. */
export async function checkCalculatorLayout(page) {
    await page.waitForFunction(() => document.querySelector('pc-calculator')?.item && !document.querySelector('pc-calculator')?.busy);
    const viewport = page.viewportSize();
    for (const size of sizes) {
        await resize(page, size, '.pc-calculator');
        for (const context of ['input', 'goal']) {
            await page.locator(`[data-calc-context="${context}"]`).click();
            const card = page.locator(`[data-context-card="${context}"]`);
            assert.ok(await card.isVisible());
            const ledger = await card.locator('.pc-mod-group-prefix, .pc-mod-group-suffix').evaluateAll(groups => groups.map(group => {
                const rect = group.getBoundingClientRect();
                return {x: rect.x, width: rect.width, y: rect.y};
            }));
            assert.equal(ledger.length, 2);
            assert.equal(ledger[0].x, ledger[1].x);
            assert.equal(ledger[0].width, ledger[1].width);
            assert.ok(ledger[1].y > ledger[0].y, 'Prefixes and suffixes retain the normal vertical ledger');
        }
        const before = await bounds(page.locator('.pc-calc-contexts'));
        for (const tool of ['solve', 'odds']) {
            await page.locator(`[data-calc-tool="${tool}"]`).click();
            assert.ok(await page.locator('pc-calculator pc-craft-controls').isVisible());
            assert.ok(await page.locator('pc-calculator pc-mod-pool').isVisible());
            const pane = page.locator(`[data-calc-pane="${tool}"]`);
            assert.ok(await pane.evaluate(element => element.scrollWidth <= element.clientWidth + 1), `${tool} controls fit the compact results panel`);
        }
        await page.locator('pc-calculator [data-craft-panel="harvest"]').click();
        await page.locator('pc-calculator .pc-craft-panel-body').evaluate(pane => {pane.scrollTop = pane.scrollHeight;});
        assert.deepEqual(await bounds(page.locator('.pc-calc-contexts')), before);
        for (const selector of ['.pc-calc-contexts', '.pc-calc-editing', '.pc-calc-tools']) {
            const rect = await bounds(page.locator(selector));
            assert.ok(rect.x >= 0 && rect.right <= size.width && rect.bottom <= size.height);
        }
    }
    await resize(page, viewport, '.pc-calculator');
}

export async function checkEmulatorLayout(page) {
    await page.waitForFunction(() => document.querySelector('pc-emulator')?.item && !document.querySelector('pc-emulator')?.busy);
    const viewport = page.viewportSize();
    for (const size of sizes) {
        await resize(page, size, '.pc-emulator');
        const item = await bounds(page.locator('.pc-emu-item'));
        const pool = await bounds(page.locator('.pc-emu-pool'));
        for (const panel of ['basic', 'essence', 'harvest', 'fossil']) {
            await page.locator(`pc-emulator [data-craft-panel="${panel}"]`).click();
            assert.deepEqual(await bounds(page.locator('.pc-emu-item')), item);
            assert.deepEqual(await bounds(page.locator('.pc-emu-pool')), pool);
            const craft = await bounds(page.locator('pc-emulator pc-craft-controls'));
            const history = await bounds(page.locator('.pc-emu-side'));
            assert.ok(craft.right <= size.width && history.bottom <= size.height);
            assert.ok(history.y >= craft.bottom && history.bottom - history.y >= 160);
        }
        assert.ok(item.bottom <= size.height && pool.bottom <= size.height);
    }
    await page.locator('pc-emulator [data-craft-panel="basic"]').click();
    await resize(page, viewport, '.pc-emulator');
}
