# Batch 29-platform-font-resolution · Builder

## Done

Normal batch, from latest default `1068ee315006c5d3599e5c2100435aa8b9441967`.
Source delivery `771f25462d28dab888e6292d350b689b6345b0d8`; separate worktree,
ocgcore initialized and unchanged. Batch 28 remains intact. No self-acceptance
or merge; no shared status-page changes needed beyond Brain recording the
font gate disposition after review.

Resolve installed proportional/monospace families through public Qt APIs;
initialize the application's default before QML and bind Theme to the same
resolver. Preserve size/weight/tracking/layout tokens. No suppression, font
installation, dependency, private API, CI change or engine/model/policy edit.
Dedicated font regression has separate CMake module scope; existing navigation
QML, harness and screen driver are unchanged. Architecture records the actual
QML comma-string semantics and Qt-floor source compatibility; no ADR needed
for this bounded implementation repair.

## Checked

Native macOS arm64 / Qt 6.11.1: Debug/WERROR/UI_TESTS configure, build and
CTest exit 0 at source SHA. All three targets pass. Real shell separately
survives 20 seconds (status 124) and produces zero-byte stderr; both independent
assertions and wrapper exit 0. Python supplies timeout semantics because GNU
timeout is absent, without changing either assertion.

Linux Qt 6.8.3: [source CI](https://github.com/cntrl-alt-lenny/edopro-next/actions/runs/37937649254)
passes all five required jobs. Existing Release/UI_TESTS build, CTest and
literal workflow timeout/empty-stderr checks pass; screenshot artifact saved.

Dedicated tests establish installed/resolved Helvetica/Menlo locally, pitch,
ordinary controls and popup delegates, plus QML's single literal family string.
Inspected real Main/DeckBuilder captures at 960×600/1280×800 using synthetic
cards: title/body wrapping, result elision/clipping, numeric mono text, search
focus outline and open popup readability. Also captured the real executable
with a synthetic catalogue. Captures/assets remain untracked.

Generators, README/home checks, supported Python 3.13 discovery, golden
reproduction/unchanged diff, framework, whitespace and PR-body evidence checker
pass. Discovery's 11 skips are explicit (semantic binary absent; Windows ACL).
Exact commands, outputs, exits and failed attempts:
[evidence](29-platform-font-resolution-evidence/commands.md).

## Not checked

Qt 6.5 execution, Windows, Steam Deck, real card databases, physical keyboard,
native navigation reliability, all language glyphs or duel behaviour. Public
Qt 6.5 source compatibility is distinct from execution. Linux CI uses Release;
Linux Debug/WERROR was not run and the forbidden CI-settings change was not
made. Existing screen harness still emits its pre-bootstrap alias warning on
macOS; its bootstrap belongs to batch 27. Real product smoke is clean.

## Failed or blocked

Baseline real smoke: survival passed, stderr failed on missing Sans Serif.
Pre-fix regression at `fcbf00d1`: exit 1. Retained intermediate QML-module
collision/TestHarness failures, missing Theme import, popup ownership lookup,
resource alias, absent capture directory, scratch fixture arity error, empty
PR-check input and Python 3.9 README-test failure. Corrected causes or used
the supported Python version; no weaker assertions. No remaining product-font
failure observed. Brain must review this exact delivery and coordinate new-head
integration/revalidation of batches 27/28.
