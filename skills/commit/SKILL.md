---
name: commit
description: >
  Write or amend a GunStack commit. Subject is only
  "<ModuleName> MAJOR.MINOR.PATCH" per updated module. Use when asked
  to commit, amend, write a commit message, or check that a commit
  follows this format.
---

If the target is unclear, ask once. Else start now.
Do not invent a body. Details live in library headers.
One pass: align headers, then commit or amend. Stop.

# Subject

One line. No prose.

    <ModuleName> MAJOR.MINOR.PATCH

A second update is another sentence on the same line:

    Cool 0.0.0. Rend 1.0.4.

- ModuleName matches the directory / public header (`Cool`, `Rend`, `Peak`).
- Version is the header macros after this commit, not the previous tag.
- List every module whose sources, public header, or version changed.
- Do not list README, `build.c`, demos, or binaries as modules.
- Do not write "Added", "Fixed", "Updated", or a reason.

# Headers are the record

Each module header owns `MOD_MAJOR` / `MOD_MINOR` / `MOD_PATCH` and a
`CHANGE LOG`. Macros may be integers or quoted strings.

    #define REND_MAJOR 1
    #define REND_MINOR 0
    #define REND_PATCH 4

    /* CHANGE LOG
     * 1.0.4 - @vasco - vulkan backend collapsed; renderer create fails cleanly
     */

Changelog line: `* X.Y.Z - @user - brief note`. `@user` is the git
author's usual handle (`@vasco`). One line. Same brevity as the subject.

Bump before you commit if the module actually changed:

- MAJOR — breaking API
- MINOR — non-breaking feature
- PATCH — fix, refactor, or internal collapse with no API change

If the module did not change, do not bump it and do not name it.

# Check

Before `git commit` or `git commit --amend`:

1. `git status` and the staged / HEAD diff. Name the modules that moved.
2. Read each module's public header macros and last changelog line.
3. If the subject would claim a version the header does not have, bump
   the macros and append the changelog line first. Stage those edits.
4. If the header was bumped with no matching source change, do not
   claim it unless that bump is the change.
5. Subject versions must equal the header macros you are committing.

Amend only when the user asked, or the latest commit is yours, unpushed,
and they asked to fix its message. Keep author and date unless told
otherwise.

    git commit --amend -m "Cool 0.0.0. Rend 1.0.4."

Do not push.
