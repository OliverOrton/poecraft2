from __future__ import annotations

from copy import deepcopy
from dataclasses import asdict, FrozenInstanceError
import hashlib
import json
import sqlite3

import pytest

from poecraft_ingest.compiled_data import compile_engine_data, validate_engine_data
from poecraft_ingest.engine_selection import (
    resolve_base_selection,
    resolve_cluster_catalog_configuration,
)
from poecraft_ingest.repo_loader import load_source_snapshot
from poecraft_ingest.validation import (
    UnsupportedFeatureError,
    query_normal_rollable_mods,
)
from poecraft_ingest.write_sqlite import build_database
from tests.test_ingest import SCHEMA_PATH, _fixture_sources, _write_json


BASE = "Metadata/Items/Jewels/JewelPassiveTreeExpansion"
SIZES = {"Large": (8, 12), "Medium": (4, 6), "Small": (2, 3)}


def _cluster_sources():
    source = _fixture_sources()
    source["cluster_jewels.json"] = {}
    large_base = source["base_items.json"][BASE + "Large"]
    for size, (minimum, maximum) in SIZES.items():
        base = deepcopy(large_base)
        base["name"] = size + " Cluster Jewel"
        base["tags"] = ["jewel", "expansion_jewel_" + size.lower(), "default"]
        source["base_items.json"][BASE + size] = base
        source["cluster_jewels.json"][BASE + size] = {
            "name": base["name"], "size": size,
            "min_skills": minimum, "max_skills": maximum,
            "total_indices": 6 if size == "Small" else 12,
            "notable_indices": [4], "socket_indices": [4],
            "small_indices": [0, 4, 2],
            "passive_skills": [{
                "id": size.lower() + "_passive", "tag": size.lower() + "_tag",
                "name": size + " passive", "stats": {"fixture_stat": 10},
                "stat_text": ["fixture passive text"],
            }],
        }
    source["cluster_jewels.json"][BASE + "Small"]["passive_skills"] = [
        {"id": "attack_block", "tag": "shared_block", "name": "Attack block",
         "stats": {"attack_block": 2}, "stat_text": ["Attack block text"]},
        {"id": "spell_block", "tag": "shared_block", "name": "Spell block",
         "stats": {"spell_block": 2}, "stat_text": ["Spell block text"]},
    ]
    source["cluster_jewels.json"][BASE + "Medium"]["passive_skills"].append({
        "id": "legacy_passive", "tag": "old_do_not_use_legacy_passive",
        "name": "Legacy passive", "stats": {"legacy_stat": 2}, "stat_text": [],
    })
    notable = deepcopy(source["mods.json"]["LifePrefix1"])
    notable.update({
        "domain": "affliction_jewel", "name": "Notable", "type": "FixtureNotable",
        "groups": ["FixtureNotable"], "required_level": 75,
        "adds_tags": ["has_affliction_notable"],
        "spawn_weights": [{"tag": "shared_block", "weight": 49},
                          {"tag": "default", "weight": 0}],
        "generation_weights": [
            {"tag": "expansion_jewel_large", "weight": 100},
            {"tag": "expansion_jewel_medium", "weight": 100},
            {"tag": "has_affliction_notable", "weight": 0},
            {"tag": "expansion_jewel_small", "weight": 100},
            {"tag": "default", "weight": 0},
        ],
        "stats": [{"id": "linked_notable_stat", "min": 1, "max": 1}],
    })
    source["mods.json"]["FixtureNotable"] = notable
    source["cluster_jewel_notables.json"] = [
        {"id": "linked_notable", "name": "Linked", "jewel_stat": "linked_notable_stat"},
        {"id": "unlinked_notable", "name": "Unlinked", "jewel_stat": "absent_notable_stat"},
    ]
    return source


