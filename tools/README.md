# Repository tools

## Demo lint and complexity

```sh
python3 tools/lint_demos.py                 # Python 3.10+, standard library only
python3 tools/lint_demos.py --report /tmp/demo-lint.json
python3 tools/lint_demos.py --check-format  # additionally requires clang-format
python3 tools/lint_demos.py --format        # format handwritten C/headers, then lint
python3 tools/lint_demos.py --compile       # Clang, native SDKs, HarfBuzz/FriBidi pkg-config
```

The audit scans handwritten C, headers, Slang, Python and shell sources under
`demos/`. It checks source hygiene, Python/shell syntax, and function complexity.
The default limits are 20 cyclomatic decisions (including the initial path),
160 function lines and 120 code columns. Comments and literal contents are not
charged against C code width. C function lines omit blank/comment-only lines;
Python function lines use AST source spans. The JSON report includes every
function, so a passing run still shows where the largest workloads live.

C/Slang metrics use tokens rather than compilation: branches, loops, case
labels, ternaries and short-circuit operators add paths. Preprocessor directives
and macro bodies are ignored; all conditional source branches are counted
together. Python uses its AST and measures nested functions separately. Shell
scripts get `bash -n` syntax checks, without a shell complexity metric. Compiler
mode checks native C syntax and demo-owned warnings; errors from included
libraries still fail, while library-owned warnings stay outside this audit.

The explicit exclusions are vendored `stb_image.h`, the Teapot mesh arrays,
Cool's generated `view.cool.c`, and Grit's hash-protected historical workload.
Hidden build directories are skipped. Exclusions are listed in the JSON report;
new demo sources are included automatically. Formatting uses `demos/.clang-format`.
Clang-format 22 was used for this cleanup; the dependency-free CI audit does
not require a particular formatter version. This audit complements the existing
CPU, shader and browser integration checks.

## Documentation pipeline

Requires Python 3 (standard library only). No Cast, Doxygen, or compiler is needed.

```sh
python3 tools/generate_docs.py          # regenerate API sections
python3 tools/generate_docs.py --check  # fail if any page is missing or stale
python3 -m unittest discover -s tools -p 'test_*.py'
hugo --source site                    # optional: build the website
```

Commands may be invoked from another working directory using the script's absolute
path. Header and output paths are always resolved relative to the repository.

## Inputs and outputs

The explicit library list in `generate_docs.py` produces one Markdown page for each
of Cast, Cool, Fuse, Grit, Peak, Poof, Rend, Type, and Wire in
`site/articles/symbols/`. Helpers such as FuseRend and demo headers are not inputs.
Rend2 is WIP and excluded from public generation; its existing guide and vault
page are drafts until the backend/API refactor settles.
The same extraction writes `site/data/symbols.json`, which Hugo incorporates into
search and catalog records with stable per-symbol anchors. Library guides in
`site/articles/docs/` are handwritten, task-oriented documentation.
Add a library to `LIBRARIES` when it becomes part of the public collection.

Existing vault front matter and handwritten prose are retained. Only the region between
`<!-- BEGIN GENERATED API -->` and `<!-- END GENERATED API -->` on vault pages is replaced.
If no region exists, it is appended; new pages receive Hugo front matter.
Malformed or duplicated markers are errors. All pages are prepared before writes.

## Writing header comments

The generator extracts public typedefs, named aggregates, function declarations,
and function-like library macros. It accepts leading `/** ... */`, `/*! ... */`,
`///`, `//!`, and ordinary comments, plus same-line trailing comments. Member
comments remain in struct/enum declarations. Multiline declarations and callback
typedefs are supported. For example:

```c
/**
 * @brief Process a value.
 * @param[in] value Input value.
 * @return Zero on success.
 */
int cast_process(int value);
```

`@brief`, `@param`, `@return`/`@returns`, `@note`, `@warning`, `@see`, and
`@code`/`@endcode` are rendered as Markdown (backslash commands also work).
Other commands remain verbatim. Markdown in descriptions is retained. Undocumented
declarations still appear, without invented explanations.

## Scope and limitations

This is a lexical extractor, not a C parser or full Doxygen implementation. It
skips preprocessor directives except function-like library macros, so object-like
constants and configuration flags are not separate API entries. All platform
branches are shown, including alternative declarations of the same symbol.
It does not expand macros, evaluate conditionals, resolve references, or interpret
Doxygen groups and inheritance.

For ordinary single-header libraries it reads from the public header guard to the
implementation directive. Poof has no implementation macro, so its existing
`/* Implementation */` marker is the boundary. Rend2 uses the `rend_` function
prefix. Changes to these layouts may require updating the extractor.

The API extractor does not modify headers, generate guides, delete pages, or
replace the existing HTML documentation demo. The Hugo website combines its
output with handwritten product and task guidance.

## Website workflow

The active theme is `site/themes/godstack-heaven`; content is in `site/articles`.
The new layout convention requires Hugo 0.146.0 or newer. Site CI pins 0.165.0,
the version used for verification. Build into a fresh directory to avoid stale
files from previous builds:

