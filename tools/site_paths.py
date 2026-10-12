"""Location of the separately maintained Godstack website checkout."""

import os
from pathlib import Path

# Explicit override works on other machines and in CI; never depend on cwd.
SITE = Path(os.environ.get("GODSTACK_SITE", Path.home() / "rsr/src/site")).expanduser().resolve()
