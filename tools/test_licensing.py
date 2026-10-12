#!/usr/bin/env python3
"""Focused checks for MIT licensing and preserved third-party notices."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADERS = ("Cast", "Cool", "Fuse", "FuseRend", "Grit", "Peak", "Poof",
           "Rend", "Type", "Wire")


class LicensingTests(unittest.TestCase):
    def test_root_mit_license(self):
        text = (ROOT / "LICENSE").read_text()
        self.assertIn("MIT License", text)
        self.assertIn("Copyright (c) 2026 Vasco Alves", text)
        self.assertIn("Permission is hereby granted, free of charge", text)
        self.assertIn('THE SOFTWARE IS PROVIDED "AS IS"', text)

    def test_owned_header_notices(self):
        license_body = (ROOT / "LICENSE").read_text().split("Copyright (c) 2026 Vasco Alves\n", 1)[1]
        for library in HEADERS:
            with self.subTest(library=library):
                source = (ROOT / f"{library}.h").read_text()
                self.assertIn("MIT License", source)
                self.assertIn(license_body, source)
                self.assertNotIn("GPL-3.0-only", source)
                self.assertNotIn("COMMERCIAL-LICENSE.md", source)

    def test_third_party_notices_remain(self):
        peak = (ROOT / "Peak.h").read_text()
        self.assertIn("Copyright © 2008-2013 Kristian Høgsberg", peak)
        self.assertIn("including the next", peak)
        stb = (ROOT / "demos/teapot/stb_image.h").read_text()
        self.assertIn("Copyright (c) 2017 Sean Barrett", stb)
        self.assertIn("ALTERNATIVE A - MIT License", stb)
        self.assertIn("ALTERNATIVE B - Public Domain", stb)
        audit = (ROOT / "THIRD-PARTY-NOTICES.md").read_text()
        for asset in ("Blue Screen Personal Use.ttf", "teapot.h", "wal.jpg"):
            self.assertIn(asset, audit)


if __name__ == "__main__":
    unittest.main()
