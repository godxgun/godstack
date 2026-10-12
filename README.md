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
| Grit   | 0.2.1   | Allocators, math, rng, software raster.    |
| Cast   | 0.0.0   | Parse C into an AST... C-AST?              |
| Cool   | 0.1.0   | HTML templating and markdown parsing.      |
| Wire   | 0.1.0   | HTTP server.                               |

# Installing

1. Copy the single header file.
2. Follow the header's configuration and include-order notes.
3. For most libraries, define the matching implementation macro in exactly one
   translation unit before including the correctly capitalized filename (for
   example, `GRIT_IMPLEMENTATION` and `Grit.h`). Poof is static header-only and
   does not require an implementation macro.

```c
#define LIBRARY_IMPLEMENTATION
#include "Library.h"
```

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

## Website and agent resources

The site under `site-public` publishes `/llms.txt`, `/llms-small.txt`, `/llms-full.txt`, `/catalog.json`,
and stable Markdown versions of the public documentation. Visit `/ai/` for
copyable prompts and instructionss rather than replacing them.

## License

Everything is MIT licensed. One font used in demos is only for personal use. 
Some stb headers are embedded, MIT license applies there.
