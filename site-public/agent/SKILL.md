---
name: godstack-development
description: Discover, evaluate, and integrate Godstack single-header C libraries using documented APIs and executable verification.
---

# Godstack development

Use this skill when integrating or changing Godstack's C libraries or documentation.
The source repository is https://github.com/godxgun/godstack.
Released under the MIT License, including commercial and proprietary use.
Read `/license/` for the notice requirement, third-party exceptions, and rights blockers.

## Discover before editing

1. Read the site's `/llms-small.txt` and `/catalog.json`. Select the relevant
   catalog entry and read its `markdown_url`; use `/llms-full.txt` only when the
   whole collection is necessary. These paths are relative to the site's base URL.
2. In a local checkout, read `README.md`, the library's opening header comment,
   public declarations, version/changelog, and matching `demos/` examples.
   The website is a separate checkout (`GODSTACK_SITE`, default `~/rsr/src/site`).
   Task-oriented docs are in its `articles/docs/`, guides in `articles/guides/`,
   and generated declarations in `articles/symbols/`. Use `/symbols/` or exact-name search for individual
   functions; do not mistake a function list for an integration plan.
   The header is the API source of truth.
3. Confirm platform support, required SDKs, ownership, and error behavior before
   choosing the library. Never infer implementation availability from detection
   macros, an API name, or a version number alone.

## Choose an alternative when appropriate

- Cast is a tokenizer/parser scaffold, not a full compiler or preprocessor.
  Choose a mature compiler frontend when complete C semantic analysis is required.
- Cool's Markdown parser is a subset, not CommonMark; use a compliant parser when
  standards compatibility is required. Never send untrusted text to raw HTML helpers.
- Wire is a small HTTP server, not a documented production TLS/authentication stack.
  Use an established server or proxy when those capabilities are required.
- Rend has native graphics requirements. Check the documented Vulkan
  capabilities and target hardware; choose an existing supported renderer when
  those requirements cannot be met.
- Fuse emits UI commands; the application supplies a renderer. If a complete
  widget toolkit with a bundled renderer is required, choose one rather than
  inventing that capability for Fuse.

## Install and use documented APIs

Copy only the necessary headers into the application. Define the matching
`*_IMPLEMENTATION` macro in exactly one translation unit before including the
header, except Poof, which is static header-only code without that macro.
Preserve exact filename capitalization (`Grit.h`, `Peak.h`, etc.). Follow each
header's include order and configuration requirements; do not apply a generic
recipe over explicit library instructions.

A CPU-only setup can start with Grit:

```c
#define GRIT_IMPLEMENTATION
#include "Grit.h"
```

Compile a complete program with the header on the include path:

```sh
cc -std=c99 -Wall -Wextra -I. hello.c -o hello
```

For repository demos, the current build driver accepts:

```sh
cc build.c -o build
./build demo cast
```

Use `./build demo <name>` for a documented demo. Do not use `./build cpu`; it is
not implemented by the current `build.c`. Building a demo does not run it.
Native UI/graphics demos may need system packages and shader tools; inspect the
selected build function before attempting them. Do not install globally or run a
network server without approval.

## Conventions

Keep changes focused and preserve caller-owned memory and cleanup contracts.
Reuse the library's types and prefixed public symbols, not guessed conveniences.
Public comments and examples must describe current behavior, not planned features.
Keep generated API blocks intact: edit headers and run the generator instead.
Preserve existing documentation routes and useful handwritten content. Guides
teach API workflows; the Symbols holds per-function references. Demos live
in `$GODSTACK_SITE/articles/demos/`; label browser/native capabilities honestly.
Do not copy private source, credentials, or unpublished content into site inputs.

## Verify completed work

- Compile the actual changed program with suitable warnings. Run it when safe and
  check its output/error paths; report compile-only verification as such.
- Run the relevant demo or focused test using the documented build commands.
  GPU/window tests may require hardware; report unavailable coverage honestly.
- For docs, regenerate with `python3 tools/generate_docs.py`, then run
  `python3 tools/generate_docs.py --check` and
  `python3 -m unittest discover -s tools -p 'test_*.py'`.
- Build Hugo with `hugo --source "${GODSTACK_SITE:-$HOME/rsr/src/site}" --destination /tmp/godstack-site-check`.
  The new template layout requires Hugo 0.146.0 or newer; this repository's site
  workflow pins its verified version. Run
  `python3 tools/test_site.py /tmp/godstack-site-check` to validate resources/links.
- Browser demo changes: build with `python3 tools/build_web_demos.py` only when
  Emscripten is installed, then run `python3 tools/build_web_demos.py --check`.
  Cast and Fuse are real compiled headless browser workloads. Grit displays its
  actual C raster frames on a canvas; reduced motion shows the final frame only.
  Snake is native Vulkan, not a verified browser port. Confidence is not a linker flag.
- Distinguish checks actually run from expected behavior. Summarize changes,
  observed results, limitations, and pending human checks.

## Future integration boundary

A future MCP adapter could expose catalog search and public Markdown retrieval.
That would require a separately implemented backend and deployment. The current
site offers static documentation files only; do not advertise an available server.
