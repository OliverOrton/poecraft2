import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';

function luminance(value) {
    const rgb = value.match(/[\d.]+/g).slice(0, 3).map(Number).map(x => {
        const channel = x / 255;
        return channel <= .04045 ? channel / 12.92 : ((channel + .055) / 1.055) ** 2.4;
    });
    return .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2];
}
function contrast(a, b) {
    const x = luminance(a), y = luminance(b);
    return (Math.max(x, y) + .05) / (Math.min(x, y) + .05);
}

/** The installed dock owns layout; its rendered surfaces share the C2 tokens. */
export async function checkDockTheme(page) {
    await page.evaluate(() => document.fonts.ready);
    const measured = await page.locator('pc-workspace').evaluate(workspace => {
        const probe = document.createElement('span'); workspace.append(probe);
        const color = token => { probe.style.color = `var(${token})`; return getComputedStyle(probe).color; };
        const expected = {page: color('--pc-surface-page'), panel: color('--pc-surface-panel')};
        probe.remove();
        const groups = [...workspace.querySelectorAll('.dv-groupview')].map(group => {
            const strip = group.querySelector('.dv-tabs-and-actions-container');
            return {page: getComputedStyle(group).backgroundColor, panel: getComputedStyle(strip).backgroundColor,
                stripHeight: strip.getBoundingClientRect().height,
                tabs: [...group.querySelectorAll('.dv-tabs-container > .dv-tab')].map(tab => {
                    const style = getComputedStyle(tab);
                    return {active: tab.classList.contains('dv-active-tab'), background: style.backgroundColor,
                        text: style.color, radius: style.borderRadius, height: tab.getBoundingClientRect().height,
                        fontSize: style.fontSize};
                })};
        });
        return {expected, groups};
    });
    assert.ok(measured.groups.length > 0);
    const textContrasts = [];
    for (const group of measured.groups) {
        assert.equal(group.page, measured.expected.page);
        assert.equal(group.panel, measured.expected.panel);
        assert.equal(group.stripHeight, 35, 'Joined document strips retain their installed geometry');
        assert.ok(group.tabs.length > 0);
        for (const tab of group.tabs) {
            assert.equal(tab.background, tab.active ? measured.expected.page : measured.expected.panel);
            assert.equal(tab.radius, '0px'); assert.equal(tab.height, 35);
            assert.equal(tab.fontSize, '14px');
            textContrasts.push(contrast(tab.text, tab.background));
        }
    }
    assert.ok(textContrasts.every(value => value >= 4.5), 'Active and inactive document tab text contrast');
    return {page: measured.expected.page, panel: measured.expected.panel,
        groups: measured.groups.length, tabs: textContrasts.length, stripHeight: 35,
        squareJoinedTabs: true, fontSize: 14, minimumTextContrast: Math.min(...textContrasts)};
}

/** Uses the existing real app fixtures/runner; no extra server or native workload. */
export async function checkWorkbenchPresentation(page) {
    await page.locator('pc-emulator [data-craft-panel="essence"]').click();
    await page.locator('pc-emulator [data-mechanic="essence-type"]').filter({hasText: /^Woe$/}).click();
    await page.locator('pc-emulator [data-mechanic="essence-key"]').filter({hasText: /^Screaming$/}).click();
    await page.evaluate(() => document.fonts.ready);
    const primary = page.locator('pc-emulator [data-config-action="essence"]');
    await primary.locator('xpath=self::button[not(@disabled)]').waitFor();
    const styles = await primary.evaluate(element => {
        const style = getComputedStyle(element), root = getComputedStyle(document.documentElement);
        const probe = document.createElement('span');
        document.body.append(probe);
        const color = token => { probe.style.color = root.getPropertyValue(token); return getComputedStyle(probe).color; };
        const result = {text: style.color, fill: style.backgroundColor, radius: style.borderRadius,
            page: color('--pc-surface-page'), panel: color('--pc-surface-panel'), card: color('--pc-surface-card'),
            body: color('--pc-text-body'), muted: color('--pc-text-muted'), focus: color('--pc-focus'),
            boundary: color('--pc-border-control'), field: color('--pc-surface-field')};
        probe.remove();
        return result;
    });
    const dockTheme = await checkDockTheme(page);
    assert.equal(styles.radius, '4px');
    assert.ok(contrast(styles.text, styles.fill) >= 4.5, 'Execution text contrast');
    for (const surface of [styles.page, styles.panel, styles.card]) {
        assert.ok(contrast(styles.body, surface) >= 4.5);
        assert.ok(contrast(styles.muted, surface) >= 4.5);
        assert.ok(contrast(styles.focus, surface) >= 3);
    }
    assert.ok(contrast(styles.boundary, styles.field) >= 3);
    await primary.hover();
    const hover = await primary.evaluate(element => ({text: getComputedStyle(element).color, fill: getComputedStyle(element).backgroundColor}));
    assert.ok(contrast(hover.text, hover.fill) >= 4.5);
    await primary.focus();
    await page.keyboard.press('Tab');
    await page.keyboard.press('Shift+Tab');
    assert.equal(await primary.evaluate(element => element.matches(':focus-visible')), true);
    assert.equal(await primary.evaluate(element => getComputedStyle(element).outlineWidth), '2px');
    const slots = await page.locator('pc-emulator .pc-mod-explicit-ledger .pc-mod-slot').evaluateAll(elements =>
        elements.map(element => ({height: element.getBoundingClientRect().height, radius: getComputedStyle(element).borderRadius})));
    assert.ok(slots.every(slot => slot.height === 72 && slot.radius === '0px'));
    assert.ok(await page.evaluate(() => document.fonts.check('400 14px "Noto Sans"') && document.fonts.check('700 14px "Noto Sans"')));
    return {primaryContrast: contrast(styles.text, styles.fill), hoverContrast: contrast(hover.text, hover.fill),
        mutedCardContrast: contrast(styles.muted, styles.card), focusCardContrast: contrast(styles.focus, styles.card),
        fieldBoundaryContrast: contrast(styles.boundary, styles.field), keyboardFocus: true, stableSquareSlots: true,
        dockTheme};
}

