"""Runtime-shape normalization for Solver Lab external documents.

Native partial reports are deliberately incremental: an optional object may be
absent, explicitly JSON null, or only partly populated.  Catalog documents are
also reopened across schema versions.  Consumers must normalize those runtime
shapes before following nested fields instead of relying on ``dict.get``
defaults, which do not apply to present-null values.
"""

from __future__ import annotations

from collections.abc import Mapping
from typing import Any


def as_mapping(value: Any) -> dict[str, Any]:
    """Return a shallow plain mapping or an empty mapping for any other shape."""

    return dict(value) if isinstance(value, Mapping) else {}


def as_list(value: Any) -> list[Any]:
    """Return a shallow list or an empty list for any other shape."""

    return list(value) if isinstance(value, list) else []


def first_mapping(value: Any) -> dict[str, Any]:
    """Return the first list element when it is a mapping."""

    values = as_list(value)
    return as_mapping(values[0]) if values else {}

