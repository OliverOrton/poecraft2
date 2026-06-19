import assert from "node:assert/strict";

import { itemSnapshotRarity } from "../src/app/workspace/persistence";

assert.equal(
    itemSnapshotRarity({
        base: "base",
        itemLevel: 1,
        rarity: "magic",
        state: { rarity: 2 },
    }),
    "magic",
);
assert.equal(
    itemSnapshotRarity({ base: "base", itemLevel: 1, state: { rarity: 0 } }),
    "normal",
);
assert.equal(
    itemSnapshotRarity({ base: "base", itemLevel: 1, state: { rarity: 1 } }),
    "magic",
);
assert.equal(
    itemSnapshotRarity({ base: "base", itemLevel: 1, state: { rarity: 2 } }),
    "rare",
);

console.log("  ok - saved item rarity survives reopen (including legacy records)");
