"""Checks the landing page against the owner's README standard (brief 014).

The standard is `standards/readme.md` in cntrl-alt-lenny/agentic-framework at
fed26f360294baddedc74eeaabbaf2e716572260, with the owner's 2026-09-21 rulings
applied where they differ from it. What is checked here is the part a machine
can honestly check offline:

  * visible words are within 250-500 (method below);
  * the shape: the standard's headings, in its order;
  * badges: four or five, shields.io `flat` (plus the repository's own CI
    badge), CI first and license last, each linking to evidence, no status or
    milestone badge, and a static AGPL-3.0-or-later license badge that links
    to LICENSE;
  * the credits, trademark and "no game data included" notices stay;
  * every relative link and image in the README and the pages that moved out
    of it resolves, including same-file and cross-file heading anchors;
  * the quick start's `cmake -S <dir>` targets exist;
  * the 1280x640 social-preview PNG exists and is exactly that size.

What it does not check, and cannot: that a badge URL is live (that is a
network probe, run by hand and reported), or that the page looks right. The
"What works" block is checked in tests/test_readme_status.py.

# Word-count method

`visible_words()` strips HTML comments and images, keeps the text of links,
drops other HTML tags and markdown punctuation, drops any `<details>` block
except its `<summary>` text, and counts whitespace-separated tokens that
contain a letter or digit. Fenced code is counted, because it is visible. It is
a count of the Markdown source, not of the rendered page; the two agree closely
on this README, and the rendered HTML from GitHub's own renderer was counted
separately when the brief was delivered.
"""
from __future__ import annotations

import pathlib
import re
import struct
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent
README = REPO / "README.md"

# The pages that received the material cut from the landing page. Their
# relative links are checked too, since a moved page with dead links is a
# regression the move itself introduced.
MOVED_PAGES = [
    REPO / "docs" / name
    for name in (
        "overview.md",
        "capabilities.md",
        "making-change-provable.md",
        "building.md",
        "contributing.md",
    )
]

WORD_MIN, WORD_MAX = 250, 500

REQUIRED_HEADINGS = [
    "What is this?",
    "Quick start",
    "What works",
    "Documentation",
    "Credits and license",
]

_IMAGE_MD = re.compile(r"!\[[^\]]*\]\([^)]*\)")
_LINK_MD = re.compile(r"\[([^\]]*)\]\(([^)]*)\)")
_IMG_TAG = re.compile(r"<img\b[^>]*>", re.IGNORECASE)
_TAG = re.compile(r"<[^>]+>")
_COMMENT = re.compile(r"<!--.*?-->", re.DOTALL)
_DETAILS = re.compile(r"<details\b.*?</details>", re.DOTALL | re.IGNORECASE)
_SUMMARY = re.compile(r"<summary\b[^>]*>(.*?)</summary>", re.DOTALL | re.IGNORECASE)


def visible_words(markdown: str) -> int:
    text = markdown.replace("\r\n", "\n")
    text = _COMMENT.sub("", text)

    def keep_summary(match: re.Match) -> str:
        summary = _SUMMARY.search(match.group(0))
        return summary.group(1) if summary else ""

    text = _DETAILS.sub(keep_summary, text)
    text = _IMAGE_MD.sub("", text)
    text = _IMG_TAG.sub("", text)
    text = _LINK_MD.sub(lambda m: m.group(1), text)
    text = _TAG.sub(" ", text)
    text = re.sub(r"[*_`>|#]", " ", text)
    return sum(1 for token in text.split() if re.search(r"[A-Za-z0-9]", token))


def github_slug(heading: str) -> str:
    """GitHub's heading anchor: lower-case, punctuation dropped, spaces to hyphens."""
    heading = re.sub(r"[`*_]", "", heading.strip().lower())
    heading = re.sub(r"[^\w\- ]", "", heading, flags=re.UNICODE)
    return heading.replace(" ", "-")


