# Brief 007 — make `data/` and `policy/` build under Apple clang

Status: accepted

Delivered by Builder as `85a11055` on PR #21. Verifier reviewed it as part of
`3a2fea97..85a11055`. Merged 2026-09-01 as `f1eb9d66`.

**Process defect, recorded rather than tidied away: this brief was issued as a
launch prompt and never written to `docs/briefs/active.md`.** Verifier said so
in its own report — "brief 007 was not supplied or present in the repository"
— and was right to. It therefore reviewed Correction C against the diff alone,
with no acceptance criteria to check it against, and said so. The brief text
below is reconstructed here so the round is legible cold. The rule this breaks
is `AGENTS.md`'s: durable facts go in the repository, never only in chat.

---

## MODE: IMPLEMENTATION

## Goal

`data/` and `policy/` did not build under Apple clang with the project's own
documented evidence command. Three constants were defined and never used, and
`-Wunused-const-variable` is an error under `-DEDOPRO_NEXT_WERROR=ON`:

```
data/tests/test_card_search.cpp:37       kTypeSpell
data/tests/test_card_search.cpp:39       kTypePendulum
policy/tests/test_deck_validation.cpp:57 kTypeTrap
```

GCC does not emit this warning for namespace-scope `constexpr` under
`-Wall -Wextra`, which is why CI was green at every prior SHA.

## Why it was next

PR #21 existed to make the C++ layers build across compilers, and had already
fixed three MSVC defects of exactly this shape. This was the same class on a
third compiler. It was also a prerequisite: `AGENTS.md`'s evidence table
requires a configure/build/`ctest` cycle for any change to `data/` or
`policy/`, and until this was fixed no round touching those layers could
produce that evidence on macOS at all.

## Scope, non-scope, invariants

Minimum change to make both layers build clean under Apple clang. No
production C++. No weakening of any warning setting — a per-file or
per-target suppression would have counted as weakening. Linux and MSVC not to
regress.

## Required investigation

1. Why does each constant exist — is it residue of a deleted test, or of one
   never written?
2. Are these three the only instances? Ninja stops at the first failing
   target, so the whole picture had to be established.

## Outcome

**Accepted.** Builder removed the three constants rather than suppressing the
warning, which is the right shape. Brain independently verified at
`85a11055`: `client/` 7/7, `data/` 3/3, all three layers building with zero
warnings under clang `-Werror`, and all five required checks green.

Investigation 1 was answered: all three were introduced with their test files
(`49d1967a`, `423029f4`) and never used, so no deleted test is implied.

**Two things this round surfaced that outlived it:**

- `policy/` `ctest` does **not** pass on macOS — `lf_list` fails. Builder
  reported this accurately and unprompted, with the correct mechanism, and
  correctly declined to fix it as out of scope. It is brief 008.
- The acceptance criterion as written ("`data/` and `policy/` each ... pass
  `ctest`") was therefore unsatisfiable on the platform it was written for.
  That was Brain's error in drafting, not Builder's in executing.
