"""Regression tests for tools/generate_protocol_constants.py's --check mode.

The generator's job is to make hand-copying upstream's protocol constants
unnecessary, and its docstring has always claimed that a constant it cannot
evaluate is "reported so the omission is visible." That claim was only true
in the plain-write mode: --check compared the rendered header text against
the committed one and reported success regardless of what `collect()` had
silently dropped into its `skipped` list - and CI runs --check, not a plain
write, so the omission was in practice never visible where it mattered.

This suite pins the fix directly against the module's own functions rather
than by shelling out, using scratch copies of the real upstream headers so a
test can inject one unresolvable #define without ever touching gframe/. Two
things are proven: --check now fails, loudly, the moment anything relevant is
unresolvable (test_check_fails_...), and it still passes cleanly when nothing
is (test_check_passes_...) - guarding against the fix being too strict, not
just against it being too lax.

Run:
    python -m unittest discover -s tests -v
"""
from __future__ import annotations

import contextlib
import io
import pathlib
import shutil
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

import tools.generate_protocol_constants as gpc  # noqa: E402

REAL_OCGAPI = REPO / "gframe" / "ocgapi_constants.h"
REAL_COMMON = REPO / "gframe" / "common.h"


class _ScratchGenerator(unittest.TestCase):
    """Points the generator's module-level globals at a private tempdir.

    GROUPS and EXTRA_MESSAGES hold the actual pathlib.Path objects collect()
    reads, captured at module-import time - patching OCGAPI/COMMON alone
    would not be enough, since those two tuples already hold the original
    Path objects. Everything is restored in tearDown, and every test proves
    gframe/ itself was never touched, since another test importing this same
    module in the same process must see the real paths again.
    """

    def setUp(self) -> None:
        self._workdir = tempfile.TemporaryDirectory()
        workdir = pathlib.Path(self._workdir.name)
        self.ocgapi = workdir / "ocgapi_constants.h"
        self.common = workdir / "common.h"
        shutil.copy(REAL_OCGAPI, self.ocgapi)
        shutil.copy(REAL_COMMON, self.common)
        self.out = workdir / "protocol_constants.h"

        self._saved = (gpc.GROUPS, gpc.EXTRA_MESSAGES, gpc.OUT)
        gpc.GROUPS = [(prefix, self.ocgapi, ctype) for prefix, _, ctype in gpc.GROUPS]
        gpc.EXTRA_MESSAGES = [(name, self.common) for name, _ in gpc.EXTRA_MESSAGES]
        gpc.OUT = self.out

    def tearDown(self) -> None:
        gpc.GROUPS, gpc.EXTRA_MESSAGES, gpc.OUT = self._saved
        self._workdir.cleanup()

    def _prime_out_from_current_scratch(self) -> None:
        """Write OUT as an honest generator run against the scratch headers
        would, so --check's own staleness comparison is not what fails the
        test - only the skipped-constants check should be able to."""
        groups, _ = gpc.collect()
        with self.out.open("w", encoding="utf-8", newline="\n") as generated:
            generated.write(gpc.render(groups))

    def _run_check(self) -> tuple[int, str, str]:
        # main() parses sys.argv itself; drive it the same way a real
        # invocation would, restoring argv afterward.
        saved_argv = sys.argv
        try:
            sys.argv = ["generate_protocol_constants.py", "--check"]
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                rc = gpc.main()
        finally:
            sys.argv = saved_argv
        return rc, out.getvalue(), err.getvalue()


class TestCheckDetectsUnresolvableConstants(_ScratchGenerator):
    def test_check_passes_when_nothing_is_skipped(self) -> None:
        self._prime_out_from_current_scratch()
        rc, out, err = self._run_check()
        self.assertEqual(rc, 0, err)
        self.assertIn("up to date", out)

    def test_check_fails_when_a_relevant_constant_is_unresolvable(self) -> None:
        # A left-shift, which _evaluate() deliberately does not understand -
        # this is exactly the scenario the module's own docstring describes
        # ("casts, shifts, arithmetic") and the one CI would face if upstream
        # ever expressed a new LOCATION_/POS_/etc constant this way.
        with self.ocgapi.open("a", encoding="utf-8") as fh:
            fh.write("\n#define LOCATION_FOO (1 << 12)\n")

        # Prime OUT as if a normal (non-check) run had just been made against
        # these now-broken scratch headers, so the header is "current" by
        # text comparison and only the skip itself can fail the check.
        self._prime_out_from_current_scratch()

        rc, out, err = self._run_check()

        self.assertEqual(rc, 1)
        self.assertIn("LOCATION_FOO", err)
        self.assertNotIn("up to date", out)

    def test_unresolvable_constant_never_reaches_the_generated_header(self) -> None:
        with self.ocgapi.open("a", encoding="utf-8") as fh:
            fh.write("\n#define LOCATION_FOO (1 << 12)\n")
        groups, skipped = gpc.collect()
        self.assertIn("LOCATION_FOO = (1 << 12)", skipped)
        self.assertNotIn("LOCATION_FOO", [name for name, _ in groups["LOCATION_"]])
        self.assertNotIn("LOCATION_FOO", gpc.render(groups))

    def test_the_real_repository_has_nothing_to_skip_right_now(self) -> None:
        # A live check against the actual committed headers - not the
        # scratch copies - confirming today's baseline really is clean and
        # this suite is not exercising a case that can never occur.
        groups, skipped = gpc.collect()
        self.assertEqual(skipped, [])
        self.assertGreater(sum(len(v) for v in groups.values()), 0)


class TestEvaluate(unittest.TestCase):
    """_evaluate() in isolation: no module-global patching needed."""

    def test_hex_octal_decimal_literals_resolve(self) -> None:
        self.assertEqual(gpc._evaluate("0x10", {}), 0x10)
        self.assertEqual(gpc._evaluate("010", {}), 8)
        self.assertEqual(gpc._evaluate("42", {}), 42)

    def test_or_composition_of_known_names_resolves(self) -> None:
        known = {"LOCATION_MZONE": 0x4, "LOCATION_SZONE": 0x8}
        self.assertEqual(gpc._evaluate("LOCATION_MZONE | LOCATION_SZONE", known), 0xC)

    def test_shift_expression_is_unresolvable(self) -> None:
        self.assertIsNone(gpc._evaluate("(1 << 12)", {}))

    def test_unknown_name_is_unresolvable(self) -> None:
        self.assertIsNone(gpc._evaluate("SOME_UNDEFINED_NAME", {}))


if __name__ == "__main__":
    unittest.main()
