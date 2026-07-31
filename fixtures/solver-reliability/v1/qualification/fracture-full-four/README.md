# Qualified Fracture full-four fixture

This directory freezes the exact full-four qualification input used by the
Fracture-local coarse-parent milestone. The ordinary tracked
`fixtures/solver-natural-t1/v1` copy has reduced state, reforge-work, memory,
and scheduling caps, so it cannot reproduce the qualified carrier graph and
workload.

The case is preserved byte-for-byte from
`build/gate1-baseline-corpus/cases/natural-t1-full-four-47d8b909aa88.json`.
Its SHA-256 is
`ec05210da8e1fa7df2fc6aab5fc1419048467bc5dfbbac2df5d5dde9d4910304`.
The source corpus manifest SHA-256 was
`88570c7306ffaaab242c37896f4dbe2d529ad23e4f9e77e041012fb3d9f4822f`.

Qualification must preserve:

- six coarse-parent junk classes;
- exactly 217 root Chaos successors;
- a 927-state coarse carrier graph;
- Fracture transition hash `04a66ba6c6dfcabf`;
- Fracture policy hash `3e5d7530e7aed5fb`;
- compiled strategy SHA-256
  `e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`.
