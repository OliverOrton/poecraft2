# Authored random recombination calculator

## Layout repair qualified; parent visual review pending

Oliver rejected the delivered visual fit at **04:43:10 UTC**: the desktop odds
table and full cards pushed the actual mods/picker below the fold, and mobile
captures started mid-card. Parent holds UI main integration. Earlier behavioral
qualification and Library previews describe the older source, not visual
acceptance of the repair. Existing images/receipts are preserved.

Qualified source `5b51c1b9bfe7d5f8ca6397328f43044a7ff364b3` opts into compact shared
ItemCards, retains base/level/rarity/influences and full affix text, summarizes
empty capacity, and leaves advanced properties collapsible. Odds and Calculate
occupy one compact strip; carrier, attempt and model details expand on demand.
Desktop keeps A left, B right and goal above the middle picker in bounded panels
with independent scrolling. Mobile presents A/B/Goal summaries, the selected
card and a naturally scrolling picker, with initial capture at the logical top.
Changing the selected mobile item returns to that overview; editing the same
item preserves position. Text sizes and charcoal/ember component geometry remain
shared. No API, native mechanics, optimizer, dependency, data or price change.

The shared card's compact mode is opt-in; default cards retain their stable full
slot presentation. The existing functional native/base/stale/picker/keyboard
checks remain. The passed browser gate uses native-authored six-affix A/B and
goal, all input affixes visible inside their scrollports at **1536×864** and
**1366×768**, visible goal affixes/picker controls/odds together, reachable final
goal tiers through internal scrolling, closed carrier details and actual mobile
top/selector behavior. Four functional captures plus two populated desktop and
two mobile captures were saved and inspected locally. Goal rows beyond the
available desktop height remain reachable in the goal's internal scrollport;
mobile uses normal vertical scrolling for the selected card and picker.

**Built/tested/rendered: passed at the exact source above. Parent visual
acceptance and main integration: pending, HOLD.** The preserved finite batch
passes metadata (10.52s), six focused tests including real WASM (23.24s),
TypeScript (3.45s), Vite (3.52s), immutable packaging (2.39s), and Chrome (3.66s).
The receipt is [layout-qualification.json](layout-qualification.json); logs and
eight PNGs are retained in `out/recombination-calculator/gate-layout-fit-r4-supervised/`.
Build ID: `fc9fb78df7f6358b9240c284111315d9f2d89d0c60f250b9dfb01b3875c573c0`.

LOCAL was released at **05:38:32 UTC**. All six original stage identities are
proved absent, stage cleanup flags are clear, and the elevated ownership-scoped
CIM census returns zero. Parent later attributed ghost Terminal windows to the
old detached `f532376a` launcher used by this historical batch. The stage/CIM
evidence does not establish absence of all Terminal hosts. CI owns that audit.
Any future heavy execution must use the repaired worker at commit `29ae95cf`,
SHA256 `670a258e01ff30bcf12f66b1614cdf082c8f423dc328a0b4a4db9b17609dc74b`,
and a fresh parent reservation. No heavy execution ran during closeout.

Fresh previews from this qualified source are saved in Library, version 0:

| Preview | Captured UTC | Library file |
| --- | --- | --- |
| Six-affix desktop, 1536x864 | 05:38:18.949197 | `libfile_468a22422020819186bbf2b60b82b355` |
| Six-affix desktop, 1366x768 | 05:38:19.042402 | `libfile_cd362d9279148191affdb9a42dca2bde` |
| Mobile overview, 390x844 | 05:38:19.119169 | `libfile_7c5ea51ae8f481919672a10de5a5ac6b` |

The current upload helper failed before preparation because its hosted-app
connection was unavailable. Parent's approved direct create fallback saved all
three in one ordered call. Each cloud result is confirmed; Windows
`os.setxattr` prevents local metadata writeback. Identities and unchanged PNG
hashes are retained in the receipt. There were no duplicate cloud uploads.
Earlier bad-layout Library files remain rejected and are not these previews.

The layout attempts retain their original logs: r1 fixed a test querying
influence text instead of its accessibility label; r2/r3 exposed the last input
affix outside the 1366px scrollport. Combining header controls and tightening
compact spacing fixed that fit without smaller type or weaker assertions. The
first r4 launch stopped at sandboxed Node's `spawnSync git EPERM`; the unchanged
source then passed under the authorized supervisor. Original identities and
ownership-scoped cleanup were checked after each attempt. Immutable input and
earlier capture hashes agree before/after. Parent owns visual acceptance and
main/CI integration; source closeout changes only docs and receipts.

