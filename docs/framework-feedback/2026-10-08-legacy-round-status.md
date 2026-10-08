# Legacy round README becomes a fictitious batch

Project: edopro-next at `169ebfbc60792ed31e419b80f9d61f1b0a72c015`.
Framework: released 4.0.1; its `tools/fw.py` matches this project's 4.0.0 copy.

## What happened

`python3 tools/fw.py status` describes the unmerged round 026 branches as
"batch EADME: Worker summary in" and ends with
"next: ask Brain to review batch EADME". It also mislabels the old round 024
blocked-report branch. There is no batch EADME or Worker summary for it.

## Reproduction

After fetching this project's origin, run `python3 tools/fw.py status` with
the unmerged `verifier/026-native-filter-navigation` branch available.
The command exits 0; the incorrect output is quoted above.

A minimal read-only reproduction from the project root:

```python
import importlib.util
spec = importlib.util.spec_from_file_location("fw", "tools/fw.py")
fw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fw)
print(fw.batch_papers({"docs/rounds/README.md"}))
```

Actual output, exit 0: `(['EADME'], [])`.
Expected output: `([], [])`.

`batches_lines` combines changed batch and legacy-round paths.
`batch_papers` slices every path by the batch-prefix length without checking
that prefix; `docs/rounds/README.md` consequently becomes `EADME.md`.

## Expected result and impact

Legacy work should be identified as a 3.x round with its real branch and
historical reports, not as a completed batch. Brain can inspect git directly,
but the owner's next action is misleading. This feedback changes no copied
framework file. A framework fix should also establish that mixed legacy and
real batch paths are classified correctly.
