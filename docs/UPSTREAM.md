# Tracking upstream

This fork intends to keep consuming upstream work indefinitely. That only stays possible
if we are disciplined about where our changes go.

## Remotes

```
origin    https://github.com/cntrl-alt-lenny/edopro-next.git
upstream  https://github.com/edo9300/edopro.git      (fetch)
upstream  DISABLED_use_origin                        (push)
```

`upstream`'s push URL is deliberately set to an invalid value so that a stray
`git push upstream` cannot reach Project Ignis. **Do not undo this.** Contributions to
upstream should be made deliberately, as pull requests from a separate clone.

Reproduce this configuration with:

```bash
git remote add upstream https://github.com/edo9300/edopro.git
git remote set-url --push upstream DISABLED_use_origin
```

## Fork basis

| | |
|---|---|
| Forked from | `edo9300/edopro` |
| Base commit | `54ea755aa0243e2f18bb6bd2187fc9b2f7e29788` |
| Base date | 2026-08-20 |
| Base subject | "make Duel constructor private" |
| Submodule | `ocgcore` → `edo9300/ygopro-core` |

This is a **standalone repository**, not a GitHub fork object, because the account
already holds a fork of `edo9300/edopro` used for staging upstream patches and GitHub
permits only one fork per account per upstream. The full upstream history is preserved
here, so merges behave identically; only GitHub's "forked from" badge is absent. The
attribution that AGPL actually requires is in `README.md`, `LICENSE`, `COPYING` and
`notices/`.

## Taking upstream changes

```bash
git fetch upstream
git merge upstream/master        # or rebase a topic branch onto it
# resolve, then re-run the baseline build before trusting the result
```

After a successful merge:

1. Update the base commit table above.
2. Re-run the baseline build (`docs/BASELINE.md`) and confirm it still produces a
   working `ygoprodll`.
3. Note anything upstream changed that invalidates a claim in
   `docs/architecture/current-edopro.md`. That survey is pinned to a commit and will
   drift.

## Keeping merges cheap

The single most important rule: **new code goes in new directories.**

| Path | Ownership | Merge risk |
|---|---|---|
| `gframe/` | Upstream | High — avoid touching |
| `ocgcore/` | Upstream submodule | Do not touch |
| `travis/`, `premake5.lua` | Upstream | Medium — coordinate carefully |
| `ui/` | Ours | None |
| `client/` | Ours | None |
| `docs/` | Ours | None (except `docs/upstream-README.md`) |

Do not reformat, rename or re-indent upstream code you are not otherwise changing. A
mass-formatting commit turns every future merge into a manual conflict resolution
exercise. Keep patches to the lines you actually mean to change.

When we eventually must modify `gframe/` — and we will, to introduce the seam that emits
semantic events — those changes should be as small and as clearly delimited as possible,
and recorded in the table below.

## Upstream's own CI in this fork

`.github/workflows/edopro.yml` is upstream's build-and-deploy workflow. It is **left
unmodified on disk** but **disabled at the repository level** (Actions → "Build EDOPro" →
disabled manually). Re-enable with:

```bash
gh api -X PUT repos/cntrl-alt-lenny/edopro-next/actions/workflows/edopro.yml/enable
```

The reason is worth recording, because the failure looks alarming and is not:

> Upstream's workflow **builds successfully** in this fork. Every job then fails at the
> `Deploy` step, which needs `DEPLOY_REPO` / `DEPLOY_TOKEN` secrets that only Project
> Ignis holds, followed by a `Log Failure` step that needs a Discord webhook. Verified on
> run `32776508289`: `Build: success`, `Predeploy: success`, `Deploy: failure` with
> *"No webhook is given."*

So the red status was purely missing deployment credentials, never a broken build. It was
disabled because a permanently-failing check on every push destroys the signal value of CI,
not because anything is wrong with it. Our own `edopro-next` workflow covers the same
build via its `upstream-baseline` job, without needing secrets.

Disabling is a repository setting, not a file change, so it creates no merge conflict.

## Intentional local deltas from upstream

Anything we knowingly change in upstream-owned files is recorded here, so a future merge
conflict can be resolved with the original intent visible.

| File | Purpose | Approximate scope | Why unavoidable | Expected upstream-merge risk |
|---|---|---|---|---|
| `README.md` | Replaced; upstream's preserved at `docs/upstream-README.md` | Existing repository-level replacement | This repository is a distinct project and must not misrepresent itself as upstream. Attribution retained. | Existing project-level delta |
| `gframe/duelclient.cpp` | One include and one RAII observer scope at `ClientAnalyze` entry | One include plus one six-line call site | The live observer must see the exact normalized packet and both independent protocol-width flags, then compare after every existing return without rewriting the legacy switch. | Low; the switch and its control flow remain unchanged, but this is the runtime seam to recheck after upstream merges |
| `gframe/cli_args.h` | `SEMANTIC_VERIFY_REPLAY` launch parameter enum | One additive enum value | Enables CLI-driven replay verification without menu interaction. | Low |
| `gframe/edopro_main.cpp` | Parse `--semantic-verify-replay` argument | Four localized lines in argument parser | Routes CLI verification invocation. | Low |
| `gframe/gframe.cpp` | Execute `verify_replay_cli` when flag is set | Ten localized lines at `edopro_main` entry | Invokes headless replay verification. | Low |
| `gframe/drawing.cpp` | Non-blocking `WaitFrameSignal` for `EDT_NULL` | Two lines in `WaitFrameSignal` | Allows headless replay execution without blocking on frame signals. | Low |
| `gframe/premake5.lua` | Conditional observer define/link on the legacy targets | Four localized lines in the target configuration | gframe must remain C++17 while the optional observer is linked as a separate target. | Low |
| `premake5.lua` | `--semantic-observer` option and conditional `client`/observer project includes | Eight additive lines | Premake needs an explicit, reversible opt-in build path. | Low |
| `travis/build.sh` | Forwards `EDOPRO_NEXT_SEMANTIC_OBSERVER=1` to Premake | Three environment-gated lines | CI needs to exercise the observer-enabled legacy build without changing the ordinary baseline command. | Low |

No source changes were made to `ocgcore`; the intentional `gframe/` changes are listed above.

### Upstream files read but not modified

M2 built a second decoder for the duel protocol. It reads upstream headers, and one
generator consumes them. The following upstream files were read but not edited in this
slice:

| File | Relationship |
|---|---|
| `gframe/ocgapi_constants.h` | Parsed by `tools/generate_protocol_constants.py` to generate `client/include/edopro_next/client/protocol_constants.h`. CI fails on drift. |
| `gframe/common.h` | Same, for `OLD_REPLAY_MODE`. |
| `gframe/duelclient.cpp`, `client_card.*`, `client_field.*`, `core_utils.*`, `game.h`, `replay_mode.cpp` | Read as the reference for message layouts, packet boundaries and zone bookkeeping. Findings recorded in `docs/architecture/semantic-model.md` and `docs/architecture/live-semantic-observer.md`. |

The generated header is committed under `client/`, not `gframe/`, so a merge that changes
an upstream constant surfaces as a CI drift failure rather than a conflict.
