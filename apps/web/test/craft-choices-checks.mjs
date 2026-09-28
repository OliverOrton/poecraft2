import assert from 'node:assert/strict';

const ready = page => page.waitForFunction(() => document.querySelector('pc-emulator')?.item && !document.querySelector('pc-emulator')?.busy);
const choice = (page, host, name, label) => page.locator(`${host} [data-mechanic="${name}"]`).filter({hasText: new RegExp(`^${label}$`)});
const rows = page => page.locator('pc-emulator .pc-mod-explicit-ledger .pc-mod-slot').evaluateAll(elements => elements.map(element => element.getBoundingClientRect().height));

export async function checkCraftChoices(page) {
    await ready(page);
    const heights = await rows(page);
    assert.ok(heights.length > 0 && heights.every(height => height === 72));
    const cursor = await page.evaluate(() => document.querySelector('pc-emulator').undoHistory.cursor);
    await page.evaluate(() => {
        const emulator = document.querySelector('pc-emulator');
        window.craftChoiceCalls = [];
        const apply = emulator.client.apply.bind(emulator.client);
        emulator.client.apply = (...args) => {
            window.craftChoiceCalls.push(structuredClone(args[2]));
            return apply(...args);
        };
    });
    const apply = async action => {
        await page.locator(`pc-emulator [data-config-action="${action}"]:not(:disabled)`).click();
        await ready(page);
    };
    const lastCall = () => page.evaluate(() => window.craftChoiceCalls.at(-1));
    await page.locator('pc-emulator [data-craft-panel="essence"]').click();
    await choice(page, 'pc-emulator', 'essence-type', 'Woe').click();
    const screaming = choice(page, 'pc-emulator', 'essence-key', 'Screaming');
    await screaming.click();
    assert.equal(await screaming.getAttribute('aria-pressed'), 'true');
    assert.equal(await page.evaluate(() => window.craftChoiceCalls.length), 0, 'Selecting a material must not craft');
    await screaming.hover();
    await page.getByRole('tooltip').waitFor();
    assert.match(await page.getByRole('tooltip').innerText(), /Body Armour/);
    assert.match(await page.getByRole('tooltip').innerText(), /50–61 to maximum Energy Shield/);
    assert.doesNotMatch(await page.getByRole('tooltip').innerText(), /\(50-61\)/);
    await page.keyboard.press('Escape');
    assert.equal(await page.getByRole('tooltip').count(), 0);
    await apply('essence');
    assert.deepEqual(await lastCall(), {type: 'essence', essence: 'Metadata/Items/Currency/CurrencyEssenceWoe5'});
    assert.deepEqual(await rows(page), heights);
    await choice(page, 'pc-emulator', 'essence-type', 'Anger').click();
    const tiers = await page.locator('pc-emulator [data-mechanic="essence-key"]').evaluateAll(elements => elements.map(element => element.dataset.value));
    assert.ok(tiers.length > 0 && tiers.every(key => key.includes('EssenceAnger')));
    assert.equal(await page.locator('pc-emulator [data-mechanic="essence-key"][aria-pressed="true"]').count(), 1);

    await page.locator('pc-emulator [data-craft-panel="fossil"]').click();
    const pristine = choice(page, 'pc-emulator', 'fossil', 'Pristine');
    await pristine.hover();
    assert.match(await page.getByRole('tooltip').innerText(), /More Life modifiers/);
    assert.match(await page.getByRole('tooltip').innerText(), /No Defence modifiers/);
    for (const label of ['Pristine', 'Jagged', 'Dense', 'Corroded']) await choice(page, 'pc-emulator', 'fossil', label).click();
    const fifth = choice(page, 'pc-emulator', 'fossil', 'Aberrant');
    assert.equal(await fifth.getAttribute('aria-disabled'), 'true');
    await fifth.click({force: true});
    assert.equal(await page.locator('pc-emulator [data-mechanic="fossil"][aria-pressed="true"]').count(), 4);
    for (const label of ['Dense', 'Corroded']) await choice(page, 'pc-emulator', 'fossil', label).click();
    await apply('fossil');
    const fossilCall = await lastCall();
    assert.equal(fossilCall.type, 'fossil');
    assert.deepEqual(fossilCall.fossils, ['Metadata/Items/Currency/CurrencyDelveCraftingLife', 'Metadata/Items/Currency/CurrencyDelveCraftingPhysical']);
    assert.equal(await page.locator('pc-emulator [data-config-action="fossil"] img').count(), 0);
    assert.deepEqual(await rows(page), heights);

    await page.locator('pc-emulator [data-craft-panel="harvest"]').click();
    await choice(page, 'pc-emulator', 'harvest-reforge-tag', 'Fire').click();
    await apply('harvest_reforge');
    assert.deepEqual(await lastCall(), {type: 'harvest_reforge', target_tag: 'fire'});
    await choice(page, 'pc-emulator', 'resist-from', 'Fire').click();
    await choice(page, 'pc-emulator', 'resist-to', 'Cold').click();
    await apply('harvest_resist');
    assert.deepEqual(await lastCall(), {type: 'harvest_resist', source_tag: 'fire', target_tag: 'cold'});
    await choice(page, 'pc-emulator', 'harvest-augment-tag', 'Life').click();
    await apply('harvest_augment');
    assert.deepEqual(await lastCall(), {type: 'harvest_augment', target_tag: 'life'});
    await page.waitForFunction(() => Array.from(document.querySelectorAll('pc-emulator .pc-material-choice img')).every(image => image.complete && image.naturalWidth > 0));
    assert.equal(await page.locator('pc-emulator .pc-material-choice:not(:has(img))').count(), 0);

    await page.locator('pc-emulator [data-craft-panel="influenced"]').click();
    await choice(page, 'pc-emulator', 'influence', 'Hunter').click();
    await apply('influence_exalt');
    assert.deepEqual(await lastCall(), {type: 'influence_exalt', influence: 'hunter'});
    assert.equal(await page.locator('pc-emulator [data-config-action] img').count(), 0);
    assert.equal(await page.locator('pc-emulator .pc-material-choice:not(:has(img))').count(), 0);
    await page.locator(`pc-emulator [data-history-index="${cursor}"]`).click();
    await ready(page);
    assert.deepEqual(await rows(page), heights);
    await page.locator('pc-emulator [data-craft-panel="basic"]').click();
}