def anchors_of(path: pathlib.Path) -> set[str]:
    anchors = set()
    in_fence = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("```"):
            in_fence = not in_fence
        if in_fence:
            continue
        m = re.match(r"^#{1,6}\s+(.*?)\s*#*\s*$", line)
        if m:
            anchors.add(github_slug(m.group(1)))
    return anchors


def local_targets(markdown: str) -> list[str]:
    """Repository-relative link and image targets, as written."""
    text = _COMMENT.sub("", markdown)
    targets = [m.group(2).strip() for m in _LINK_MD.finditer(text)]
    targets += re.findall(r"""<img\b[^>]*\bsrc=["']([^"']+)["']""", text, re.IGNORECASE)
    targets += re.findall(r"""<a\b[^>]*\bhref=["']([^"']+)["']""", text, re.IGNORECASE)
    return [
        t for t in targets
        if t and "://" not in t and not t.startswith(("mailto:", "data:"))
    ]


def badges(markdown: str) -> list[tuple[str, str, str]]:
    """(alt, image URL, link target) for every `[![alt](image)](target)`."""
    return re.findall(r"\[!\[([^\]]*)\]\(([^)]*)\)\]\(([^)]*)\)", markdown)


class LandingPageShapeTest(unittest.TestCase):
    def setUp(self):
        self.text = README.read_text(encoding="utf-8")

    def test_visible_word_count_is_within_the_standard(self):
        n = visible_words(self.text)
        self.assertGreaterEqual(n, WORD_MIN, f"{n} visible words; the standard asks for {WORD_MIN}-{WORD_MAX}")
        self.assertLessEqual(n, WORD_MAX, f"{n} visible words; the standard asks for {WORD_MIN}-{WORD_MAX}")

    def test_word_counter_ignores_what_is_not_visible(self):
        sample = (
            "one two <!-- hidden words here -->\n"
            "![alt words](x.png) [link text](y.md) `three`\n"
            "<details><summary>four</summary>five six seven</details>\n"
            "<img src='a.png' alt='no words'> ---\n"
        )
        self.assertEqual(visible_words(sample), 6)  # one two link text three four

    def test_sections_appear_in_the_standards_order(self):
        headings = [
            m.group(1).strip()
            for m in re.finditer(r"^##\s+(.*)$", self.text, re.MULTILINE)
        ]
        self.assertEqual(headings, REQUIRED_HEADINGS)

    def test_hero_banner_is_kept_and_present(self):
        self.assertIn("docs/assets/hero.svg", self.text)
        self.assertTrue((REPO / "docs" / "assets" / "hero.svg").is_file())

    def test_quick_start_build_targets_exist(self):
        for source in re.findall(r"cmake\s+-S\s+(\S+)", self.text):
            with self.subTest(source=source):
                self.assertTrue((REPO / source / "CMakeLists.txt").is_file())

    def test_no_news_style_panels(self):
        lowered = self.text.lower()
        for phrase in ("what's new", "recent commits", "latest news"):
            self.assertNotIn(phrase, lowered)


