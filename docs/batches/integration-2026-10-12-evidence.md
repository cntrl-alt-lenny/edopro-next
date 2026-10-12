# Integration evidence index

Brain's checks are pinned to navigation `08f4e630` and search `7af65e44`.
Both merge approved default/font head `af334154`; the source equality and
CMake registration review are in each `integration-diff.txt`. Nothing here
replaces review at a subsequently moved head.

`27-integration-brain-evidence/` and `28-integration-brain-evidence/` contain
commands, raw outputs (repository paths sanitized), exit statuses and CI
snapshots. Final local builds are fresh Debug/WERROR/UI_TESTS in
`ui/build/integration-qt6112`, with Homebrew Qt 6.11.2 and prefix `/opt/homebrew`.
The `qt683-*` and `missing-qt-configure.txt` files retain earlier environment
failures; they are not passing checks. See `integration-2026-10-12-setup.txt`.

The smoke wrapper starts the real shell offscreen, independently asserts it
survives 20 seconds and stderr is empty, then terminates/reaps it. Neither
assertion filters output. Normalized timeout status is 124; wrapper exit is 0
only if both predicates pass. Shell stdout/stderr files are retained separately.

Native rows run sequentially with the committed foreground.swift observer
compiled as separate diagnostic tooling. Five preselected traced rows precede
two preselected untraced cases. Default-forward fails in the latter group.
The matrix stops at that failure; no retry is substituted. The deliberate
peer control is a separate labelled experiment afterwards: its test exits 1,
and the predicate requiring that activation-loss failure exits 0. Every owned
test/peer/monitor exits before later UI interaction.

`linux-capture-review.txt` records exact CI artifact runs, image names and
hashes of the inspected original minimum/default images. Local real-shell
typography captures also ran successfully; only explicitly named inspected
states support visual claims. No physical keyboard or broad appearance claim.

Rerun the UI commands named in each file. For Cocoa use
`QT_QPA_PLATFORM=cocoa ui/build/integration-qt6112/tests/test_deckbuilder_screen
filterTabTraversalStaysVisible:<row>` and the unavailable-controls slot;
set `EDOPRO_NAVIGATION_TRACE=1` only for traced rows. Keep a separate foreground
observer, exclusive desktop slot and all original assertions. Deliberate peer
interventions must be labelled and never mixed into quiet acceptance rows.
