# Engine Notes

**Status: non-authoritative working notes.** Implemented contracts live in
[Engine](README.md); mechanic rulings live in [Mechanics](../mechanics/README.md).

Parent: [Engine](README.md)

## General

No open entries.

## Build and exports

- 2026-07-19 `#debt` — Open: `scripts/build-wasm.ps1`'s explicit `$Exported`
  array omits the four stepped solver functions. `EMSCRIPTEN_KEEPALIVE` and
  the tracked release module currently export them, so product behavior is
  present; synchronize the script inventory with the public surface. See
  [WASM export-list drift](wasm.md#export-list-drift).
