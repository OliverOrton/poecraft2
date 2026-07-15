# Economy League Selector Design Spec

Selected direction: compact title-bar trigger plus aligned popover, based on
`design/mockups/economy-selector/variant-a-compact-popover.png`.

- Place the control after the existing title-bar spacer; do not reorder the
  document creation commands.
- Trigger text is selected league plus a short source-age token. One status dot
  conveys fresh/stale/offline/manual state; accessible text and title carry the
  full state.
- Popover width is 280px or less. It overlays the workspace and never changes
  title-bar height.
- Group active temporary leagues first, permanent leagues second, the newest
  archived challenge family third, and `Custom / manual` last.
- HC/SC/permanent distinctions use short text badges, not color alone.
- The selected row has a restrained accent background and checkmark. A target
  being fetched is disabled and labelled `Switching…`; failure preserves the
  old selection and exposes Retry.
- Footer shows full source age/state and a compact Refresh button.
- Escape, outside click, and successful selection close the popover. Arrow-key
  behavior follows ordinary buttons; every row is a button.