@pytest.fixture(scope="module")
def catalog(tmp_path_factory):
    root = tmp_path_factory.mktemp("cluster-catalog")
    source_path = root / "source"
    source_path.mkdir()
    source = _cluster_sources()
    for name, payload in source.items():
        _write_json(source_path / name, payload)
    database = root / "catalog.db"
    build_database(load_source_snapshot(source_path), database, SCHEMA_PATH)
    artifact = root / "compiled"
    manifest = compile_engine_data(database, artifact, generated_at_utc="2026-10-02T00:00:00Z")
    report = validate_engine_data(database, artifact)
    assert report["ok"], report["errors"]
    before = hashlib.sha256(database.read_bytes()).hexdigest()
    with sqlite3.connect(database.as_uri() + "?mode=ro", uri=True) as connection:
        yield {
            "connection": connection, "source": source, "database": database,
            "manifest": manifest,
            "data": json.loads((artifact / "game-data.json").read_text()),
            "strings": json.loads((artifact / "strings.json").read_text())["strings"],
        }
    connection.close()
    assert hashlib.sha256(database.read_bytes()).hexdigest() == before


def _resolve(catalog, size="Small", key="attack_block", count=2):
    return resolve_cluster_catalog_configuration(
        catalog["connection"], BASE + size, key, count,
    )


@pytest.mark.parametrize("size", SIZES)
def test_source_configuration_bounds_and_layout_survive_compilation(catalog, size):
    source = catalog["source"]["cluster_jewels.json"][BASE + size]
    data, strings = catalog["data"], catalog["strings"]
    clusters = data["cluster_jewels"]
    position = [strings[sid] for sid in clusters["key_string_ids"]].index(BASE + size)
    for field in ("min_skills", "max_skills", "total_indices"):
        assert clusters[field][position] == source[field]
    for field in ("notable_indices", "socket_indices", "small_indices"):
        assert json.loads(strings[clusters[field + "_json_string_ids"][position]]) == source[field]
    for count in range(source["min_skills"], source["max_skills"] + 1):
        for passive in source["passive_skills"]:
            result = _resolve(catalog, size, passive["id"], count)
            assert (result.base_metadata_path, result.passive_key, result.passive_count) == (BASE + size, passive["id"], count)
            assert result.size == size
            assert (result.min_skills, result.max_skills) == SIZES[size]
            assert result.total_indices == source["total_indices"]
            for field in ("notable_indices", "socket_indices", "small_indices"):
                assert getattr(result, field) == tuple(source[field])
            assert result.passive_tag == passive["tag"]
            assert dict(result.passive_stats) == passive["stats"]
            assert result.passive_stat_text == tuple(passive["stat_text"])
            assert result.session_support == "cluster_unsupported"
    lo, hi = clusters["passive_offsets"][position:position + 2]
    for index, passive in zip(range(lo, hi), source["passive_skills"], strict=True):
        assert strings[clusters["passive_key_string_ids"][index]] == passive["id"]
        assert json.loads(strings[clusters["passive_stats_json_string_ids"][index]]) == passive["stats"]
    bases = data["base_items"]
    base_position = [strings[sid] for sid in bases["metadata_path_string_ids"]].index(BASE + size)
    assert bases["session_support_codes"][base_position] == 1
    assert bases["flags"][base_position] == 1


def test_shared_tag_never_collapses_stable_passive_identity(catalog):
    attack = _resolve(catalog)
    spell = _resolve(catalog, key="spell_block")
    assert attack.passive_tag == spell.passive_tag == "shared_block"
    assert attack.passive_key != spell.passive_key
    assert attack.passive_stats != spell.passive_stats
    assert attack != spell
    with pytest.raises(ValueError, match="passive key not found"):
        _resolve(catalog, key="shared_block")
    encoded = json.loads(json.dumps(asdict(attack)))
    assert encoded["passive_key"] == "attack_block"
    assert encoded["session_support"] == "cluster_unsupported"
    with pytest.raises(FrozenInstanceError):
        attack.passive_count = 3


def test_legacy_records_remain_preserved_but_unsupported(catalog):
    result = _resolve(catalog, "Medium", "legacy_passive", 4)
    assert result.passive_tag == "old_do_not_use_legacy_passive"
    assert result.session_support == "cluster_unsupported"


@pytest.mark.parametrize("size,key,count", [
    ("Large", "attack_block", 8), ("Medium", "large_passive", 4),
    ("Small", "medium_passive", 2), ("Small", "Attack block", 2),
    ("Small", "missing", 2), ("Small", "linked_notable", 2),
    ("Small", "unlinked_notable", 2),
])
def test_passives_must_belong_to_the_selected_size(catalog, size, key, count):
    with pytest.raises(ValueError, match="passive key not found"):
        _resolve(catalog, size, key, count)


