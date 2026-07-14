import assert from "node:assert/strict";

import {
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
