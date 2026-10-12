+++
title = 'AI and agent documentation'
description = 'Public machine-readable Godstack documentation endpoints and grounded guidance for coding agents.'
[params]
  id = 'ai'
  product = 'Godstack'
  category = 'Platform'
  tags = ['AI', 'agents', 'documentation']
  access = 'free'
+++

Godstack publishes documentation formats that coding assistants and other automated readers can fetch. These are static documentation resources, not an execution or tool backend:

- [llms.txt](/llms.txt) is a compact index for discovering site content.
- [llms-small.txt](/llms-small.txt) and [llms-full.txt](/llms-full.txt) provide different-sized text collections.
- [catalog.json](/catalog.json) is a machine-readable content catalog.
- Individual documentation pages have Markdown output, for example [Cast Markdown](/docs/cast.md) and [Getting started Markdown](/guides/getting-started.md).
- Agent guidance is available as [SKILL.md](/agent/SKILL.md) and [AGENTS.md](/agent/AGENTS.md).

These are explicit URLs to fetch or link from your project; do not assume an editor discovers or loads them automatically. Availability and exact formats depend on the deployed site. There is no MCP server or backend currently available. A possible future MCP adapter could expose catalog search and documentation retrieval, but it does not exist today.

## Add documentation to a coding workflow

### Codex

Fetch [llms-small.txt](/llms-small.txt) and the relevant page's Markdown (for example [Grit](/docs/grit.md)) into the repository, then link the local paths from `AGENTS.md` or a relevant nested `AGENTS.md`. Codex reads repository guidance files; it does not automatically discover this website's resources.

```text
In this repository, first read the local Godstack documentation index and the relevant library's Markdown page, then inspect the corresponding header and demo. Treat the header as the API source of truth. Keep the implementation macro in exactly one translation unit where required, respect documented limitations and ownership, and do not assume APIs from another library. If no local docs are present, fetch `/llms-small.txt` and the relevant `/docs/<library>.md` explicitly from this site's origin. Verify examples by compiling when feasible.
```

### Claude Code

Fetch [llms-small.txt](/llms-small.txt) and the relevant Markdown page, then place their paths and reading instructions in a project guidance file such as `CLAUDE.md` (or the applicable Claude Code project instructions). This file is optional project guidance; a URL alone does not make Claude fetch it.

```text
Before using a Godstack library, read the project-linked llms-small.txt and the relevant per-page Markdown, then inspect the corresponding C header and demo. Follow only documented APIs and limitations; preserve ownership/lifetime requirements and put any implementation macro in one translation unit. Fetch `/llms-small.txt` and the relevant `/docs/<library>.md` explicitly from this site's origin if local documentation is unavailable. Compile examples when feasible.
```

### Cursor

Fetch [llms-small.txt](/llms-small.txt) and the relevant Markdown page into project files and add them to Cursor's project context or reference them in project rules. Cursor does not automatically load these site URLs merely because this page mentions them.

```text
Use the project-provided Godstack llms-small.txt and relevant library Markdown page as discovery aids, then verify APIs against the actual header and demo. Respect the header's version, constraints, and ownership rules. Do not infer unsupported symbols or platform support. If the project has no local docs, explicitly fetch `/llms-small.txt` and `/docs/<library>.md` from this site's origin before coding; compile examples where feasible.
```