@pytest.mark.parametrize("count", [None, True, False, 2.0, "2", [], {}, float("nan")])
def test_noninteger_counts_are_not_coerced(catalog, count):
    with pytest.raises(ValueError, match="must be an integer"):
        _resolve(catalog, count=count)


@pytest.mark.parametrize("size", SIZES)
def test_out_of_bounds_counts_are_refused(catalog, size):
    minimum, maximum = SIZES[size]
    key = "attack_block" if size == "Small" else size.lower() + "_passive"
    for count in (-1, 0, minimum - 1, maximum + 1):
        with pytest.raises(ValueError, match="must be between"):
            _resolve(catalog, size, key, count)


@pytest.mark.parametrize("key", [None, "", 1, "Large Cluster Jewel", BASE + "Unknown", "Metadata/Items/Armours/BodyArmours/TestArmour"])
def test_only_exact_cluster_base_keys_resolve(catalog, key):
    with pytest.raises(ValueError):
        resolve_cluster_catalog_configuration(catalog["connection"], key, "large_passive", 8)


@pytest.mark.parametrize("key", [None, "", 1])
def test_passive_keys_must_be_nonempty_strings(catalog, key):
    with pytest.raises(ValueError, match="nonempty stable key"):
        _resolve(catalog, key=key)


def test_notable_catalogue_is_not_a_rollable_mod_catalogue(catalog):
    connection = catalog["connection"]
    rows = connection.execute(
        """
        SELECT n.key, m.key FROM cluster_notable AS n
        LEFT JOIN mod_stat AS s ON s.stat_key = n.jewel_stat_key
        LEFT JOIN mod AS m ON m.mod_id = s.mod_id
        ORDER BY n.key
        """
    ).fetchall()
    assert rows == [("linked_notable", "FixtureNotable"), ("unlinked_notable", None)]
    data, strings = catalog["data"], catalog["strings"]
    assert data["cluster_notables"]["count"] == 2
    assert "unlinked_notable" not in [strings[sid] for sid in data["mods"]["key_string_ids"]]
    # Even a linked notable is not a passive-configuration key.
    for key, _ in rows:
        with pytest.raises(ValueError, match="passive key not found"):
            _resolve(catalog, key=key)


def test_compiler_preserves_notable_generation_order_and_added_tags(catalog):
    data, strings = catalog["data"], catalog["strings"]
    position = [strings[sid] for sid in data["mods"]["key_string_ids"]].index("FixtureNotable")
    tag_names = dict(zip(data["tags"]["global_tag_ids"], (strings[sid] for sid in data["tags"]["name_string_ids"]), strict=True))
    weights = data["generation_weights"]
    lo, hi = weights["offsets"][position:position + 2]
    actual = [{"tag": tag_names[weights["tag_ids"][i]], "weight": weights["weights"][i]} for i in range(lo, hi)]
    assert actual == catalog["source"]["mods.json"]["FixtureNotable"]["generation_weights"]
    adds = data["adds_tags"]
    lo, hi = adds["offsets"][position:position + 2]
    assert [tag_names[tag] for tag in adds["tag_ids"][lo:hi]] == ["has_affliction_notable"]


@pytest.mark.parametrize("size", SIZES)
def test_configuration_resolution_does_not_admit_crafting_sessions(catalog, size):
    connection = catalog["connection"]
    assert connection.row_factory is None
    key = "attack_block" if size == "Small" else size.lower() + "_passive"
    _resolve(catalog, size, key, SIZES[size][0])
    assert connection.row_factory is None
    # The existing session selector expects Row and must keep refusing clusters.
    connection.row_factory = sqlite3.Row
    try:
        with pytest.raises(UnsupportedFeatureError, match="cluster_jewel_session"):
            resolve_base_selection(connection, BASE + size, 84)
    finally:
        connection.row_factory = None
    with pytest.raises(UnsupportedFeatureError, match="cluster_jewel_session"):
        query_normal_rollable_mods(catalog["database"], BASE + size, 84)
