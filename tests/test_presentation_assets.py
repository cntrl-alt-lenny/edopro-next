"""Tests that the README's images do not say something the roadmap contradicts.

Two claims are pinned, each by a mechanism that can fail:

  * `docs/assets/hero.svg` and `docs/assets/social-preview.svg` draw the same
    four layer-stack chips in the same colours. The social preview is a
    re-layout of the banner, so a change to one that the other does not follow
    is a defect (the first version of this repository's social preview
    embedded the banner by reference; the current one is standalone, so it can
    drift, and this is what stops it).
  * While the roadmap marks M2 (the semantic client model) done, the banner
    does not draw its chip dashed or dimmed, which reads as "not built".

Run:
    python -m unittest discover -s tests -v
"""
from __future__ import annotations

import pathlib
import sys
import unittest
import xml.etree.ElementTree as ET

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

import tools.generate_readme_status as grs  # noqa: E402

ASSETS = REPO / "docs" / "assets"
HERO = ASSETS / "hero.svg"
SOCIAL = ASSETS / "social-preview.svg"
SVG = "{http://www.w3.org/2000/svg}"
CHIPS = ("Project Ignis CardScripts", "ocgcore", "semantic model", "Qt 6 / QML")
DIMMED_TEXT = "#6B7280"  # Theme.textTertiary: the colour hero.svg used for "not built"


def chip_styles(svg_text: str) -> dict[str, dict[str, str]]:
    """label -> the chip's own paint: fill, stroke, dash, and its text fill."""
    root = ET.fromstring(svg_text)
    styles: dict[str, dict[str, str]] = {}
    for g in root.iter(f"{SVG}g"):
        rect, text = g.find(f"{SVG}rect"), g.find(f"{SVG}text")
        if rect is None or text is None:
            continue
        label = "".join(text.itertext()).strip()
        if label in CHIPS:
            styles[label] = {
                "fill": rect.get("fill", ""),
                "stroke": rect.get("stroke", ""),
                "dash": rect.get("stroke-dasharray", ""),
                "text": text.get("fill", ""),
            }
    return styles


def semantic_model_done(roadmap_text: str) -> bool:
    return any(ms.title == "Semantic client model" and ms.status == grs.DONE
               for ms in grs.parse_roadmap(roadmap_text))


HERO_TEXT = HERO.read_text(encoding="utf-8")
SOCIAL_TEXT = SOCIAL.read_text(encoding="utf-8")
ROADMAP_TEXT = grs.ROADMAP.read_text(encoding="utf-8")


class ChipsStayInStep(unittest.TestCase):
    def test_both_svgs_draw_all_four_chips(self):
        for name, text in (("hero.svg", HERO_TEXT), ("social-preview.svg", SOCIAL_TEXT)):
            self.assertEqual(sorted(chip_styles(text)), sorted(CHIPS), name)

    def test_social_preview_chips_match_the_banner(self):
        self.assertEqual(chip_styles(SOCIAL_TEXT), chip_styles(HERO_TEXT))

    def test_the_comparison_can_fail(self):
        drifted = SOCIAL_TEXT.replace('fill="#3A3218" stroke="#C9A227"/>\n      <text x="14" y="17" fill="#C9A227">semantic model',
                                      'fill="#16181D" stroke="#3A404C" stroke-dasharray="3 3"/>\n      <text x="14" y="17" fill="#6B7280">semantic model', 1)
        self.assertNotEqual(drifted, SOCIAL_TEXT, "mutation anchor did not match")
        self.assertNotEqual(chip_styles(drifted), chip_styles(HERO_TEXT))


class BannerDoesNotDenyWhatExists(unittest.TestCase):
    def test_semantic_model_chip_is_not_drawn_as_unbuilt_while_m2_is_done(self):
        if not semantic_model_done(ROADMAP_TEXT):
            self.skipTest("roadmap no longer marks the semantic client model done")
        for name, text in (("hero.svg", HERO_TEXT), ("social-preview.svg", SOCIAL_TEXT)):
            chip = chip_styles(text)["semantic model"]
            self.assertEqual(chip["dash"], "", f"{name}: dashed outline reads as 'not built'")
            self.assertNotEqual(chip["text"].upper(), DIMMED_TEXT, f"{name}: dimmed text reads as 'not built'")

    def test_the_check_can_fail(self):
        original = ('<rect x="0" y="0" width="150" height="26" rx="4" fill="#16181D" stroke="#3A404C" stroke-dasharray="3 3"/>\n'
                    '          <text x="14" y="17" fill="#6B7280">semantic model</text>')
        old_hero = HERO_TEXT.replace(
            '<rect x="0" y="0" width="150" height="26" rx="4" fill="#3A3218" stroke="#C9A227"/>\n'
            '          <text x="14" y="17" fill="#C9A227">semantic model</text>', original, 1)
        self.assertNotEqual(old_hero, HERO_TEXT, "mutation anchor did not match")
        chip = chip_styles(old_hero)["semantic model"]
        self.assertEqual(chip["dash"], "3 3")
        self.assertEqual(chip["text"].upper(), DIMMED_TEXT)


class SocialPreviewSource(unittest.TestCase):
    def test_canvas_is_1280_by_640_and_stands_alone(self):
        root = ET.fromstring(SOCIAL_TEXT)
        self.assertEqual(root.get("viewBox"), "0 0 1280 640")
        self.assertEqual((root.get("width"), root.get("height")), ("1280", "640"))
        self.assertEqual(list(root.iter(f"{SVG}image")), [], "external <image> would not travel with the file")


if __name__ == "__main__":
    unittest.main()
