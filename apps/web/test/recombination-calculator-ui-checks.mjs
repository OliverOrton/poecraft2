import assert from 'node:assert/strict';

/** Focused authored-pair gate in the existing immutable static-host harness. */
export async function checkRecombinationCalculator(page, capture) {
    await page.setViewportSize({width: 1536, height: 864});
    await page.locator('[data-cmd="new-recombination"]').click();
    const host = page.locator('pc-recombination-calculator');
    const ready = () => page.waitForFunction(() => {
        const c = document.querySelector('pc-recombination-calculator');
        return c && !c.busy && c.inputs.size === 2;
    });
    await ready();
    assert.equal(await host.locator('.pc-recomb-error:visible').count(), 0);
    const select = async focus => {
        await host.locator(`.pc-recomb-focus-tabs [data-recomb-focus="${focus}"]`).click();
        assert.equal(await host.locator(`[data-recomb-card="${focus}"]`).evaluate(node => node.classList.contains('is-editing')), true);
        assert.match(await host.locator('.pc-recomb-focus-label').textContent(), new RegExp(focus === 'goal' ? 'Goal result' : `Input ${focus.toUpperCase()}`));
    };
    await select('goal');
    await host.locator('.pc-mod-family-header').first().click();
    const key = await host.locator('.pc-mod-tier').first().getAttribute('data-mod-key');
    const tier = () => host.locator(`.pc-mod-tier[data-mod-key="${key}"] .pc-mod-tier-btn`);
    await tier().click(); await ready();
    assert.equal(await host.locator('[data-recomb-item="goal"] .pc-mod-slot.is-filled').count(), 1);
    for (const focus of ['a', 'b']) {
        await select(focus); await tier().click(); await ready();
        assert.equal(await host.locator(`[data-recomb-item="${focus}"] .pc-mod-slot.is-filled`).count(), 1);
    }
    await capture(page, 'recombination-input-b');
    await select('a');
    await capture(page, 'recombination-input-a');
    await select('goal');
    const calculate = async () => {
        await host.locator('[data-recomb-calculate]').click();
        await page.waitForFunction(() => !document.querySelector('pc-recombination-calculator').calculating);
    };
    await calculate();
    assert.equal(await host.locator('.pc-recomb-answer strong').textContent(), '100%');
    await capture(page, 'recombination-goal-odds');
    await host.locator('[data-recomb-base-care]').check(); await ready();
    assert.equal(await host.locator('.pc-recomb-answer').count(), 0, 'Base-care invalidates old odds immediately');
    await calculate();
    assert.equal(await host.locator('.pc-recomb-answer strong').textContent(), '100%', 'Equal bases count both carriers');

    // Repeated/interrupted base picker, invalid level, focus trap and recovery.
    await host.locator('[data-recomb-base="b"]').click();
    const initialBase = await host.locator('.pc-bp-base').inputValue();
    await host.locator('.pc-bp-ilvl').fill('0');
    assert.equal(await host.locator('.pc-bp-confirm').isDisabled(), true);
    await page.keyboard.press('Escape');
    assert.equal(await host.locator('[data-recomb-base="b"]').evaluate(node => node === document.activeElement), true);
    await host.locator('[data-recomb-base="b"]').click();
    assert.equal(await host.locator('.pc-bp-base').inputValue(), initialBase);
    await page.keyboard.press('Shift+Tab');
    assert.equal(await host.locator('.pc-bp-confirm').evaluate(node => node === document.activeElement), true);
    await page.keyboard.press('Escape');
    await host.locator('[data-recomb-base="b"]').click();
    const differentBase = await host.locator('.pc-bp-base option').evaluateAll((nodes, original) =>
        nodes.find(node => node.value && node.value !== original)?.value, initialBase);
    assert.ok(differentBase);
    await host.locator('.pc-bp-base').selectOption(differentBase);
    await host.locator('.pc-bp-confirm').click(); await ready();
    assert.equal(await host.locator('.pc-recomb-answer').count(), 0);
    await host.locator('[data-recomb-required-base]').selectOption('b'); await ready();
    await calculate();
    const baseFiltered = await page.evaluate(() => {
        const c = document.querySelector('pc-recombination-calculator');
        const base = c.inputs.get('b').snapshot.base;
        return {expected: c.result.outcomes.filter(row => row.is_goal && row.base_metadata_path === base).reduce((sum, row) => sum + row.probability, 0),
            displayed: c.querySelector('.pc-recomb-answer strong').textContent};
    });
    assert.equal(baseFiltered.displayed, `${(baseFiltered.expected * 100).toLocaleString(undefined, {maximumFractionDigits: 4})}%`);

    // Delay delivery of a REAL native result to check edits and Cancel.
    for (const action of ['base-edit', 'cancel']) {
        await page.evaluate(() => {
            const c = document.querySelector('pc-recombination-calculator');
            const original = c.client.recombinationCalculate.bind(c.client);
            c.__restore = () => {c.client.recombinationCalculate = original;};
            c.__pending = false;
            c.client.recombinationCalculate = async (...args) => {
                const result = await original(...args);
                await new Promise(resolve => {c.__release = resolve; c.__pending = true;});
                return result;
            };
        });
        await host.locator('[data-recomb-calculate]').click();
        await page.waitForFunction(() => document.querySelector('pc-recombination-calculator').__pending);
        if (action === 'base-edit') {await host.locator('[data-recomb-base-care]').uncheck(); await ready();}
        else await host.locator('[data-recomb-cancel]').click();
        await page.evaluate(async () => {
            const c = document.querySelector('pc-recombination-calculator');
            c.__release(); c.__restore(); await Promise.all(c.calculations);
        });
        assert.equal(await host.locator('.pc-recomb-answer').count(), 0, 'Stale native delivery stays cancelled');
    }

    // Engine refusal is visible, with no unsupported odds retained.
    const aProperties = host.locator('[data-recomb-item="a"] .pc-item-properties');
    await aProperties.locator('summary').click();
    await aProperties.locator('[aria-label="Item corruption"]').selectOption('true'); await ready();
    await calculate();
    assert.match(await host.locator('[data-recomb-output]').textContent(), /Odds unavailable:/);
    assert.equal(await host.locator('.pc-recomb-answer').count(), 0);
    await capture(page, 'recombination-unsupported');
    await aProperties.locator('[aria-label="Item corruption"]').selectOption('false'); await ready();

    // Native atomic editor rejects incompatible additions without corrupting input.
    await page.evaluate(async () => {
        const c = document.querySelector('pc-recombination-calculator');
        const input = c.inputs.get('a');
        const before = JSON.stringify(input.snapshot.state);
        await c.client.editItem(input.item, input.session, {add_explicit: 'not-a-real-mod'}).then(
            () => {throw new Error('Invalid modifier unexpectedly accepted');}, () => {});
        const after = await c.client.exportItem(input.item, input.session);
        if (JSON.stringify(after) !== before) throw new Error('Invalid edit changed authored input');
    });
    await page.mouse.move(0, 0);
    await page.waitForFunction(() => getComputedStyle(document.querySelector('pc-recombination-calculator [data-recomb-calculate]')).backgroundColor === 'rgb(230, 160, 120)');
    const styles = await host.evaluate(node => {
        const button = node.querySelector('[data-recomb-calculate]'), style = getComputedStyle(button);
        return {background: style.backgroundColor, text: style.color,
            modWeights: [...node.querySelectorAll('.pc-mod-tier-text, .pc-mod-family-name, .pc-mod-lines')].map(mod => getComputedStyle(mod).fontWeight)};
    });
    assert.equal(styles.background, 'rgb(230, 160, 120)'); assert.equal(styles.text, 'rgb(24, 24, 24)');
    assert.ok(styles.modWeights.length > 0 && styles.modWeights.every(weight => weight === '400'));
    await host.locator('[data-recomb-calculate]').hover();
    assert.equal(await host.locator('[data-recomb-calculate]').evaluate(node => getComputedStyle(node).backgroundColor), 'rgb(240, 180, 142)');
    await page.mouse.move(0, 0);
    await host.locator('[data-recomb-calculate]').focus();
    await page.keyboard.press('Shift+Tab'); await page.keyboard.press('Tab');
    assert.equal(await host.locator('[data-recomb-calculate]').evaluate(node => node === document.activeElement), true);
    assert.notEqual(await host.locator('[data-recomb-calculate]').evaluate(node => getComputedStyle(node).outlineStyle), 'none');

    // Narrow stacking, usable cards/picker, and recovery retain separate inputs.
    await page.setViewportSize({width: 390, height: 844});
    const narrow = await host.evaluate(node => {
        const boxes = ['.pc-recomb-a', '.pc-recomb-b', '.pc-recomb-goal', '.pc-recomb-picker'].map(selector => {
            const box = node.querySelector(selector).getBoundingClientRect(); return {x: box.x, right: box.right, top: box.top};
        });
        return {width: node.clientWidth, scroll: node.scrollWidth, boxes};
    });
    assert.ok(narrow.scroll <= narrow.width + 1, 'Calculator has no horizontal mobile overflow');
    assert.ok(narrow.boxes.every(box => box.x >= 0 && box.right <= 391));
    assert.ok(narrow.boxes.every((box, index) => index === 0 || box.top > narrow.boxes[index - 1].top));
    await host.locator('.pc-recomb-goal').scrollIntoViewIfNeeded(); await capture(page, 'recombination-narrow-goal');
    await host.locator('.pc-recomb-picker').scrollIntoViewIfNeeded(); await capture(page, 'recombination-narrow-picker');
    await page.reload(); await ready();
    assert.equal(await host.locator('[data-recomb-item="a"] .pc-mod-slot.is-filled').count(), 1);
    assert.match(await host.locator('[data-recomb-item="b"] .pc-item-title-line').textContent(), /iLvl 86/);
    assert.equal(await host.locator('[data-recomb-item="goal"] .pc-mod-slot.is-filled').count(), 1);
    assert.equal(await host.locator('.pc-recomb-answer').count(), 0, 'Recovered drafts do not recover stale probability results');
    return {result: 'passed', native: 'real immutable WASM pair projection', screenshots: 6,
        controls: ['A/B/goal focus', 'shared picker', 'base toggle', 'equal and different bases', 'unsupported input',
            'atomic invalid edit', 'stale delivery', 'cancel', 'repeated picker', 'keyboard focus', 'narrow layout', 'reload recovery']};
}
