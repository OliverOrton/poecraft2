from __future__ import annotations

from dataclasses import dataclass
import json
import math
import time
from typing import Any, Mapping
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


SOURCE_KEY = "poe-ninja"
SOURCE_NAME = "poe.ninja"
SOURCE_TERMS_URL = "https://poe.ninja/data"
ADAPTER_VERSION = "poe1-2026-07-15"
DEFAULT_BASE_URL = "https://poe.ninja/poe1/api/economy"
DEFAULT_USER_AGENT = (
    "poecraft2-economy-ingest/0.1 "
    "(scheduled economy publisher; https://github.com/)"
)


@dataclass(frozen=True)
class CategoryCapability:
    name: str
    surface: str
    required: bool


# Product capabilities, not a mirror of every upstream category. The endpoint
# surface is adapter data because poe.ninja has moved categories between its
# exchange and stash APIs over time.
CATEGORY_CAPABILITIES = (
    CategoryCapability("Currency", "exchange", True),
    CategoryCapability("Fossil", "exchange", True),
    CategoryCapability("Resonator", "exchange", True),
    CategoryCapability("Essence", "exchange", True),
    CategoryCapability("Beast", "stash", False),
    CategoryCapability("BaseType", "stash", False),
)


@dataclass(frozen=True)
class SourceLeague:
    source_id: str
    display_name: str


@dataclass(frozen=True)
class FetchResult:
    status: int
    body: bytes | None
    etag: str | None
    last_modified: str | None
    cache_control: str | None


@dataclass(frozen=True)
class ParsedRow:
    row_key: str
    item_id: str
    name: str
    category: str
    chaos_value: float
    source_value: float
    reference_currency: str
    listing_count: int | None
    volume: float | None
    confidence: str
    payload: Mapping[str, Any]


def _finite_non_negative(value: Any, field: str) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"{field} must be numeric") from error
    if not math.isfinite(result) or result < 0:
        raise ValueError(f"{field} must be finite and non-negative")
    return result


def _json(body: bytes) -> Any:
    try:
        return json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("provider returned malformed JSON") from error


