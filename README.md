```
 ▄▄ • ▄• ▄▌ ▐ ▄ .▄▄ · ▄▄▄▄▄ ▄▄▄·  ▄▄· ▄ •▄
▐█ ▀ ▪█▪██▌•█▌▐█▐█ ▀. •██  ▐█ ▀█ ▐█ ▌▪█▌▄▌▪
▄█ ▀█▄█▌▐█▌▐█▐▐▌▄▀▀▀█▄ ▐█.▪▄█▀▀█ ██ ▄▄▐▀▀▄·
▐█▄▪▐█▐█▄█▌██▐█▌▐█▄▪▐█ ▐█▌·▐█ ▪▐▌▐███▌▐█.█▌
·▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀▀  ▀▀▀  ▀  ▀ ·▀▀▀ ·▀  ▀
```

# Description

Collection of modular and minimal libraries used by our software.

These libraries are single-header files meant to be used as unity builds.

All files follow a simple format: 
1. Large "unboxing" comment explaining the library design and usage. 
2. Header guard followed by version, change log and public API.
3. Implementation MACRO and library implementation.
4. License.

# Libraries

Here is a brief list of the libraries in this repo and what they do.

| Name   | Version | Description                                |
| ------ | ------- | ------------------------------------------ |
| Poof   | 0.2.1   | Build system.                              |
| Peak   | 1.0.0   | Single-header platform library with explicit runtime contexts. |
| Fuse   | 0.13.0  | Immediate-mode UI command buffer.          |
| Rend   | 2.0.0   | Modern graphics API layer.                 |
| Rend2  | WIP     | Backend/API refactor; not publicly documented. |
| Grit   | 0.2.1   | Allocators, math, rng, software raster.    |
| Cast   | 0.0.0   | Parse C into an AST... C-AST?              |
| Cool   | 0.1.0   | HTML templating and markdown parsing.      |
| Wire   | 0.1.0   | HTTP server.                               |

# Installing

1. Copy the header(s) required by the selected library.
2. Follow the header's configuration and include-order notes.
3. For most libraries, define the matching implementation macro in exactly one
   translation unit before including the correctly capitalized filename (for
   example, `GRIT_IMPLEMENTATION` and `Grit.h`). Poof is static header-only and
   does not require an implementation macro.

## Build

```sh
cc build.c -o build  # bootstrap once
./build demo <name>  # build the selected demo
./build demo peak    # example
./build demo snake   # example 2
```

## Maturity & Versioning

Most libraries include a version and a change log at the top of the file.
This is helpful if you decide to update one of the libraries you are using in a project.
Libraries that have reached or surpassed the 1.0.0 landmark are generally considered
to be "completed."

## Documentation

We recommend reading the header files first, they have useful comments.

The website separates task-oriented [library guides](site/articles/docs/), a
[demo gallery](site/articles/demos/), and the generated
[Symbols vault](site/articles/symbols/). Guides explain how to use the APIs;
the vault is for looking up individual declarations.

```sh
python3 tools/generate_docs.py          # update the Symbols vault and search data
python3 tools/generate_docs.py --check  # check for stale output without writing
```

The generator preserves handwritten content outside marked vault sections and
needs only Python's standard library. See [the pipeline guide](tools/README.md) for
supported comment syntax, limitations, and tests.

### Website and agent resources

```sh
hugo --source site --destination /tmp/godstack-site --noBuildLock
python3 tools/test_site.py /tmp/godstack-site
```

The active theme requires Hugo 0.146.0 or newer; site CI pins the verified version
0.165.0. Set the production URL with `--baseURL https://your-domain/` when deploying.
The output directory is a static site; no application server or hosted search
service is required. See [the website workflow](tools/README.md#website-workflow).

The site publishes `/llms.txt`, `/llms-small.txt`, `/llms-full.txt`, `/catalog.json`,
and stable Markdown versions of the public documentation. Visit `/ai/` for
copyable prompts and instructions. Downloadable guidance lives in
`site/static/agent/SKILL.md` and `site/static/agent/AGENTS.md`; merge the snippet into
your project's existing instructions rather than replacing them.

- Human guides: `site/articles/docs/` and `site/articles/guides/`.
- API declarations: `site/articles/symbols/`; generated search data: `site/data/symbols.json`.
- Demos: `site/articles/demos/`. Browser workloads are built with
  `python3 tools/build_web_demos.py` (requires Emscripten); the checked-in output
  is statically deployable. Snake uses one C game/rendering source with Rend’s native Vulkan and browser WebGL2 backends.
- Headers and their design notes remain the API source of truth.
- No hosted MCP server or paid offering is currently documented.