/** Without a goal there is no native solve; only explicit buttons change the chosen action. */
export async function checkCalculatorChoices(page) {
    await page.waitForFunction(() => document.querySelector('pc-calculator')?.item && !document.querySelector('pc-calculator')?.busy);
    await page.locator('pc-calculator [data-craft-panel="fossil"]').click();
    const before = await page.evaluate(() => document.querySelector('pc-calculator').actionId);
    await choice(page, 'pc-calculator', 'fossil', 'Pristine').click();
    assert.equal(await page.evaluate(() => document.querySelector('pc-calculator').actionId), before);
    await page.locator('pc-calculator [data-derive-action="fossil"]:not(:disabled)').click();
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    assert.equal(await page.evaluate(() => document.querySelector('pc-calculator').actionId), 'fossil:Metadata/Items/Currency/CurrencyDelveCraftingLife');
    await choice(page, 'pc-calculator', 'fossil', 'Jagged').click();
    assert.equal(await page.evaluate(() => document.querySelector('pc-calculator').fossilKeys.length), 1, 'Staged choices must not alter the evaluated loadout');
    await page.locator('pc-calculator [data-derive-action="fossil"]:not(:disabled)').click();
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    assert.equal(await page.evaluate(() => document.querySelector('pc-calculator').fossilKeys.length), 2);
    await page.locator('pc-calculator [data-craft-panel="essence"]').click();
    await choice(page, 'pc-calculator', 'essence-type', 'Woe').click();
    await choice(page, 'pc-calculator', 'essence-key', 'Screaming').click();
    await page.locator('pc-calculator [data-derive-action="essence"]:not(:disabled)').click();
    await page.waitForFunction(() => !document.querySelector('pc-calculator')?.busy);
    assert.equal(await page.evaluate(() => document.querySelector('pc-calculator').actionId), 'essence:Metadata/Items/Currency/CurrencyEssenceWoe5');
    await page.reload();
    await page.waitForFunction(() => document.querySelector('pc-calculator')?.item && !document.querySelector('pc-calculator')?.busy);
    assert.equal(await choice(page, 'pc-calculator', 'essence-type', 'Woe').getAttribute('aria-pressed'), 'true');
    assert.equal(await choice(page, 'pc-calculator', 'essence-key', 'Screaming').getAttribute('aria-pressed'), 'true');
    await page.locator('pc-calculator [data-craft-panel="basic"]').click();
}
