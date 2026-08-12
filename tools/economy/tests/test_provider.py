from __future__ import annotations

import json
import unittest

from poecraft_economy.provider import CATEGORY_CAPABILITIES, PoeNinjaAdapter


class ProviderAdapterTests(unittest.TestCase):
    def test_capabilities_are_product_driven(self) -> None:
        capabilities = {item.name: item for item in CATEGORY_CAPABILITIES}
        self.assertEqual(
            [item.name for item in CATEGORY_CAPABILITIES if item.required],
            ["Currency", "Fossil", "Resonator", "Essence", "Beast"],
        )
        self.assertEqual(capabilities["Fossil"].surface, "exchange")
        self.assertEqual(capabilities["Beast"].surface, "stash")
        self.assertFalse(capabilities["BaseType"].required)

    def test_exchange_and_stash_shapes_stop_at_adapter_boundary(self) -> None:
        adapter = PoeNinjaAdapter()
        exchange = {
            "core": {"items": [], "rates": {}, "primary": "chaos", "secondary": "divine"},
            "lines": [{"id": "chaos", "primaryValue": 1, "volumePrimaryValue": 99}],
            "items": [{"id": "chaos", "name": "Chaos Orb"}],
        }
        row = adapter.parse_rows(
            CATEGORY_CAPABILITIES[0], json.dumps(exchange).encode()
        )[0]
        self.assertEqual(row.item_id, "chaos")
        self.assertEqual(row.chaos_value, 1)
        self.assertEqual(row.confidence, "low")

        stash = {
            "lines": [
                {
                    "id": 42,
                    "detailsId": "craicic-croaker",
                    "name": "Craicic Croaker",
                    "chaosValue": 75.5,
                    "count": 8,
                    "listingCount": 19,
                }
            ]
        }
        beast = next(item for item in CATEGORY_CAPABILITIES if item.name == "Beast")
        row = adapter.parse_rows(beast, json.dumps(stash).encode())[0]
        self.assertEqual(row.row_key, "craicic-croaker")
        self.assertEqual(row.item_id, "42")
        self.assertEqual(row.confidence, "low")

    def test_malformed_or_non_chaos_exchange_is_rejected(self) -> None:
        adapter = PoeNinjaAdapter()
        payload = {
            "core": {"primary": "divine"},
            "lines": [],
            "items": [],
        }
        with self.assertRaisesRegex(ValueError, "chaos-primary"):
            adapter.parse_rows(
                CATEGORY_CAPABILITIES[0], json.dumps(payload).encode()
            )


if __name__ == "__main__":
    unittest.main()