## Earlier behavioral qualification (visually rejected)

The earlier isolated calculator gate **passes** at clean source
`abab3f60e5bfce6691e3c6db3e2389ab3186b8f2`, including the navigation wrapping fix
and regression assertion at `8d9b57fb9059fa142973397b9bf5c2f76b5e6238`. Fresh Vite,
immutable packaging and actual Chrome interaction checks passed. All six matching
desktop/mobile captures were reviewed; navigation and calculator fit at 390px.
Unchanged focused real-WASM tests and TypeScript qualification are reused from
`dd942153d22eee3506bdffe23a949e7db8b0852e`. Parent owns main/CI integration.

LOCAL was released at **04:26:05 UTC**: all four final stage identities are
proved absent, timeout/cancellation/descendant/cleanup/survivor flags are clear,
and elevated ErrorAction=Stop CIM returns zero matching processes. Eighteen
immutable hashes and all six reviewed gate-r6 images match before/after. No heavy
job remains from this session. The earlier gate-r6 release was 04:15:22 UTC.

## Scope and behavior

Oliver selected a fresh standalone two-input calculator. Branch
`dot/recomb-calculator-ui-20261006` starts from reverified remote main
`7252027c80856628ed16734583bfc9d6e166458b`, isolated at
`C:/Users/Oliver/Documents/Codex/2026-10-05/task-9/recomb-ui`. The private bare
repository is its sibling `source.git`; parent may fetch the local branch there.
The normal checkout, protected `0`, native mechanics, optimizer worktree and
frozen data/prices are untouched. Parent owns main publication and CI gating.

`+ Recombination` opens a dedicated workspace document. Input A left and B right
use independent native sessions and shared editable ItemCards; one goal sits
above the shared modifier picker. Tabs, buttons, outlined cards and picker text
identify the edit target. Narrow views show an A/Goal/B overview, the selected
card, then the picker; odds occupy a compact action strip. The charcoal/ember tokens, normal-weight
modifier copy, influence display and shared geometry are retained.

Native atomic editing authors the inputs. The existing read-only random pair/goal
API calculates a snapshot using temporary sessions/items and a pair, all closed
on success or failure. Goal/input/base edits and Cancel invalidate previous and
pending odds. Cancellation drops delivery while synchronous native work finishes
and cleans up. IndexedDB recovers independent inputs and the goal, never odds.
The base modal traps focus; cancellation preserves state and returns focus.

A result-base requirement matches A's or B's actual base, without selecting a
carrier or renormalizing probability. Native terminal `is_goal` and base rows
provide unconditional mass; equal bases count both carriers. The endpoint uses
A's native goal catalogue, disclosed beside the selector; absent goal families
and unsupported pairs are native refusals. No frontend probability model,
optimizer, Apply button or simulator was added.

Odds enumerate the supported **estimated game model**, not game-exact mechanics.
Provisional spawn weights, omitted unverified upgrades, unobserved numerical
rolls/property scope, exceptional-pair refusal and unknown gold/dust costs remain
explicit. There is no API blocker for the supported authored-pair calculator.

## Earlier qualification and identities (superseded visual fit)

Earlier passed receipt: [navigation-qualification.json](navigation-qualification.json).
Original detailed logs are retained locally in
`out/recombination-calculator/gate-navigation-final/`. The unchanged focused/TS
qualification and earlier artifact remain in [qualification.json](qualification.json)
and `out/recombination-calculator/gate-r6/`.

| Layer | Status | Wall time |
| --- | --- | --- |
| Metadata / immutable input checks | Passed | 10.34s |
| Six focused web tests, including real pair/goal WASM | Passed | 23.03s |
| TypeScript | Passed | 3.28s |
| Vite | Passed | 3.46s |
| Immutable package | Passed | 1.83s |
| Real Chrome 152.0.7977.83 | Passed | 3.08s |