class PoeNinjaAdapter:
    def __init__(
        self,
        *,
        base_url: str = DEFAULT_BASE_URL,
        user_agent: str = DEFAULT_USER_AGENT,
        timeout: float = 30.0,
        retries: int = 3,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.user_agent = user_agent
        self.timeout = timeout
        self.retries = retries

    def capabilities(self) -> tuple[CategoryCapability, ...]:
        return CATEGORY_CAPABILITIES

    def _request(
        self, url: str, conditional_headers: Mapping[str, str] | None = None
    ) -> FetchResult:
        headers = {
            "Accept": "application/json",
            "User-Agent": self.user_agent,
        }
        headers.update(conditional_headers or {})
        last_error: Exception | None = None
        for attempt in range(self.retries + 1):
            request = Request(url, headers=headers)
            try:
                with urlopen(request, timeout=self.timeout) as response:
                    return FetchResult(
                        int(response.status),
                        response.read(),
                        response.headers.get("ETag"),
                        response.headers.get("Last-Modified"),
                        response.headers.get("Cache-Control"),
                    )
            except HTTPError as error:
                last_error = error
                if error.code == 304:
                    return FetchResult(
                        304,
                        None,
                        error.headers.get("ETag"),
                        error.headers.get("Last-Modified"),
                        error.headers.get("Cache-Control"),
                    )
                if error.code not in (408, 425, 429, 500, 502, 503, 504):
                    raise
                retry_after = error.headers.get("Retry-After")
                delay = float(retry_after) if retry_after and retry_after.isdigit() else 2**attempt
            except URLError as error:
                last_error = error
                delay = 2**attempt
            if attempt == self.retries:
                assert last_error is not None
                raise last_error
            time.sleep(min(delay, 8.0))
        raise RuntimeError("unreachable retry state")

    def discover_leagues(self) -> tuple[list[SourceLeague], FetchResult]:
        response = self._request(f"{self.base_url}/leagues")
        value = _json(response.body or b"")
        if not isinstance(value, list):
            raise ValueError("league discovery must return an array")
        leagues: list[SourceLeague] = []
        seen: set[str] = set()
        for entry in value:
            if not isinstance(entry, dict):
                raise ValueError("league discovery row must be an object")
            source_id = str(entry.get("id", "")).strip()
            display_name = str(entry.get("name", "")).strip()
            if not source_id or not display_name:
                raise ValueError("league discovery row is missing id or name")
            if source_id in seen:
                raise ValueError(f"duplicate source league id: {source_id}")
            seen.add(source_id)
            leagues.append(SourceLeague(source_id, display_name))
        return leagues, response

    def fetch_category(
        self,
        source_league_id: str,
        capability: CategoryCapability,
        conditional_headers: Mapping[str, str] | None = None,
    ) -> FetchResult:
        if capability.surface == "exchange":
            path = "exchange/current/overview"
        elif capability.surface == "stash":
            path = "stash/current/item/overview"
        else:
            raise ValueError(f"unknown provider surface: {capability.surface}")
        query = urlencode({"league": source_league_id, "type": capability.name})
        return self._request(
            f"{self.base_url}/{path}?{query}", conditional_headers
        )

    def parse_rows(
        self, capability: CategoryCapability, raw_response: bytes
    ) -> list[ParsedRow]:
        payload = _json(raw_response)
        if not isinstance(payload, dict):
            raise ValueError(f"{capability.name} response must be an object")
        lines = payload.get("lines")
        if not isinstance(lines, list):
            raise ValueError(f"{capability.name} response is missing lines")
        if capability.surface == "exchange":
            return self._parse_exchange(capability, payload, lines)
        return self._parse_stash(capability, lines)

    def _parse_exchange(
        self,
        capability: CategoryCapability,
        payload: Mapping[str, Any],
        lines: list[Any],
    ) -> list[ParsedRow]:
        core = payload.get("core")
        if not isinstance(core, dict) or core.get("primary") != "chaos":
            raise ValueError(
                f"{capability.name} exchange response is not chaos-primary"
            )
        items_value = payload.get("items")
        if not isinstance(items_value, list):
            raise ValueError(f"{capability.name} response is missing items")
        items = {
            str(item.get("id")): item
            for item in items_value
            if isinstance(item, dict) and item.get("id") is not None
        }
        result: list[ParsedRow] = []
        for ordinal, line in enumerate(lines):
            if not isinstance(line, dict):
                raise ValueError(f"{capability.name} line {ordinal} is not an object")
            item_id = str(line.get("id", "")).strip()
            item = items.get(item_id)
            if not item_id or item is None:
                raise ValueError(
                    f"{capability.name} line {ordinal} has no stable item identity"
                )
            value = _finite_non_negative(line.get("primaryValue"), "primaryValue")
            volume_value = line.get("volumePrimaryValue")
            volume = (
                _finite_non_negative(volume_value, "volumePrimaryValue")
                if volume_value is not None
                else None
            )
            low = bool(line.get("lowConfidence")) or volume is None or volume < 100
            result.append(
                ParsedRow(
                    row_key=item_id,
                    item_id=item_id,
                    name=str(item.get("name", "")).strip() or item_id,
                    category=capability.name,
                    chaos_value=value,
                    source_value=value,
                    reference_currency="chaos",
                    listing_count=None,
                    volume=volume,
                    confidence="low" if low else "normal",
                    payload={"line": line, "item": item},
                )
            )
        return result

    def _parse_stash(
        self, capability: CategoryCapability, lines: list[Any]
    ) -> list[ParsedRow]:
        result: list[ParsedRow] = []
        for ordinal, line in enumerate(lines):
            if not isinstance(line, dict):
                raise ValueError(f"{capability.name} line {ordinal} is not an object")
            item_id_value = line.get("id")
            details_id = str(line.get("detailsId", "")).strip()
            item_id = str(item_id_value if item_id_value is not None else details_id)
            if not item_id or not details_id:
                raise ValueError(
                    f"{capability.name} line {ordinal} has no stable item identity"
                )
            value = _finite_non_negative(line.get("chaosValue"), "chaosValue")
            listing_value = line.get("listingCount")
            listing_count = int(listing_value) if listing_value is not None else None
            count_value = line.get("count")
            volume = (
                _finite_non_negative(count_value, "count")
                if count_value is not None
                else None
            )
            low = bool(line.get("lowConfidence")) or (
                listing_count is not None and listing_count < 20
            )
            result.append(
                ParsedRow(
                    row_key=details_id,
                    item_id=item_id,
                    name=str(line.get("name", "")).strip() or details_id,
                    category=capability.name,
                    chaos_value=value,
                    source_value=value,
                    reference_currency="chaos",
                    listing_count=listing_count,
                    volume=volume,
                    confidence="low" if low else "normal",
                    payload=line,
                )
            )
        return result
