# ADR 0013: Application-local filter keyboard navigation

Date: 2026-10-09
Status: Proposed for Brain review

## Context

The owner selected all enabled deck-builder filters as Tab destinations, leaving
macOS settings untouched. Round 026 established that Cocoa's text/list preference
excludes non-text controls even with Tab focus capability. Its process-local
`QStyleHints::setTabFocusBehavior` experiment uses an internally documented setter
and retains intermittent native failures; it is not a supported product fix.

## Decision

Use public Qt Quick `Keys`, `KeyNavigation` links, `Item.forceActiveFocus` and
`Item.nextItemInFocusChain`. Explicit forward/backward links cover the filter
column and each popup. A shared key handler accepts Backtab and Shift+Tab,
skips disabled/invisible links, and retains Qt's ordinary chain at the column's
entry/exit. The results ListView declares its actual public `Accessible.List`
role so Cocoa's implicit boundary traversal can reach it. Popups cycle only
inside their modal content, focus their first control after opening, and return
to their trigger after closing. Existing focus scrolling follows these changes.

These presentation choices never decide search matching, legality or any rule.
There is no style-hint override, native/private Qt dependency, new dependency,
OS preference write, new configuration switch or build-file change.

## Compatibility and alternatives

The used APIs predate Qt 6.5, the supported floor. Qt 6.5.3 and CI's Qt 6.8.3
source were inspected; local execution uses Qt 6.11.1. Actual execution on 6.5
and 6.8.3 is separate evidence, not inferred from API presence. A global setter
was rejected because its documented contract is internal. Requiring the owner
to enable full keyboard access contradicts the selected application-local goal.

## Consequences

Filter order is explicit presentation state and must be updated with any added
control. Tests require destination accounting, clipping, operation, popup entry
and restoration; unavailable controls must be skipped in both encodings.
Outside these filter surfaces the existing platform chain remains; this decision
does not establish full keyboard/controller or accessibility parity.