class BadgeTest(unittest.TestCase):
    def setUp(self):
        self.text = README.read_text(encoding="utf-8")
        self.badges = badges(self.text)

    def test_four_or_five_badges(self):
        self.assertIn(len(self.badges), (4, 5), self.badges)

    def test_ci_first_license_last(self):
        self.assertIn("actions/workflows/edopro-next.yml/badge.svg", self.badges[0][1])
        self.assertIn("/badge/license-", self.badges[-1][1])

    def test_shields_badges_are_flat(self):
        for alt, image, _ in self.badges:
            if "img.shields.io" in image:
                with self.subTest(badge=alt):
                    self.assertIn("style=flat", image)
                    self.assertNotIn("for-the-badge", image)

    def test_every_badge_links_to_evidence_that_exists(self):
        for alt, _, target in self.badges:
            with self.subTest(badge=alt):
                self.assertTrue(target)
                if "://" in target:
                    self.assertTrue(target.startswith("https://github.com/cntrl-alt-lenny/edopro-next/"))
                else:
                    self.assertTrue((REPO / target).exists(), target)

    def test_no_development_stage_or_milestone_badge(self):
        """Owner's ruling: both are changeable facts, and nothing may state one by hand."""
        for alt, image, _ in self.badges:
            with self.subTest(badge=alt):
                blob = (alt + " " + image).lower()
                for word in ("status", "milestone", "progress", "early", "development", "stage"):
                    self.assertNotIn(word, blob)

    def test_license_badge_is_static_and_links_to_license(self):
        alt, image, target = self.badges[-1]
        self.assertEqual(alt, "license")
        self.assertIn("AGPL--3.0--or--later", image)
        self.assertNotIn("/github/license/", image)  # the live badge would read "not specified"
        self.assertEqual(target, "LICENSE")

    def test_license_file_is_untouched_upstream_text(self):
        """Non-scope check: the static badge exists because LICENSE must stay verbatim."""
        head = (REPO / "LICENSE").read_text(encoding="utf-8", errors="replace")
        self.assertIn("GNU AFFERO GENERAL PUBLIC LICENSE", head.upper())


class NoticesTest(unittest.TestCase):
    """The credits and trademark notice, and the no-game-data statement, stay put."""

    def setUp(self):
        self.text = " ".join(README.read_text(encoding="utf-8").split())

    def test_credits(self):
        for name in ("Project Ignis", "edo9300"):
            self.assertIn(name, self.text)

    def test_trademark_and_non_affiliation(self):
        self.assertIn("Yu-Gi-Oh! is a trademark of Shueisha and Konami", self.text)
        self.assertIn("not affiliated with or endorsed by Project Ignis, Konami or Shueisha", self.text)

    def test_no_game_data_is_included(self):
        self.assertIn("Card scripts, card databases and card artwork are **not** in this repository", self.text)

    def test_license_statement(self):
        self.assertIn("GNU AGPL v3 or later", self.text)
        self.assertIn("inherited code is not relicensed", self.text)


class LinkTest(unittest.TestCase):
    def _check_page(self, page: pathlib.Path):
        markdown = page.read_text(encoding="utf-8")
        for target in local_targets(markdown):
            with self.subTest(page=page.relative_to(REPO).as_posix(), target=target):
                path_part, _, anchor = target.partition("#")
                resolved = page if not path_part else (page.parent / path_part).resolve()
                self.assertTrue(resolved.exists(), f"{target} does not exist")
                if anchor and resolved.suffix == ".md":
                    self.assertIn(
                        anchor, anchors_of(resolved),
                        f"{target}: no heading with that anchor in {resolved.name}",
                    )

    def test_readme_links_and_images_resolve(self):
        self._check_page(README)

    def test_moved_pages_links_and_images_resolve(self):
        for page in MOVED_PAGES:
            with self.subTest(page=page.name):
                self.assertTrue(page.is_file())
            if page.is_file():
                self._check_page(page)

    def test_slug_function_matches_github_for_a_known_heading(self):
        self.assertEqual(github_slug("5. The chosen mechanism and why"), "5-the-chosen-mechanism-and-why")
        self.assertEqual(github_slug("What is this?"), "what-is-this")


class SocialPreviewTest(unittest.TestCase):
    PNG = REPO / "docs" / "assets" / "social-preview.png"

    def test_png_exists_and_is_exactly_1280_by_640(self):
        self.assertTrue(self.PNG.is_file(), "docs/assets/social-preview.png is missing")
        data = self.PNG.read_bytes()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(data[12:16], b"IHDR")
        width, height = struct.unpack(">II", data[16:24])
        self.assertEqual((width, height), (1280, 640))


if __name__ == "__main__":
    unittest.main()