The table above describes gate-r6. The final navigation-only batch runs metadata
(10.35s), Vite (3.51s), package (1.87s), and Chrome (3.10s), all passed. It reuses
unchanged TypeScript, focused/native tests and WASM/ABI evidence after checking
the exact two-file web delta. The 420s parent reservation runs no other heavy job.
Final build ID `ff28cb2101e6280b1cc5b97ab5ebd6a74740ea8ca45c970b50cc230a169d6e91`,
source-tree SHA256 `fa28bee482770b7462b13c79c84e5d89c6962bd20f84419365f2c7d82f05b467`.
Final immutable archive:
`dist/public-artifacts/web/fc87466556946fdf1161d51604a1cf2af7cee42dd562fce9b0587a605770e7e9`.

Browser coverage: A/B/goal selection, shared modifier authoring, native 100% goal
fixture, equal/different-base mass, base-toggle invalidation, repeated/interrupted
picker, invalid item level, native corruption refusal, atomic invalid edit,
delayed real native delivery after edits/Cancel, normal/hover button contrast,
normal-weight mods, actual Tab focus, 390px calculator bounds/stacking and reload.
The full existing smoke scope is unchanged; the dedicated scope was run here.
The Node DOM fixture logs the existing game-art relative-URL fallback warning;
the real browser loads artwork and has no console/page errors.

Earlier gate-r6 build ID `84520ecc74fbef8b9553250a0b0c1309d50f22b0982976c9d9c874ff1582f6db`,
source-tree SHA256 `e1c013f95762bfc8d872ac25f292b6d632417658d275c61f14474a5bc6d1f19d`.
Immutable web archive:
`dist/public-artifacts/web/f75f104dff38c2446d258088e6a2af31fa955a578431f6eb2521ef73f2970a54`.

The unchanged qualified runner is `d485ff11`; worker blob `f532376a`, SHA256
`1f7978fa0cf43c331487ea57212286cbf92ff96141d379a901c4bd421d4aae92`.
Every stage uses owned-job cleanup with a 5s drain and a finite watchdog. Gate-r6
uses the parent's 04:06:24–04:21:24 UTC reservation; the final batch uses its new
420s reservation with 30s reserved for cleanup. No explicit outer memory cap
exists in the adapter API. No native compiler, dependency install, data refresh,
solver search or Simulator trials ran.

Dependencies reuse the qualified tree only after exact package-lock SHA256
`244102b6e9a380054cb847b8ecbed3ef3f7956eb79a18bf8562acbc200d5be3c` match.
Reused WASM `50c98f55`, loader `8ec20cf7`, runtime `82fb60a2`, ABI 3 match the
qualified receipt; engine/WASM source matches its `7fd6a48e` source.

## Retained attempts and integration handoff

`gate-r1` and `gate-r2` stop at TypeScript diagnostics, subsequently fixed.
`gate-r3` reaches Chrome and exposes a base-checkbox event value being reset by
busy rendering; the production handlers now snapshot values before rendering.
`gate-r4` stops at a normal-color assertion with a hovered pointer; the test now
explicitly checks normal and hover states. `gate-r5` stops at narrow overflow;
scoped picker wrapping and ResizeObserver settlement fix it. All failed logs and
receipts remain; each attempt proves original process absence and immutable hash
agreement before its next attempt. No assertions or native odds were weakened.

Six PNGs in `gate-r6/screens/` show input A/B focus, goal odds, unsupported input,
narrow goal and narrow picker. The cards/picker are readable without horizontal
overflow. Saved narrow pixels also expose clipped shared document navigation.
The isolated two-line wrapping fix (`40f7e5c1`) and two-line gate assertion
(`8d9b57fb`) subsequently pass the final supervised source-matched build/package
and browser run at `abab3f60`. Its six separately reviewed PNGs are in
`gate-navigation-final/screens/`; the original gate-r6 images are preserved and
continue to describe the earlier source. No further source polish followed the
final gate. Finalization changes receipts/docs only. Parent owns image delivery,
reconciliation with main `1b038ef8` HANDOFF, publication and CI gating; no rebase
is required for its unrelated reporter regression. Behavioral QA was complete
at that checkpoint; Oliver's later visual rejection and the repair above
supersede its visual acceptance claim.

The thread-messaging tool disappeared after the cleanup receipt. Sending the
immediate LOCAL-release message failed because the callable was unavailable;
this final delegation result supplies the release and source identities.
