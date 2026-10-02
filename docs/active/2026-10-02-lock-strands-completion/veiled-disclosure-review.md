# Veiled Lock disclosure boundary

The current Lock slice holds Veiled Chaos/Exalt requests and original items with
pending veiled modifiers. This is a product/API disclosure restriction, not a
claim that the game forbids foreseeing the visible veiled result. Ordinary
veiled crafting, acquisition-time offer fixation, explicit Unveil and consumed
Lock history remain supported by their existing contracts.

The [approved Unveil contract](../../mechanics/veiled-crafting.md) fixes offers
at acquisition (Oliver, 2026-08-13). Its separate reveal action exposes those
already-stored offers. Native `add_veiled_mod` generates them at acquisition;
`do_unveil` consumes a selected offer and clears the pending flag. No new
probability law is needed to reserve this existing complete native outcome.
Completed Unveil metadata alone does not block a later Lock.

A visible-only Lock projection is technically possible: retain the complete
reserved item internally and omit pending offer IDs/counts from every public
preview. A renderer change alone is insufficient. Current native preview and
portable `fixed-currency-lock-v1` checkpoint both contain the full reserved item;
its serialized snapshot would disclose the offers before acquisition. Supporting
save/reload/import/Undo while keeping those unobserved IDs out of the public
payload needs an opaque persistence/disclosure contract, not merely hidden UI.
The repository has no Lock-specific ruling authorizing what may be observed at
this stage. The integration owner can review that scope with Oliver before
expansion; this batch does not guess it or start a new implementation programme.

Creation refuses before draw/payment. Active checkpoints introducing pending
veiled input/current/preview refuse atomically. Ended Lock history may preserve
ordinary later veiled acquisition, without a cached preview. Focused negative
transport fixtures and native/worker tests qualify this boundary. No hidden
offers, RNG seeds/states, adaptive policy or exact Lock value are enabled.
