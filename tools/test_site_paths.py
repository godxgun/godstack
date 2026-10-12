"""Website checkout discovery must be explicit and independent of cwd."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parent


class SitePathTests(unittest.TestCase):
    def resolve_site(self, environment, cwd):
        result = subprocess.run(
            [sys.executable, "-c", "from site_paths import SITE; print(SITE)"],
            env=dict(environment, PYTHONPATH=str(TOOLS)), cwd=cwd,
            capture_output=True, text=True, check=True,
        )
        return Path(result.stdout.strip())

    def test_absolute_override_works_outside_repository(self):
        with tempfile.TemporaryDirectory() as directory:
            site = Path(directory) / "external site"
            self.assertEqual(self.resolve_site(dict(os.environ, GODSTACK_SITE=str(site)), directory), site)

    def test_default_uses_home_not_code_checkout(self):
        environment = dict(os.environ)
        environment.pop("GODSTACK_SITE", None)
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(self.resolve_site(environment, directory), Path.home() / "rsr/src/site")


if __name__ == "__main__":
    unittest.main()
