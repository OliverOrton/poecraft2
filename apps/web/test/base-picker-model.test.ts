import assert from "node:assert/strict";

import {
    basePickerAttributeCombo,
    compareBasePickerBases,
    supportedBasePickerBases,
} from "../src/app/base-picker-model";
import type { BaseInfo } from "../src/app/engine-protocol";

const base = (
    path: string,
    name: string,
    dropLevel: number,
    support = 0,
): BaseInfo => ({
    path,
    name,
    item_class_key: "Body Armour",
    drop_level: dropLevel,
    support,
});

{
    const cases = [
        ["BodyArmours/BodyInt17", "Int"],
        ["BodyArmours/BodyStrDexInt1", "StrDexInt"],
        ["Helmets/HelmetStrIntRitual3", "StrInt"], // Archdemon Crown
        ["Helmets/HelmetDexIntRitual3", "DexInt"], // Blizzard Crown
        ["Helmets/HelmetStrDexRitual3", "StrDex"], // Penitent Mask
        ["Gloves/GlovesIntRitual3", "Int"], // Nexus Gloves
        ["Boots/BootsDexRitual3", "Dex"], // Stormrider Boots
        ["Boots/BootsStrRitual3", "Str"], // Brimstone Treads
        ["Shields/ShieldDexE3", "Dex"], // Cold-attuned Buckler
        ["Gloves/GlovesAtlasStrInt", "StrInt"], // Apothecary's Gloves
        ["Helmets/HelmetAtlas1", null], // No attribute encoded in this path
    ] as const;
    for (const [path, expected] of cases) {
        assert.equal(basePickerAttributeCombo(`Metadata/Items/Armours/${path}`), expected, path);
    }
    assert.equal(basePickerAttributeCombo("Metadata/Items/Jewels/JewelInt"), null,
        "attribute jewels must not acquire armour defence filters");
    console.log("  ok - Ritual and variant armour bases retain their full defence category");
}

{
    const ordered = [
        base("Metadata/Z", "Same", 68),
        base("Metadata/UnknownB", "Beta", -1),
        base("Metadata/High", "High", 84),
        base("Metadata/A", "Same", 68),
        base("Metadata/Mid", "Mid", 70),
        base("Metadata/UnknownA", "Alpha", -1),
    ].sort(compareBasePickerBases);

    assert.deepEqual(
        ordered.map((entry) => entry.path),
        [
            "Metadata/High",
            "Metadata/Mid",
            "Metadata/A",
            "Metadata/Z",
            "Metadata/UnknownA",
            "Metadata/UnknownB",
        ],
    );
    console.log("  ok - base order is level descending, unknown last, then name/path");
}

{
    const supported = supportedBasePickerBases([
        base("Metadata/Unsupported", "Unsupported", 100, 1),
        base("Metadata/Low", "Low", 1),
        base("Metadata/High", "High", 84),
        base("Metadata/Nameless", "", 90),
    ]);
    assert.deepEqual(
        supported.map((entry) => entry.path),
        ["Metadata/High", "Metadata/Low"],
    );
    console.log("  ok - the shared picker orders only supported, named bases");
}
