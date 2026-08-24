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

## Intentional local deltas from upstream

Anything we knowingly change in upstream-owned files is recorded here, so a future merge
conflict can be resolved with the original intent visible.

| File | Change | Why |
|---|---|---|
| `README.md` | Replaced; upstream's preserved at `docs/upstream-README.md` | This repository is a distinct project and must not misrepresent itself as upstream. Attribution retained. |

*(No source changes to `gframe/` or `ocgcore/` yet.)*
