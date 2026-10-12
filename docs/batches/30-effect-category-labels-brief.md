# EDOPro Next · batch 30-effect-category-labels · Builder

Normal path: a bounded presentation change to existing filter labels, with
Brain review; no automatic Verifier. Work in a fresh isolated worktree on
`worker/30-effect-category-labels` from the fetched default branch. Initialize
`ocgcore`. Read project/framework rules and ADR 0012, especially Decision 5.
Do not merge or self-accept.

Goal: replace the unhelpful `Category N (0xBIT)` labels with trustworthy,
descriptive English names for the existing 32 effect-category controls.
Keep their index/order, bits, selection, filter semantics and count unchanged.
This is a search aid, never a statement of legality or engine behaviour.

Establish names from pinned primary upstream string resources, not memory or
invention. Re-read `gframe/game.cpp`'s checkbox construction with
`GetSysString(1100 + i)`; find and pin the corresponding authoritative English
resources. Quote the relevant mapping mechanism and record source/revision,
licensing/provenance and any deliberately fixed-local-language limitation.
Brain confirmed all 32 entries exist in
[Project Ignis Distribution's pinned strings](https://github.com/ProjectIgnis/Distribution/blob/54a6e2395c532648ff762540e9615319fac4f51b/config/strings.conf),
whose repository carries AGPL licensing. Re-read the resource and notices.
Do not commit card databases, artwork, CardScripts or unrelated resource packs.
If a name/index cannot be established, report the gap before shipping a guess.

Own `ui/src/deckbuilder/search_filters.{cpp,h}` only for category display names,
pure adapter tests in `ui/tests/test_deckbuilder.cpp`, any dedicated new label
test with scoped CMake registration, and label architecture/ADR documentation.
The existing QML already binds `searchResults.categoryNames`; leave QML,
TestHarness, screen driver, search lookup, Theme/fonts/startup and shared status
pages alone. Brain handles any genuinely needed shared integration or layout
seam. Do not copy the navigation branch or change its activation guard.

Demonstrate source-backed mapping across all labels and that displayed names
still select the original category bits/results. Include meaningful regression
sensitivity rather than merely duplicating a string array. Configure/build
UI Debug/WERROR/UI_TESTS, full CTest and strict real-shell survival/empty-stderr
smoke. Run screen layout/keyboard checks and inspect real popup captures at
minimum/default sizes, including long labels and lower controls. Keep bounds,
visibility and model assertions strict. Run generator, README, Python, golden,
framework/whitespace and PR-evidence checks; report actual skips/failures.

Start immediately: this does not depend on navigation #48, search #49 or
planning #47 merging. Font #50 is already on default. Local Qt 6.11.2 uses
`-DCMAKE_PREFIX_PATH=/opt/homebrew`; Linux CI uses Qt 6.8.3. Stay explicitly
offscreen while batch 27 owns native input; coordinate any required native
slot through Brain via the owner. No engine edits, dependency additions,
licensing changes, system settings or framework edits.

Commit focused changes and the four-part summary, push on every exit, and
report exact delivery/source SHA, CI and what Brain should update after merge.
