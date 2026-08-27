/* ===========================================================================
 * CAST - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * Cast as in C-AST.
 *
 * # Features / Anti-Features
 * - Lex and/or parse C code into a walkable tree.
 * - Does not load files.
 * - Does not allocate memory.
 * - Skips the preprocessor entirely.
 *
 * PREFIX: CAST (macros)  Cast (types)  cast_ (functions)
 *
 * USAGE:*/
#if 0
#include "cast.h"
#include "cast.c"

unsigned char buf[1 << 20];
char str[] = "void my_func(char *arg1, int arg2);";

CastMemory *mem = cast_memory_create(buf, sizeof buf);

size_t n;
CastToken *tokens = cast_tokenize(mem, str, strlen(str), &n);

for (size_t u = 0; u < n; ++u) {
    printf("%s(%d): %.*s\n", cast_token_type(tokens[u].type), tokens[u].arg, tokens[u].arglen);
}

size_t nodes = 0;
CastAst *ast = cast_ast(mem, tokens, n, &nodes);

for (i = 0; i < n; i++) {
    CastNode *d = cast_ast_node(ast, i);
    fprintf(stderr, "%s:%u: %s\n", d->name ? d->name : "-", d->line, d->msg);
}

cast_memory_clear(&mem); // does not free, only if you want to reuse the memory

#endif
/* =========================================================================== */

#ifndef CAST_H
#define CAST_H

#define CAST_MAJOR 0
#define CAST_MINOR 0
#define CAST_PATCH 0

/* CHANGE LOG
 * 0.0.0 - @vasco - C parser scaffold: user buf, parse, walk, diags
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(CAST_DEBUG)
#define CASSERT_N(_1, _2, N, ...) N
#define CASSERT(...) CASSERT_N(__VA_ARGS__, CASSERT2, CASSERT1)(__VA_ARGS__)
#define CASSERT1(a) assert(a)
#define CASSERT2(a, s) assert((a) && (s))
#else
#define CASSERT(...) ((void)0)
#endif

#define CAST_TODO \
	do { \
		fprintf(stderr, "CAST TODO: %s() in %s:%d\n", __func__, __FILE__, __LINE__); \
		abort(); \
	} while (0)

#define CAST_MIN(a, b) ((a) < (b) ? (a) : (b))
#define CAST_MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct CastMemory {
	unsigned char *buf;
	size_t cap;
	size_t len;
	char *src;
	size_t src_len;
} CastMemory; // User provided memory.

typedef struct CastToken {
	uint32_t type;
	int arg;
	int arglen;
} CastToken; // A single token.

typedef struct CastAst CastAst; // A parsed AST tree.

typedef struct CastNode {
	const char *name;
	uint32_t line;
	const char *msg;
	uint32_t kind;
	int arg;
	int arglen;
	size_t first_child;
	size_t next_sibling;
} CastNode; // An AST tree node.

extern CastMemory cast_memory_create(void *buf, size_t size); // Creates a memory arena. Use as thought will.
extern CastToken *cast_tokenize(CastMemory *mem, char *str, size_t len, size_t *out_len); // Tokenizes the output and returns an array plus the number of tokens.
extern char* cast_token_type(uint32_t type); // Returns cstring from a type.
extern CastAst *cast_ast(CastMemory *mem, CastToken *tokens, size_t n_tokens, size_t *out_nodes);
extern CastNode cast_ast_node(CastAst *ast, size_t i); // Gets a node from an index.
extern void cast_memory_clear(CastMemory *mem); // Does not free. Clears the Arena so you can reuse the memory.

#endif /* CAST_H */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
