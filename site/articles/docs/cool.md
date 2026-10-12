+++
title = 'Cool'
description = 'Use Cool output helpers for escaped text, trusted markup, and a limited Markdown subset.'
weight = 70
[params]
  id = 'cool'
  product = 'Cool'
  category = 'Web'
  tags = ['C', 'HTML', 'Markdown']
  access = 'free'
  audience = 'Core'
+++

# Cool

Cool generates HTML from C. It writes straight to a `FILE *` stream: escaped text, raw markup,
and a small Markdown subset. A separate transpiler turns `.cool` template files into plain C
functions that call the same helpers. Cool does not allocate, buffer or sanitize markup for you.

Define `COOL_IMPLEMENTATION` in exactly one translation unit before including `Cool.h`. Other
translation units include the header without it.

## Writing Text

The first thing to try is writing user text into a page. `cool_html_txt()` escapes `<`, `>` and
`&` and writes the result to standard output:

```c
#define COOL_IMPLEMENTATION
#include "Cool.h"

int main(void)
{
    cool_html_txt("Hello & welcome", 15);
    return 0;
}
```

This prints `Hello &amp; welcome`. The length is in bytes; output also stops at the first NUL.
`cool_html_txt()` is meant for element content. It does not escape quotes, so it is not safe for
untrusted values placed inside attribute values.

## Writing Markup

Markup you control is written unescaped. `COOL_SV()` wraps a string literal with its length;
`cool_html_raw_cstr()` takes any pointer and length; `cool_htmlf_raw()` is `printf` into the
stream. None of the raw helpers escape anything, so use them for literals and numbers, and send
every user-supplied string through `cool_html_txt()`.

```c
static const char *const names[] = {"Peak", "Rend", "Fuse <beta>"};

cool_html_raw(COOL_SV("<ul class=\"libs\">\n"));
for (int i = 0; i < 3; ++i) {
    cool_htmlf_raw("  <li data-index=\"%d\">", i);
    cool_html_txt(names[i], strlen(names[i]));
    cool_html_raw(COOL_SV("</li>\n"));
}
cool_html_raw(COOL_SV("</ul>\n"));
```

The third name is written as `Fuse &lt;beta&gt;`.

## Choosing the Output Stream

Output goes to `COOL_OUTPUT`, which defaults to `stdout`. Define it before including the header.
It is evaluated on every write, so it can name a variable that you change at runtime. On POSIX
systems, `open_memstream()` captures a page in memory, ready to send as a response body:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

static FILE *cool_out;
#define COOL_OUTPUT cool_out
#define COOL_IMPLEMENTATION
#include "Cool.h"

int main(void)
{
    char *page = NULL;
    size_t size = 0;

    cool_out = open_memstream(&page, &size);
    if (!cool_out) return 1;
    cool_html_raw(COOL_SV("<h1>"));
    cool_html_txt("Status", 6);
    cool_html_raw(COOL_SV("</h1>\n"));
    fclose(cool_out); /* page and size are valid after the stream is closed */

    fwrite(page, 1, size, stdout);
    free(page);
    return 0;
}
```

The repository's doc generator uses this pattern to answer [Wire](/docs/wire/) requests.

## Rendering Markdown

`cool_md()` converts a Markdown subset to HTML and writes it to the same stream:

```c
static const char notes[] =
    "## Release\n"
    "\n"
    "The build is **ready**. See [the guide](/docs/cool/).\n"
    "\n"
    "- `cool_md` writes HTML\n"
    "- text is *escaped*\n"
    "\n"
    "| Library | Status |\n"
    "| ------- | ------ |\n"
    "| Cool    | ok     |\n";

cool_md(notes, sizeof notes - 1);
```

The supported subset is ATX headings (`#` to `######`), paragraphs, `-`/`*`/`+` bullet lists,
`1.` ordered lists, `---` rules, fenced code blocks (a simple language name becomes
`class="language-..."`), pipe tables, inline code, `**strong**`, `*emphasis*` and
`[text](url)` links. Nested lists, block quotes, images, setext headings and underscore emphasis
are not recognized; such lines come out as paragraph text.

Text and code are escaped, so HTML embedded in the Markdown is displayed rather than interpreted.
Link targets are escaped for the attribute, but their scheme is not checked: a `javascript:` URL
passes through. Filter links yourself when the Markdown comes from users.

## Compiling Templates

For larger pages, write markup in a `.cool` file and let `CoolTranspiler.c` turn each template
into a C function. A template starts with a line beginning with `COOL` followed by a C function
header, and ends with a line containing only `}`:

```html
COOL void LibCard(char *name, int version) {
    <div class="card">
        <h3>{ name }</h3>
        <span>v{ version }</span>
    </div>
}
```

Inside a template, `{ name }` inserts a parameter. `char *` parameters are written with
`cool_html_txt()`; numeric parameters go through `cool_htmlf_raw()` with a matching format.
An expression that is not exactly a parameter name is a transpile error. `@Func(args)` emits a
call to another C function, such as another template. Lines outside templates are copied to the
output unchanged.

Build the transpiler once, then generate C from the template:

```sh
cc -std=c99 -Wall -Wextra -I. CoolTranspiler.c -o cool_transpiler
./cool_transpiler view.cool -o view.cool.c
```

The generated file is a set of plain functions. Include it after `Cool.h` and `<string.h>`,
since text parameters are measured with `strlen()`:

```c
#include <string.h>
#define COOL_IMPLEMENTATION
#include "Cool.h"
#include "view.cool.c"

int main(void)
{
    LibCard("Cool", 1);
    return 0;
}
```

Parameters are escaped as element text, not as attribute values, so do not place untrusted
strings inside quoted attributes in a template. See
[`demos/doc-generator/`](https://github.com/godxgun/godstack/tree/main/demos/doc-generator) for a
template, its generated C, and a server that renders it.

See [Cool symbols](/symbols/cool/) for the generated declarations.