```sh
python3 tools/generate_docs.py --check
hugo --source site --destination /tmp/godstack-site --cacheDir /tmp/godstack-hugo-cache --noBuildLock
python3 tools/test_site.py /tmp/godstack-site
# Optional browser checks: Node 22+ with native WebSocket, Python 3, Chromium.
node tools/test_site_browser.mjs /tmp/godstack-site
```

Set `CHROMIUM=/path/to/chromium` for a different browser executable. Browser
checks serve the built output on localhost and save screenshots in a temporary
directory. They do not require Playwright or a hosted service. Pass `--skip-demos` to
check the documentation shell without running the WebAssembly demos.

The unit-test command also compiles the public CPU-only C examples and runs the
safe ones; Wire's server example is compile-only and never binds a port.

The Particle Observatory gallery image comes from the same native software
renderer used by the demo. Regenerate it with `python3
tools/capture_grit_demo.py` (requires `cc` and `libm`). A temporary harness
includes the demo, initializes its real particle state, advances 120 fixed
steps, and captures the 720×420 software framebuffer without opening a window.
It writes `site/static/images/grit-demo.png` and requires no image library.

For deployment, set `--baseURL https://your-domain/` and publish only the generated
output directory. The checked-in `example.org` URL is a placeholder, not a live
production address. The website is statically deployable and requires no backend.
Search uses Hugo's generated JSON. JavaScript handles local search,
copy feedback, navigation, floating scrollbars, and subtle ocean motion.
The ocean stays still with reduced motion enabled and falls back to its
static illustration when WebGL is unavailable.

### Public documentation outputs

Hugo generates these from the same explicitly selected public content pages:

- `/llms.txt`: introduction, documentation index, and agent resources.
- `/llms-small.txt`: compact setup, reusing the getting-started page's
  `params.agent_setup`, plus essential links.
- `/llms-full.txt`: full Markdown documentation with page boundaries.
- `/catalog.json`: IDs, titles, descriptions, categories, tags, HTML/Markdown
  URLs, product, and access tier.
- `/search.json`: title/description metadata and searchable plain article text.
- `/docs/name.md`, `/guides/name.md`, `/symbols/name.md`, `/demos/name.md`,
  section landing pages at `/docs/index.md` (and equivalents), and public
  licensing, and AI pages. HTML directory URLs are unchanged.

`site/layouts/_partials/is-public-doc.html` explicitly selects the `docs`,
`guides`, `symbols`, and `demos` sections plus the top-level `ai`
and `license` pages. Per-symbol catalog/search records come from the same header
extraction that renders the vault. They link directly to reference anchors;
LLMs page boundaries still describe full documents, not separate files per symbol.
Drafts and pages marked `params.private = true` or `params.access = 'private'` are
excluded from those collections and have no Markdown body exported. This is not authentication: private source must remain
outside the Hugo content/static directories. Use Hugo's build controls to prevent
rendering unpublished pages; never put secrets in a public site input.

Add descriptive front matter to public pages: `description`, and `params.id`,
`product`, `category`, `tags`, `access = 'free'`. IDs should remain stable and
unique. Internal links should point to existing site pages. Machine-readable
outputs use generated permalinks rather than handwritten hostnames.

The downloadable skill and AGENTS.md snippet live in `site/static/agent/`.
They are intentionally project-specific, static guidance, not a runtime
integration. `/ai/` explains how to give resources to Codex, Claude, and Cursor.
No MCP server is implemented or advertised.

### Browser demos and gallery captures

```sh
python3 tools/build_web_demos.py          # compile Grit and Snake using emcc
python3 tools/build_web_demos.py --check  # check source/artifact fingerprints
python3 tools/capture_demo_thumbnails.py  # capture Particle Observatory and Snake gallery frames
```

The Emscripten builder copies an existing compiler cache into a temporary directory
and never installs or modifies the SDK. It stages all requested builds before
publishing them under `site/static/demos/<id>/`. Each `demo.js` contains real
compiled WebAssembly (`SINGLE_FILE`), avoiding runtime WASM fetch/CORS requirements
inside the `allow-scripts` sandbox. Clicking a gallery card opens a minimal
viewer that immediately loads the compiled demo; there is no article or extra
Run prompt. Leaving the page unloads the iframe. Artifacts and manifests can be
deployed without Emscripten installed on the server. Source/header/tool changes
require rebuilding artifacts, not optimistic button labels.

Grit, presented on the site as Particle Observatory, runs a continuous particle
simulation through the shared C software renderer. Space pauses or resumes, R
restarts deterministically, and S toggles stress mode. Reduced-motion preference
starts the simulation paused. Build and run native Grit with
`./build demo grit && ./demos/grit/demo`. `python3
tools/capture_grit_demo.py` records a deterministic 720×420 native frame for
the gallery.
Snake is an interactive 3D game sharing its C game state and cube geometry between
native Vulkan and Rend WebGL2. Peak owns keyboard input and canvas resize events;
Rend presents the scene directly on the GPU. Build just Snake with
`python3 tools/build_web_demos.py snake`; no Slang or Vulkan SDK is required for
that browser build. Native Snake still uses the documented shader tools.
See [WebGL2 validation](../docs/rend-webgl2.md) for executable integration checks.
The thumbnail tool requires a working native Snake executable/device, or accepts
`--snake-ppm` for an existing genuine capture. It does not invent a screenshot
or a browser port.