/** Optional evidence capture during the existing smoke run, in its fresh profile. */
export async function captureUiCheckpoint(page, name) {
    const directory = process.env.POECRAFT_UI_CAPTURE_DIR;
    if (!directory) return;
    const selected = process.env.POECRAFT_UI_CAPTURE_ONLY?.split(',').map(value => value.trim());
    if (selected && !selected.includes(name)) return;
    await mkdir(directory, {recursive: true});
    const viewport = page.viewportSize();
    await page.setViewportSize({width: 1536, height: 864});
    await page.evaluate(() => document.fonts.ready);
    await page.screenshot({path: resolve(directory, `${page.context().browser().browserType().name()}-${name}.png`)});
    await page.setViewportSize(viewport);
}

/** Exercise the shipped shared card in a real browser with a native-owned clone.
 * Goal fixtures explicitly provide their constraints; no live goal is modified.
 */
export async function checkItemPresentationSemantics(page) {
    await page.evaluate(async () => {
        window.presentationReturnFocus = document.activeElement;
        const emulator = document.querySelector('pc-emulator');
        const source = emulator.querySelector('pc-mod-list').model;
        const clone = await emulator.client.cloneItem(emulator.item);
        try {
            await emulator.client.editItem(clone, emulator.session, {influence_bits: 32});
            const info = await emulator.client.itemInfo(clone, emulator.session);
            if (Number(info.generic_influence_bits) !== 32) throw new Error('Native Shaper fixture was not retained');
            window.itemPresentationFixture = {...structuredClone(source),
                influences: ['Shaper'], itemFlags: Number(info.item_flags), lifecycle: Number(info.lifecycle),
                memoryStrands: Number(info.memory_strands)};
        } finally { await emulator.client.closeItem(clone); }
        const host = document.createElement('aside');
        host.id = 'item-presentation-qualification';
        host.setAttribute('aria-label', 'Item presentation qualification fixture');
        host.style.cssText = 'position:fixed;inset:16px auto 16px 16px;width:460px;overflow:auto;z-index:1000;background:var(--pc-surface-panel);padding:12px;border:1px solid var(--pc-border)';
        const card = document.createElement('pc-mod-list');
        host.append(card); document.body.append(host);
        card.setModel({...window.itemPresentationFixture, readOnly: true});
    });
    const host = page.locator('#item-presentation-qualification');
    const card = host.locator('pc-mod-list');
    try {
        assert.equal(await card.locator('[data-influence-context="actual"]').innerText(), 'Shaper');
        assert.equal(await card.locator('[data-influence-context="required"], [data-influence-context="exact"]').count(), 0);
        assert.equal(await card.locator('.pc-item-fracture-mod, .pc-item-remove-mod, .pc-item-add-mod').count(), 0);
        await card.locator('.pc-item-properties summary').click();
        assert.equal(await card.getByRole('checkbox', {name: 'Shaper', exact: true}).isDisabled(), true);
        const details = card.locator('.pc-mod-slot.is-filled .pc-mod-slot-content').first();
        await details.focus(); await page.keyboard.press('Tab'); await page.keyboard.press('Shift+Tab');
        assert.equal(await details.evaluate(element => element.matches(':focus-visible')), true);
        assert.equal(await details.getAttribute('tabindex'), '0');
        assert.equal(await details.evaluate(element => getComputedStyle(element).outlineWidth), '2px');
        await page.evaluate(() => {
            const card = document.querySelector('#item-presentation-qualification pc-mod-list');
            card.setModel({...window.itemPresentationFixture, readOnly: false});
            window.presentationFracture = null;
            card.addEventListener('fracture-mod', event => {window.presentationFracture = event.detail;});
        });
        const fracture = card.locator('.pc-item-fracture-mod').first();
        const expected = await fracture.evaluate(element => {
            const row = element.closest('.pc-mod-slot');
            return {key: row.dataset.modKey, modId: Number(row.dataset.modId), side: row.dataset.side};
        });
        await fracture.focus(); await page.keyboard.press('Enter');
        assert.deepEqual(await page.evaluate(() => window.presentationFracture), expected);
        await page.evaluate(() => {
            const source = window.itemPresentationFixture;
            const card = document.querySelector('#item-presentation-qualification pc-mod-list');
            let model = {kind: 'target', baseKey: source.baseKey, baseName: source.baseName,
                itemLevel: source.itemLevel, rarity: source.rarity, maxPrefix: source.maxPrefix, maxSuffix: source.maxSuffix,
                prefixes: [], suffixes: [], otherRequirements: [], implicitInfluences: ['Searing Exarch'],
                properties: {influences: source.properties.influences}};
            card.addEventListener('item-properties-change', event => {
                const edit = event.detail;
                model = {...model, properties: {...model.properties,
                    influenceBits: edit.influence_bits === null ? undefined : edit.influence_bits}};
                card.setModel(model);
            });
            card.setModel(model);
        });
        assert.equal(await card.locator('[data-influence-context="any"]').innerText(), 'Any ordinary influence');
        assert.equal(await card.locator('[data-influence-context="required"]').innerText(), 'Required: Searing Exarch');
        assert.equal(await card.locator('[data-influence-context="actual"]').count(), 0);
        if (!await card.locator('.pc-item-properties').evaluate(element => element.open)) {
            await card.locator('.pc-item-properties summary').click();
        }
        await card.getByRole('checkbox', {name: 'Any influence', exact: true}).uncheck();
        assert.equal(await card.locator('[data-influence-context="exact-none"]').innerText(), 'Exactly: no ordinary influence');
        await card.getByRole('checkbox', {name: 'Shaper', exact: true}).check();
        assert.equal(await card.locator('[data-influence-context="exact"]').innerText(), 'Exactly: Shaper');
        await card.getByRole('checkbox', {name: 'Any influence', exact: true}).check();
        assert.equal(await card.locator('[data-influence-context="exact"], [data-influence-context="exact-none"]').count(), 0);
        assert.equal(await card.locator('[data-influence-context="any"]').count(), 1);
        return {nativeActualInfluence: true, requiredExactAnyDistinct: true, readOnly: true, keyboardFractureIdentity: true};
    } finally {
        await host.evaluate(element => element.remove());
        await page.evaluate(() => {
            window.presentationReturnFocus?.focus();
            delete window.presentationReturnFocus; delete window.itemPresentationFixture; delete window.presentationFracture;
        });
    }
}

