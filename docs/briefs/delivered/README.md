# Delivered briefs

Rounds Builder has **delivered** — a PR exists — but that Verifier and Brain
have not yet ruled on. Not accepted, not rejected, not history.

On adjudication a brief moves to [`../archive/`](../archive/) with its outcome
appended. The full lifecycle is in [`../README.md`](../README.md).

This file also exists so the directory does, which is not merely bookkeeping:
git does not track empty directories, so when this directory last emptied, a
fresh CI checkout did not contain it at all and two documents linking here
broke. That was invisible locally — the empty directory still existed on the
author's disk — and only the merge gate caught it. `tests/test_docs_consistency.py`
now resolves links against `git ls-files` rather than the working tree, so a
local run can no longer pass where CI would fail.

This file makes no claim about what this directory currently holds. Run `ls`.
