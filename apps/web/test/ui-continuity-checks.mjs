import assert from 'node:assert/strict';
import { checkEmulatorHistory, checkStrategyHistory } from './edit-history-checks.mjs';
import { checkCraftChoices, checkCalculatorChoices } from './craft-choices-checks.mjs';

/** Real browser interactions against the packaged UI; no Solver search or Simulator runs. */
export async function checkUiContinuity(page) {
    // Ritual crowns must be selectable in their defence filter, not only All.
    await page.locator('pc-emulator .pc-bp-class').selectOption({label: 'Helmet'});
    await page.locator('pc-emulator .pc-bp-sub').selectOption({label: 'Armour / Energy Shield'});
    await page.locator('pc-emulator .pc-bp-base').selectOption({label: 'Archdemon Crown'});
    assert.equal(await page.locator('pc-emulator .pc-bp-base').inputValue(), 'Metadata/Items/Armours/Helmets/HelmetStrIntRitual3');
    assert.equal(await page.locator('pc-emulator .pc-bp-base option').filter({hasText: /^Blizzard Crown$/}).count(), 0);
    await page.locator('pc-emulator .pc-bp-class').selectOption({label: 'Body Armour'});
    await page.locator('pc-emulator .pc-bp-base').selectOption({label: 'Vaal Regalia'});
    await page.locator('pc-emulator .pc-bp-confirm:not(:disabled)').click();
    await page.locator('pc-emulator [data-simple-action="alchemy"]:not(:disabled)').click();
    await page.waitForFunction(() => document.querySelector('pc-emulator .pc-emu-history')?.textContent.toLowerCase().includes('alchemy'));
    assert.ok(await page.locator('pc-emulator .pc-mod-slot.is-filled').count() >= 4);
    await checkEmulatorHistory(page);
    await checkCraftChoices(page);
    await page.locator('pc-emulator [data-cmd="calculator"]').click();
    await checkCalculatorChoices(page);
    await page.locator('pc-calculator .pc-mod-family-header').first().click();
    await page.locator('pc-calculator .pc-mod-tier-btn').first().click();
    await page.locator('pc-calculator [data-select-action="chaos"]:not(:disabled)').click();
    await page.waitForFunction(() => document.querySelector('.pc-calc-answer-value')?.textContent === '0%');
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    await page.locator('[data-role="allow-extra-modifiers"]').check();
    await page.waitForFunction(() => {
        const result = document.querySelector('.pc-calc-answer-value')?.textContent;
        return result && result !== '0%';
    });
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    assert.equal(await page.locator('[data-solve-target="absolute"]').isDisabled(), true);
    assert.match(await page.locator('.pc-calc-goal-scope').innerText(), /optimal cost is not certified/);
    const odds = await page.locator('.pc-calc-answer-value').innerText();
    // A completed odds update persists before reload. Wait for its native work, then the draft write.
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    await page.reload();
    await page.waitForFunction(() => document.querySelector('.pc-calc-answer-value')?.textContent);
    assert.equal(await page.locator('[data-role="allow-extra-modifiers"]').isChecked(), true);
    assert.equal(await page.locator('.pc-calc-answer-value').innerText(), odds);
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    assert.equal(await page.locator('[data-solve-target="absolute"]').isDisabled(), true);
    assert.equal(await page.locator('[data-solve-target="relative"]').isDisabled(), true);
    assert.match(await page.locator('.pc-calc-goal-scope').innerText(), /optimal cost is not certified/);
    assert.equal(await page.locator('[data-solve-cmd="copy-lab"]').isEnabled(), true);
    assert.doesNotMatch(await page.locator('.pc-calc-solve-header').innerText(), /Define a goal/);

    await page.locator('.pc-tab-title').filter({hasText: /^Untitled$/}).click();
    await page.locator('pc-emulator [data-cmd="strategy"]:not(:disabled)').click();
    await page.locator('pc-strategy-node').waitFor();
    await page.locator('[data-palette="operation:restart"]').dblclick();
    assert.equal(await page.locator('[data-field="operation"]').inputValue(), 'restart');
    await page.locator('[data-palette="operation:bestiary:imprint"]').dblclick();
    assert.equal(await page.locator('[data-field="operation"]').inputValue(), 'bestiary:imprint');
    const start = page.locator('pc-strategy-node').filter({hasText: 'Initial item state'});
    const restart = page.locator('pc-strategy-node').filter({hasText: 'Restart · fresh base'});
    const from = await start.locator('.pc-node-output').boundingBox();
    const to = await restart.locator('.pc-node-input').boundingBox();
    assert.ok(from && to);
    await page.mouse.move(from.x + from.width / 2, from.y + from.height / 2);
    await page.mouse.down();
    await page.mouse.move(to.x + to.width / 2, to.y + to.height / 2, {steps: 8});
    await page.mouse.up();
    await page.locator('pc-condition-editor [data-action="add-group"]').click();
    await page.locator('pc-condition-editor [data-action="add-leaf"][data-path="0"]').click();
    await page.locator('pc-condition-editor [data-action="group-mode"][data-path="0"][data-mode="any"]').click();
    assert.equal(await page.locator('[data-action="group-mode"][data-path="root"][data-mode="all"]').getAttribute('aria-pressed'), 'true');
    assert.equal(await page.locator('[data-action="group-mode"][data-path="0"][data-mode="any"]').getAttribute('aria-pressed'), 'true');
    await page.locator('[data-action="condition-type"][data-path="0.0"]').selectOption('has_mod_family');
    await page.getByRole('button', {name: 'Choose modifiers', exact: true}).click();
    await page.getByRole('searchbox', {name: 'Search modifiers'}).fill('maximum life');
    await page.locator('.pc-modifier-family').first().click();
    await page.locator('[data-action="modifier-tier"]').selectOption('2');
    assert.equal(await page.locator('.pc-cond-tier-badge').innerText(), 'T2');
    await page.getByText('Advanced JSON', {exact: true}).click();
    let condition = JSON.parse(await page.locator('[data-action="json"]').inputValue());
    assert.match(JSON.stringify(condition), /"min_tier":2/);
    assert.match(JSON.stringify(condition), /"type":"any"/);
    await page.locator('[data-action="modifier-tier"]').selectOption('0');
    assert.equal(await page.locator('.pc-cond-tier-badge').innerText(), 'Any');
    condition = JSON.parse(await page.locator('[data-action="json"]').inputValue());
    assert.match(JSON.stringify(condition), /"min_tier":0/);
    await checkStrategyHistory(page);
    await page.locator('.pc-tab-title').filter({hasText: /^Calculator/}).click();
    await page.waitForFunction(() => document.querySelector('.pc-calc-answer-value')?.textContent);
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    assert.equal(await page.locator('[data-role="allow-extra-modifiers"]').isChecked(), true);
    const calculatorTab = page.locator('.pc-tab').filter({has: page.locator('.pc-tab-title').filter({hasText: /^Calculator/})});
    await calculatorTab.locator('.pc-tab-close').click();
    await page.locator('pc-calculator').waitFor({state: 'detached'});
    await page.locator('.pc-tab-title').filter({hasText: /^Untitled$/}).click();
    await page.waitForFunction(() => document.querySelector('pc-emulator')?.item && !document.querySelector('pc-emulator')?.busy);
    assert.ok(await page.locator('pc-emulator .pc-mod-slot.is-filled').count() >= 4);
    await page.waitForFunction(() => {
        const images = Array.from(document.querySelectorAll('pc-emulator .pc-game-art'));
        return images.length > 3 && images.every(image => image.complete && image.naturalWidth > 0);
    });
    const itemMods = await page.locator('pc-emulator .pc-mod-slot.is-filled').allTextContents();
    await page.locator('pc-emulator [data-cmd="save-as"]').click();
    await page.locator('.pc-text-modal input').fill('UI continuity item');
    await page.locator('.pc-text-modal [data-action="accept"]').click();
    await page.waitForFunction(() => document.querySelector('.pc-emu-name')?.textContent === 'Saved: UI continuity item');
    await page.locator('pc-emulator [data-cmd="undo"]:not(:disabled)').click();
    await page.waitForFunction(() => !document.querySelector('pc-emulator')?.busy);
    assert.equal(await page.evaluate(() => document.querySelector('pc-emulator').dirty), true);
    await page.locator('pc-emulator [data-cmd="redo"]:not(:disabled)').click();
    await page.waitForFunction(() => !document.querySelector('pc-emulator')?.busy);
    assert.equal(await page.evaluate(() => document.querySelector('pc-emulator').dirty), false);
    await page.getByRole('button', {name: 'Stash', exact: true}).click();
    const saved = page.locator('.pc-stash-item').filter({hasText: 'UI continuity item'});
    await saved.getByRole('button', {name: 'Import copy', exact: true}).click();
    await page.locator('pc-emulator .pc-mod-slot.is-filled').first().waitFor();
    assert.deepEqual(await page.locator('pc-emulator .pc-mod-slot.is-filled').allTextContents(), itemMods);
    const imported = page.locator('.pc-tab').filter({has: page.locator('.pc-tab-title').filter({hasText: /^Untitled$/})});
    await imported.locator('.pc-tab-close').click();
    await page.locator('.pc-modal [data-choice="cancel"]').click();
    assert.ok(await page.locator('pc-emulator').isVisible());
    await imported.locator('.pc-tab-close').click();
    await page.locator('.pc-modal [data-choice="discard"]').click();
    await imported.waitFor({state: 'detached'});
    return {crafts: true, handoffs: true, coverageGoal: true, draftRecovery: true, restart: true,
        namespacedAction: true, nestedConditions: true, tierAndAnyTier: true, tabLifecycle: true, artwork: true,
        stashRoundTrip: true, dirtyClose: true, emulatorHistory: true, strategyHistory: true, edgeReconnection: true,
        craftChoices: true, craftTooltips: true, stableModSlots: true};
}