/** Source-baseline geometry; new A/B connector qualification belongs to its feature branch. */
export async function checkBuilderPresentation(page) {
    const geometry = await page.locator('pc-strategy-node').evaluateAll(nodes => nodes.map(node => ({
        width: getComputedStyle(node).width,
        ports: [...node.querySelectorAll('.pc-node-port')].map(port => ({
            width: getComputedStyle(port).width, height: getComputedStyle(port).height,
            top: getComputedStyle(port).top, radius: getComputedStyle(port).borderRadius,
        })),
    })));
    assert.ok(geometry.length > 0);
    for (const node of geometry) {
        assert.equal(node.width, '210px');
        for (const port of node.ports) {
            assert.equal(port.width, '14px'); assert.equal(port.height, '14px');
            assert.equal(port.radius, '50%'); assert.equal(port.top, '48px');
        }
    }
    const routing = page.locator('.pc-strategy-inspector .pc-edge-routing');
    const routingWasOpen = await routing.count() ? await routing.evaluate(element => element.open) : null;
    if (routingWasOpen === false) await routing.locator('summary').click();
    const field = page.locator('.pc-strategy-inspector input:visible:not(:disabled)').first();
    const style = await field.evaluate(element => {
        const style = getComputedStyle(element);
        return {text: style.color, fill: style.backgroundColor, boundary: style.borderTopColor};
    });
    assert.ok(contrast(style.text, style.fill) >= 4.5);
    assert.ok(contrast(style.boundary, style.fill) >= 3);
    await field.focus(); await page.keyboard.press('Tab'); await page.keyboard.press('Shift+Tab');
    assert.equal(await field.evaluate(element => element.matches(':focus-visible')), true);
    assert.equal(await field.evaluate(element => getComputedStyle(element).outlineWidth), '2px');
    const clipped = await page.locator('.pc-edge-card-leaf').evaluateAll(leaves => leaves.filter(leaf =>
        leaf.scrollHeight > leaf.closest('.pc-edge-card-row').clientHeight + 1).map(leaf => leaf.textContent));
    assert.deepEqual(clipped, [], 'Edge card text fits its existing row geometry');
    if (routingWasOpen === false) await routing.locator('summary').click();
    return {nodeWidth: 210, portDiameter: 14, sourceBaselinePortTop: 48, keyboardFocus: true, edgeRowsFit: true};
}
