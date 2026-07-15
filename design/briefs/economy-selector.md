# Economy League Selector Brief

Add one compact, workspace-level economy selector that is visible from every
document without turning the title bar into a settings surface.

The trigger lives at the far right of the existing title bar and shows status,
selected league, and compact source age. Its popover groups current temporary,
permanent, one archived challenge family, and the manual profile. Switching is
atomic: the current selection remains active until a target snapshot downloads
and passes content-hash verification.

Required states: fresh, stale, offline-cached, switching, failed target, and
manual-only. Low-confidence warnings belong in price surfaces rather than
making the global selector large.
