# Godstack guidance (snippet)

Merge this snippet into your project's existing AGENTS.md; do not overwrite
unrelated instructions. This is guidance, not an executable backend.

- Discover public docs through `/llms-small.txt` and `/catalog.json` on the deployed
  Godstack site. Read the selected entry's `markdown_url` and the corresponding
  header and demo in https://github.com/godxgun/godstack. Prefer `/docs/` for
  workflows and `/symbols/` for individual declarations; exact symbol searches
  link to vault anchors.
- Copy only the headers needed. For each library requiring it, define its
  implementation macro in exactly one translation unit. Poof is the static
  header-only exception. Keep filename case and documented include order.
- Use documented public APIs, ownership rules, and configuration only. Cast is a
  parser scaffold, Cool is not CommonMark, Fuse needs your renderer, and native
  graphics/platform libraries need their documented SDKs. Choose a mature
  alternative when the required capability is outside those contracts.
- Repository demo build: `cc build.c -o build`, then `./build demo <name>`.
  `./build cpu` is not a current command. Building is not running.
- Compile focused C99 examples with warnings; run appropriate tests when safe.
  Report missing GPU/platform coverage. For docs, run the generator and
  `python3 tools/generate_docs.py --check`, unit tests, Hugo, and
  `python3 tools/test_site.py <output-directory>`.
- The website lives outside the Godstack code checkout: set GODSTACK_SITE
  (default ~/rsr/src/site). Rend2 builds and checks live in ~/rsr/src/godstack-pro.
- Preserve existing documentation routes and useful prose. Do not hand-edit
  generated API blocks or export private source. State what changed, checks
  actually run, and any remaining limitations.
- `/demos/` separates compiled browser workloads from native demos. Validate
  checked-in browser artifacts with `python3 tools/build_web_demos.py --check`.
  Do not invent a browser Snake port: the current game uses native Vulkan.
- Released under the MIT License. Preserve third-party terms and copyright
  notices; review `/license/` and repository rights blockers before distribution. No
  hosted MCP service is documented.
  See `/agent/SKILL.md` for the full workflow and extension boundary.
