# CLAUDE.md

Working agreement for AI agent sessions on this repository. Read this before changing
anything.

## What this project is

**edopro-next** is a long-term effort to give [EDOPro](https://github.com/edo9300/edopro)
a modern client — Qt 6 / QML presentation over the existing, mature duel engine.

The one-sentence framing, which resolves most design arguments:

> Preserve the engine. Expose clean semantics. Modernise the client.

This is **not** a rewrite of Yu-Gi-Oh's rules, and not a reimplementation of EDOPro.
Project Ignis's engine and card scripts represent years of correctness work. They are
assets to protect, not legacy to replace.

## The architectural rule that matters most

    THE RULES ENGINE MUST NOT BECOME THE UI.
    THE UI MUST NOT IMPLEMENT GAME RULES.

Concretely:

- UI code never decides legality, never computes what is targetable, never infers rules.
  It renders a model and sends responses.
- The client model is **semantic**, not visual. It must contain no Irrlicht or Qt types.
  If a struct describing a card needs a transform matrix, that is a bug.
- `ocgcore` and the Lua CardScripts are authoritative. If the UI and the engine disagree,
  the engine is right.

## Authoritative upstream components — do not modify

| Component | Why |
|---|---|
| `ocgcore/` (submodule) | The rules engine. Upstream owns it. |
| Project Ignis CardScripts (Lua, fetched at runtime) | Card behaviour. Authoritative. |
| Project Ignis BabelCDB (`.cdb`, fetched at runtime) | Card data. Authoritative. |

If something appears wrong in these, the fix is an upstream issue or PR — not a local
patch that silently diverges.

## Where code belongs

| Path | Contains | Ownership |
|---|---|---|
| `gframe/` | Legacy Irrlicht client | Upstream. Touch minimally. |
| `ocgcore/` | Rules engine submodule | Upstream. Do not touch. |
| `client/` | New semantic client model (no UI types) | Ours |
| `ui/` | Qt 6 / QML presentation | Ours |
| `docs/` | Architecture, ADRs, roadmap | Ours |
| `tools/` | Python tooling | Mixed — check before editing |

New systems go in clearly owned directories. Do not scatter new code through `gframe/`.

## Build and test

Baseline upstream build (Linux, verified working — see `docs/BASELINE.md`):

```bash
export TRAVIS_OS_NAME=linux TARGET_OS=linux BUILD_CONFIG=release ARCH=x64
export VCPKG_ROOT="$PWD/vcpkg"
./travis/install-premake5.sh linux
./travis/install-local-dependencies.sh linux
./travis/build.sh
# artifact: bin/x64/release/ygoprodll
```

Two hard-won gotchas, both documented fully in `docs/BASELINE.md`:

- A **Windows clone gives `travis/*.sh` CRLF endings**, and the build dies with
  `env: 'bash\r': No such file or directory` (exit 127). Normalise with
  `find . -name '*.sh' -exec sed -i 's/\r$//' {} +`.
- **Do not build on `/mnt/c`** under WSL. Copy to `$HOME` first; 9p is far too slow.

Upstream has **no test suite**. Do not claim otherwise, and do not report "tests pass"
when what happened is that a build succeeded.

## Validation expected before committing

Proportionate to what changed:

- Touched the build? Prove it still builds, and say on which platform.
- Touched the client model? Its unit tests must pass.
- Touched presentation? State explicitly what you verified visually, and what you did not.
- Touched anything near duel behaviour? Say precisely how you established behaviour is
  unchanged. "It compiles" is not evidence.

The property this project most wants to be able to assert is:

> this refactor changed presentation, not duel behaviour

Design changes so that claim is demonstrable.

## Style rules

- **C++20.** Match the surrounding style in the file you are editing. Upstream style
  differs from ours; when in `gframe/`, follow upstream's.
- **No gratuitous reformatting of upstream code.** Mass formatting makes future merges
  miserable. Keep patches focused on the lines you actually mean to change.
- **No new dependencies without justification.** Record the reasoning in an ADR. "It is
  convenient" is not a justification; "the alternative is maintaining a second protocol
  decoder" is.
- Python is tooling only. It must never sit in the render or input loop.

## Upstream merge policy

```
origin    -> https://github.com/cntrl-alt-lenny/edopro-next.git
upstream  -> https://github.com/edo9300/edopro.git   (push deliberately disabled)
```

`upstream`'s push URL is set to `DISABLED_use_origin` so nothing can be pushed to Project
Ignis by accident. Do not undo this.

To take upstream changes: `git fetch upstream && git merge upstream/master`, then resolve,
then rebuild the baseline. Record the new upstream commit in `docs/UPSTREAM.md`.

Because we deliberately keep changes out of `gframe/`, most merges should be clean. When
they are not, that is a signal we have coupled too tightly to legacy code.

## Licensing constraints

EDOPro is **AGPL-3.0-or-later**. This fork remains so.

- Preserve every upstream copyright notice, `LICENSE`, `COPYING` and the `notices/` tree.
- Do **not** relicense inherited code. Not under MIT, not under anything.
- Qt must be **dynamically linked** (LGPLv3 alongside AGPL).
- Do **not** commit card artwork, `.cdb` databases or Lua CardScripts. They are fetched at
  runtime and we do not have redistribution rights.
- Do not imply affiliation with or endorsement by Project Ignis, Konami or Shueisha.

## Honesty rules

These are not stylistic preferences; treat them as hard constraints.

- **Never describe planned functionality as shipped.** `README.md` separates what exists,
  what is in progress and what is planned. Keep that separation accurate.
- **No fake functionality.** No stub screens presented as working, no placeholder data
  dressed up as real, no README features that do not exist.
- **Report failures precisely.** If the build breaks or something cannot be completed,
  diagnose it and leave the repository clean. Do not hack around it and do not quietly
  narrow the scope.
- If you did not verify something, say so.

## Do not

- Rewrite working engine logic because it looks old.
- Delete Irrlicht code before its replacement demonstrably reaches parity.
- Start the migration with the duel field. It is the highest-risk screen; see the ordering
  in `docs/architecture/current-edopro.md`.
- Introduce Rust between the UI and the engine (see ADR 0001).
- Make one giant commit.
