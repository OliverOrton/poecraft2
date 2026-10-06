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
        const selector = page.viewportSize().width <= 900 ? '.pc-recomb-mobile-tabs' : '.pc-recomb-focus-tabs';
        await host.locator(`${selector} [data-recomb-focus="${focus}"]`).click();
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
    const compactOddsHeight = await host.locator('.pc-recomb-odds').evaluate(node => node.getBoundingClientRect().height);
    assert.ok(compactOddsHeight <= 86, 'Successful odds stays a compact action strip');
    assert.equal(await host.locator('.pc-recomb-odds table:visible').count(), 0, 'Carrier table is initially collapsed');
    await host.locator('.pc-recomb-odds-details > summary').click();
    assert.equal(await host.locator('.pc-recomb-odds table:visible').count(), 1);
    assert.match(await host.locator('.pc-recomb-odds-popover').textContent(), /expected attempts|estimated game|Estimated game/);
    await page.keyboard.press('Escape');
    assert.equal(await host.locator('.pc-recomb-odds table:visible').count(), 0);
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

    // Real six-affix native items/goals, rather than one-line layout fixtures.
    const populated = await page.evaluate(async () => {
        const c = document.querySelector('pc-recombination-calculator');
        const a = c.inputs.get('a');
        const patterns = [[0, /to maximum Life/i], [0, /to maximum Energy Shield/i], [0, /increased Energy Shield/i],
            [1, /Fire Resistance/i], [1, /Cold Resistance/i], [1, /Lightning Resistance/i]];
        const groups = new Set();
        const mods = patterns.map(([side, pattern]) => {
            const mod = a.mods.filter(mod => mod.reach_kind === 0 && mod.generation_type === side && mod.required_level <= 86 &&
                mod.family_tier_index === 1 && pattern.test(mod.text_lines.join(' ')) && !groups.has(mod.primary_group_id))
                .sort((left, right) => left.text_lines.length - right.text_lines.length || left.key.localeCompare(right.key))[0];
            if (!mod) throw new Error(`Native realistic layout family missing: ${pattern}`);
            groups.add(mod.primary_group_id); return mod;
        });
        c.edit(async () => {
            for (const side of ['a', 'b']) {
                const old = c.inputs.get(side);
                const input = await c.openInput({base: a.snapshot.base, itemLevel: 86, state: null});
                c.inputs.set(side, input); await c.closeInput(old);
                for (const mod of mods) await c.client.editItem(input.item, input.session, {add_explicit: mod.key});
                await c.refreshInput(input);
            }
            c.goal.slots = []; c.goal.goalImplicitKeys = []; c.goal.goalInfluenceBits = undefined; c.goal.goalCorrupted = undefined;
            c.goal.minSatisfiedSlots = undefined; c.goal.allowExtraModifiers = true; c.baseCare = false;
        });
        await c.currentWork;
        for (const mod of mods) {c.addGoal(mod.key); await c.currentWork;}
        c.querySelectorAll('details').forEach(details => details.removeAttribute('open'));
        c.querySelectorAll('pc-mod-list, .pc-mod-pool-body').forEach(node => {node.scrollTop = 0;});
        c.scrollTop = 0;
        return mods.map(mod => ({key: mod.key, lines: mod.text_lines}));
    });
    await ready(); await select('goal'); await calculate();
    assert.equal(await host.locator('.pc-recomb-answer').count(), 1, 'Realistic native pair has supported odds');
    for (const side of ['a', 'b', 'goal']) assert.equal(await host.locator(`[data-recomb-item="${side}"] .pc-mod-slot.is-filled`).count(), 6);
    assert.equal(await page.evaluate(() => window.devicePixelRatio), 1, 'Desktop qualification uses 100% scale');
    const visibleDesktop = async (width, height) => {
        await page.setViewportSize({width, height});
        await page.waitForFunction(expected => document.querySelector('pc-recombination-calculator').clientWidth <= expected, width);
        const layout = await host.evaluate(node => {
            const rect = element => {
                const box = element.getBoundingClientRect();
                return {left: box.left, top: box.top, right: box.right, bottom: box.bottom, height: box.height};
            };
            const named = Object.fromEntries(['a', 'b', 'goal', 'picker', 'odds'].map(name => [name, rect(node.querySelector(`.pc-recomb-${name}`))]));
            const cardRows = side => [...node.querySelectorAll(`[data-recomb-item="${side}"] .pc-mod-slot.is-filled`)].map(rect);
            const ports = Object.fromEntries(['a', 'b', 'goal'].map(side => [side, rect(node.querySelector(`[data-recomb-item="${side}"]`))]));
            ports.pool = rect(node.querySelector('.pc-mod-pool-body'));
            return {named, ports, rows: {a: cardRows('a'), b: cardRows('b')}, goalFirst: rect(node.querySelector('[data-recomb-item="goal"] .pc-mod-slot.is-filled')),
                pickerSearch: rect(node.querySelector('.pc-mod-pool-search')), pickerFirst: rect(node.querySelector('.pc-mod-family-header')),
                calculate: rect(node.querySelector('[data-recomb-calculate]')), scroll: node.scrollHeight, height: node.clientHeight,
                goalScroll: getComputedStyle(node.querySelector('[data-recomb-item="goal"]')).overflowY,
                poolScroll: getComputedStyle(node.querySelector('.pc-mod-pool-body')).overflowY};
        });
        const inScreen = box => box.top >= 0 && box.bottom <= height + 1 && box.left >= 0 && box.right <= width + 1;
        const inPort = (box, port) => box.top >= port.top - 1 && box.bottom <= port.bottom + 1 && box.left >= port.left - 1 && box.right <= port.right + 1;
        assert.ok(Object.values(layout.named).every(inScreen), `Items, goal, picker and odds share the viewport: ${JSON.stringify(layout)}`);
        assert.ok(layout.named.a.right < layout.named.goal.left && layout.named.b.left > layout.named.goal.right);
        assert.ok(layout.named.goal.bottom < layout.named.picker.top);
        assert.ok(layout.named.odds.height <= 86 && inScreen(layout.calculate));
        assert.ok(['a', 'b'].every(side => layout.rows[side].every(row => inScreen(row) && inPort(row, layout.ports[side]))), 'All six affixes on each input are visible inside their scrollports');
        assert.ok(inScreen(layout.goalFirst) && inScreen(layout.pickerSearch) && inScreen(layout.pickerFirst), 'Goal affixes and working mod picker are visible together');
        assert.ok(inPort(layout.goalFirst, layout.ports.goal) && inPort(layout.pickerFirst, layout.ports.pool), 'Visible goal/picker content is not merely clipped inside the panels');
        assert.ok(layout.scroll <= layout.height + 1, 'Desktop workbench does not require whole-page scrolling');
        assert.equal(layout.goalScroll, 'auto'); assert.equal(layout.poolScroll, 'auto');
        await host.locator('[data-recomb-item="goal"] .pc-mod-slot.is-filled').last().scrollIntoViewIfNeeded();
        await host.locator('[data-recomb-item="goal"] .pc-mod-slot.is-filled select').last().focus();
        assert.equal(await host.locator('[data-recomb-item="goal"] .pc-mod-slot.is-filled select').last().evaluate(node => node === document.activeElement), true);
        await host.locator('[data-recomb-item="goal"]').evaluate(node => {node.scrollTop = 0;});
        await host.locator('.pc-mod-pool-search').fill('maximum');
        assert.ok(await host.locator('.pc-mod-family-header:visible').count() > 0);
        await host.locator('.pc-mod-pool-search').fill('');
        await host.locator('[data-recomb-calculate]').focus();
        return layout;
    };
    const desktops = [];
    for (const [width, height] of [[1536, 864], [1366, 768]]) {
        desktops.push(await visibleDesktop(width, height));
        await capture(page, `recombination-populated-${width}`);
    }

    // Logical mobile overview at scroll zero; selected card then natural picker flow.
    await page.setViewportSize({width: 390, height: 844});
    // Dockview uses ResizeObserver to update its panel's explicit dimensions.
    await page.waitForFunction(() => document.querySelector('pc-recombination-calculator').clientWidth <= 390);
    const navigation = await page.locator('.pc-document-actions').evaluate(node => [...node.children].map(child => child.getBoundingClientRect().right));
    assert.ok(navigation.every(right => right <= 391), 'Shared document navigation remains reachable at 390px');
    await select('goal');
    const narrow = await host.evaluate(node => {
        const boxes = ['.pc-recomb-goal', '.pc-recomb-picker'].map(selector => {
            const box = node.querySelector(selector).getBoundingClientRect(); return {x: box.x, right: box.right, top: box.top};
        });
        const overflow = [...node.querySelectorAll('*')].map(child => ({
            tag: child.tagName, class: child.className, right: child.getBoundingClientRect().right,
        })).filter(child => child.right > node.getBoundingClientRect().right + 1);
        return {width: node.clientWidth, scroll: node.scrollWidth, top: node.scrollTop, boxes, overflow};
    });
    assert.ok(narrow.scroll <= narrow.width + 1, `Calculator has no horizontal mobile overflow: ${JSON.stringify(narrow)}`);
    assert.ok(narrow.boxes.every(box => box.x >= 0 && box.right <= 391));
    assert.ok(narrow.boxes.every((box, index) => index === 0 || box.top > narrow.boxes[index - 1].top));
    assert.equal(narrow.top, 0, 'Mobile initial view starts at the logical top');
    assert.equal(await host.locator('.pc-recomb-a:visible, .pc-recomb-b:visible').count(), 0);
    assert.equal(await host.locator('.pc-recomb-mobile-tabs button:visible').count(), 3);
    await capture(page, 'recombination-narrow-overview');
    for (const focus of ['a', 'b', 'goal']) {
        await select(focus);
        assert.equal(await host.locator(`[data-recomb-card="${focus}"]:visible`).count(), 1);
        assert.equal(await host.evaluate(node => node.scrollTop), 0);
        assert.equal(await host.locator(`[data-recomb-item="${focus}"] .pc-mod-slot.is-filled`).count(), 6);
    }
    await host.locator('.pc-recomb-picker').scrollIntoViewIfNeeded(); await capture(page, 'recombination-narrow-picker');
    await page.reload(); await ready();
    assert.equal(await host.locator('[data-recomb-item="a"] .pc-mod-slot.is-filled').count(), 6);
    assert.match(await host.locator('[data-recomb-item="b"] .pc-item-title-line').textContent(), /iLvl 86/);
    assert.equal(await host.locator('[data-recomb-item="goal"] .pc-mod-slot.is-filled').count(), 6);
    assert.equal(await host.locator('.pc-recomb-answer').count(), 0, 'Recovered drafts do not recover stale probability results');
    return {result: 'passed', native: 'real immutable WASM pair projection', screenshots: 8, populated, desktopLayouts: desktops,
        controls: ['A/B/goal focus', 'shared picker', 'base toggle', 'equal and different bases', 'unsupported input',
            'atomic invalid edit', 'stale delivery', 'cancel', 'repeated picker', 'keyboard focus', 'six-affix desktop fit', 'collapsed carrier details', 'mobile logical overview', 'narrow layout', 'reload recovery']};
}
