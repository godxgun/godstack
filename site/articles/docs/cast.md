+++
title = 'Cast'
description = 'A practical guide to tokenizing and walking C source with caller-owned memory.'
weight = 70
[params]
  id = 'cast'
  product = 'Cast'
  category = 'Utilities'
  tags = ['C', 'parser', 'AST']
  access = 'free'
  audience = 'Core'
+++

# Cast

Cast (C AST) reads C source from memory and gives you its tokens and a tree of its file-scope
declarations: variables, function prototypes and definitions, their types, pointers, arrays and
parameters. It is meant for tools that need to look at C code, such as binding generators,
documentation tools and template compilers. It does not load files, does not allocate, and skips
preprocessor lines instead of expanding them.

Define `CAST_IMPLEMENTATION` in exactly one translation unit before including `Cast.h`. Other
translation units include the header without it.

## Tokenizing Source

The first thing to try is splitting a line of C into tokens:

```c
#include <stdio.h>
#include <string.h>
#define CAST_IMPLEMENTATION
#include "Cast.h"

int main(void)
{
    unsigned char storage[4096];
    char source[] = "int answer;";
    CastMemory memory = cast_memory_create(storage, sizeof storage);
    size_t count = 0;
    CastToken *tokens = cast_tokenize(&memory, source, strlen(source), &count);
    if (!tokens) return 1;
    printf("tokens: %zu\n", count);
    return 0;
}
```

This prints `tokens: 3`. `cast_memory_create()` turns your buffer into an arena; every token,
node and name Cast produces is stored in it. `cast_tokenize()` returns an array of `count` tokens,
or NULL when the input is empty or the arena is too small.

Tokens do not copy text. Each `CastToken` has a `type` and the byte offset (`arg`) and length
(`arglen`) of its text in the source, so keep the source alive and unchanged while you use the
results. Comments and lines starting with `#` are skipped; macros are not expanded.

## Reading Tokens

`cast_token_type()` names a token type: `IDENT`, `KW` (keyword), `NUMBER`, `STRING`, `CHAR`,
`PUNCT` or `ERROR`. Print the text through the offset and length:

```c
for (size_t i = 0; i < count; ++i) {
    CastToken t = tokens[i];
    printf("%-6s %.*s\n", cast_token_type(t.type), t.arglen, source + t.arg);
}
```

For `int answer;` this prints:

```text
KW     int
IDENT  answer
PUNCT  ;
```

An unterminated string or a character C does not use comes back as an `ERROR` token instead of
stopping the tokenizer.

## Parsing Declarations

`cast_ast()` parses the tokens into a tree. Node 0 is the translation unit (`"tu"`); its children
are the file-scope declarations in source order. Walk the tree with `first_child` and
`next_sibling`; an index of zero ends a list, since the root is never anyone's child:

```c
#include <stdio.h>
#include <string.h>
#define CAST_IMPLEMENTATION
#include "Cast.h"

static void describe(CastAst *ast, CastNode node)
{
    const char *type = "?";
    int stars = 0, arrays = 0;
    for (size_t i = node.first_child; i; i = cast_ast_node(ast, i).next_sibling) {
        CastNode child = cast_ast_node(ast, i);
        if (strcmp(child.msg, "type") == 0) type = child.name;
        else if (strcmp(child.msg, "ptr") == 0) stars++;
        else if (strcmp(child.msg, "array") == 0) arrays++;
    }
    printf("%s", type);
    if (stars || node.name)
        printf(" %.*s%s", stars, "********", node.name ? node.name : "");
    while (arrays--) printf("[]");
}

int main(void)
{
    static unsigned char storage[64 * 1024];
    char source[] =
        "#include <stdio.h>\n"
        "static const char *lookup(int id, char **out);\n"
        "int counts[16], total;\n"
        "void reset(void) { total = 0; }\n";

    CastMemory memory = cast_memory_create(storage, sizeof storage);
    size_t token_count = 0, node_count = 0;
    CastToken *tokens = cast_tokenize(&memory, source, strlen(source), &token_count);
    if (!tokens) return 1;
    CastAst *ast = cast_ast(&memory, tokens, token_count, &node_count);
    if (!ast) return 1;

    CastNode root = cast_ast_node(ast, 0);
    for (size_t i = root.first_child; i; i = cast_ast_node(ast, i).next_sibling) {
        CastNode decl = cast_ast_node(ast, i);
        printf("line %u: %-4s ", (unsigned)decl.line, decl.msg);
        describe(ast, decl);
        if (strcmp(decl.msg, "func") == 0) {
            const char *separator = "";
            printf("(");
            for (size_t p = decl.first_child; p; p = cast_ast_node(ast, p).next_sibling) {
                CastNode param = cast_ast_node(ast, p);
                if (strcmp(param.msg, "param") != 0) continue;
                printf("%s", separator);
                describe(ast, param);
                separator = ", ";
            }
            printf(")");
        }
        printf("\n");
    }
    return 0;
}
```

It prints:

```text
line 2: func static const char *lookup(int id, char **out)
line 3: decl int counts[]
line 3: decl int total
line 4: func void reset(void)
```

Each node has a `msg` naming its kind: `"decl"` for variables, `"func"` for functions (prototype
or definition), `"param"` for parameters, `"type"` for the specifier text (its `name` keeps
storage classes and qualifiers, such as `static const char`), `"ptr"` for each `*`, and `"array"`
for each `[...]`. `name` is the declared identifier and `line` its source line; `arg` and
`arglen` locate the identifier in the source. Function bodies, initializers and `struct` member
lists are skipped, so they produce no nodes.

Names are copied into the arena, but `arg` offsets still refer to the source. `cast_ast()` reads
the source recorded by the most recent `cast_tokenize()` on the same memory, so tokenize and parse
one input before starting the next.

## Handling Unparsable Input

Cast does not stop at the first problem. When a file-scope item does not start with a type, Cast
adds an `"error"` node whose `name` is the offending token, skips to the next `;`, and carries on:

```c
char source[] = "int ok;\nreturn 0;\nfloat also_ok;\n";
/* tu children: decl ok, error "return" (line 2), decl also_ok */
```

Report error nodes with their `line` instead of treating them as declarations. Because
preprocessor lines are skipped, a declaration produced by a macro is invisible to Cast, and an
identifier type such as `size_t` is recognized only from context (an identifier followed by
another identifier or `*`).

## Sizing and Reusing Memory

Tokens take `sizeof(CastToken)` bytes each, and `cast_ast()` reserves room for four nodes per
token plus the declared names. A buffer of a few hundred bytes per token is ample. When either
call returns NULL for non-empty input, give it a larger buffer.

`cast_memory_clear()` empties the arena without freeing anything, so the same buffer can parse the
next input. Everything returned before the clear becomes invalid. Cool's template transpiler,
`CoolTranspiler.c`, uses Cast this way to read each template's parameter list.

See [Cast symbols](/symbols/cast/) for the generated declarations.
