/* ===========================================================================
 * CAST - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * Cast as in C-AST.
 *
 * # Features / Anti-Features
 * - Lex and/or parse C code into a walkable tree.
 * - Does not load files.
 * - Does not allocate memory.
 * - Does not run the preprocessor; comments and directives can be kept as
 *   tokens.
 * - The tree is one flat array of nodes that refer to each other and to the
 *   tokens by index.
 * - Statements are parsed with CAST_PARSE_BODIES; expressions are token spans.
 *
 * PREFIX: CAST (macros)  Cast (types)  cast_ (functions)
 *
 * USAGE:
 *     #define CAST_IMPLEMENTATION in exactly one translation unit
 *     #include "Cast.h"
 *
 * =========================================================================== */
#if 0
#include "Cast.h"

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

// Keep comments and directives as tokens and parse function bodies too.
tokens = cast_tokenize_opt(mem, str, strlen(str), CAST_LEX_COMMENTS | CAST_LEX_PP, &n);
ast = cast_ast_opt(mem, tokens, n, CAST_PARSE_BODIES, &nodes);

cast_memory_clear(&mem); // does not free, only if you want to reuse the memory

#endif
/* =========================================================================== */

#ifndef CAST_H
#define CAST_H

#define CAST_MAJOR 0
#define CAST_MINOR 1
#define CAST_PATCH 0

/* CHANGE LOG
 * 0.1.0 - comment and directive tokens, token line/col, statement parsing
 *         (CAST_PARSE_BODIES), node token spans, public token and node kinds.
 *         Calling-convention macros in declarators; file-scope errors end
 *         after a { } body.
 * 0.0.1 - a name before a type keyword ("API int f(void);") is a specifier
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

enum {
	CAST_TOK_EOF = 0,
	CAST_TOK_IDENT,
	CAST_TOK_NUMBER,
	CAST_TOK_STRING,
	CAST_TOK_CHAR,
	CAST_TOK_PUNCT,
	CAST_TOK_KW,
	CAST_TOK_ERROR,
	CAST_TOK_COMMENT, // Block or line comment, with CAST_LEX_COMMENTS.
	CAST_TOK_PP       // A whole directive and its continuations, with CAST_LEX_PP.
};

enum {
	CAST_LEX_COMMENTS = 1 << 0, // Keep comments as tokens.
	CAST_LEX_PP = 1 << 1,       // Keep preprocessor directives as tokens.
	CAST_PARSE_BODIES = 1 << 2  // Parse function, aggregate and initializer bodies.
};

enum {
	CAST_NODE_NONE = 0,
	CAST_NODE_TU,
	CAST_NODE_DECL,
	CAST_NODE_FUNC,
	CAST_NODE_PARAM,
	CAST_NODE_TYPE,
	CAST_NODE_PTR,
	CAST_NODE_ARRAY,
	CAST_NODE_ERROR,
	/* With CAST_PARSE_BODIES. */
	CAST_NODE_FIELDS,   // { } of a struct, union or enum: DECL children.
	CAST_NODE_INIT,     // { } initializer: INIT and EXPR children.
	CAST_NODE_BLOCK,    // { } compound statement. A FUNC with a BLOCK child is a definition.
	CAST_NODE_IF,       // EXPR condition, statement, optional ELSE.
	CAST_NODE_ELSE,     // Statement.
	CAST_NODE_SWITCH,   // EXPR, statement.
	CAST_NODE_CASE,     // case X: or default:, with the EXPR of a case.
	CAST_NODE_LABEL,
	CAST_NODE_WHILE,    // EXPR, statement.
	CAST_NODE_DO,       // Statement, EXPR.
	CAST_NODE_FOR,      // DECL or EXPR init, EXPR condition, EXPR step, statement.
	CAST_NODE_RETURN,   // Optional EXPR.
	CAST_NODE_BREAK,
	CAST_NODE_CONTINUE,
	CAST_NODE_GOTO,
	CAST_NODE_EXPR,     // Token span; expressions are not parsed into trees.
	CAST_NODE_EMPTY     // ; or an empty part of a for.
};

typedef struct CastMemory {
	unsigned char *buf;
	size_t cap;
	size_t len;
	char *src;
	size_t src_len;
} CastMemory; // User provided memory.

typedef struct CastToken {
	uint32_t type;
	int arg;       // Offset in the source.
	int arglen;
	uint32_t line; // 1-based.
	uint32_t col;  // 0-based byte column.
} CastToken; // A single token.

typedef struct CastAst CastAst; // A parsed AST tree.

typedef struct CastNode {
	const char *name;
	uint32_t line;
	const char *msg;
	uint32_t kind;
	int arg;
	int arglen;
	size_t first_child;  // Node index, 0 for none.
	size_t next_sibling; // Node index, 0 for none.
	size_t tok;          // Index of the first token.
	size_t ntok;         // Tokens spanned, comments and directives included.
} CastNode; // An AST tree node.

extern CastMemory cast_memory_create(void *buf, size_t size); // Creates a memory arena. Use as thought will.
extern CastToken *cast_tokenize(CastMemory *mem, char *str, size_t len, size_t *out_len); // Tokenizes the output and returns an array plus the number of tokens.
extern CastToken *cast_tokenize_opt(CastMemory *mem, char *str, size_t len, uint32_t flags, size_t *out_len); // cast_tokenize with CAST_LEX_ flags.
extern char* cast_token_type(uint32_t type); // Returns cstring from a type.
extern CastAst *cast_ast(CastMemory *mem, CastToken *tokens, size_t n_tokens, size_t *out_nodes);
extern CastAst *cast_ast_opt(CastMemory *mem, CastToken *tokens, size_t n_tokens, uint32_t flags, size_t *out_nodes); // cast_ast with CAST_PARSE_ flags.
extern CastNode cast_ast_node(CastAst *ast, size_t i); // Gets a node from an index.
extern void cast_memory_clear(CastMemory *mem); // Does not free. Clears the Arena so you can reuse the memory.

#endif /* CAST_H */

/*
------------------------------------------------------------------------------
MIT License
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
*/

#ifdef CAST_IMPLEMENTATION
#ifndef CAST_IMPLEMENTATION_ONCE
#define CAST_IMPLEMENTATION_ONCE


#ifndef CAST_ALIGN
#define CAST_ALIGN (sizeof (void *))
#endif

struct CastAst {
	CastNode *nodes;
	size_t n;
	size_t cap;
	char *src;
	size_t src_len;
};

typedef struct {
	CastMemory *mem;
	CastAst *ast;
	CastToken *toks;
	size_t ntoks;
	size_t *idx;   /* tokens the parser reads: no comments or directives */
	size_t nidx;
	size_t i;      /* position in idx */
	uint32_t flags;
	int depth;     /* block nesting */
	int oom;
} CastParser;

static const char *cast_internal_kws[] = {
	"_Alignas",
	"_Alignof",
	"_Atomic",
	"_Bool",
	"_Complex",
	"_Generic",
	"_Imaginary",
	"_Noreturn",
	"_Static_assert",
	"_Thread_local",
	"auto",
	"break",
	"case",
	"char",
	"const",
	"continue",
	"default",
	"do",
	"double",
	"else",
	"enum",
	"extern",
	"float",
	"for",
	"goto",
	"if",
	"inline",
	"int",
	"long",
	"register",
	"restrict",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"struct",
	"switch",
	"typedef",
	"union",
	"unsigned",
	"void",
	"volatile",
	"while"
};

static size_t cast_internal_align_up(size_t n, size_t align);
static void *cast_internal_alloc(CastMemory *m, size_t n);
static char *cast_internal_intern(CastMemory *m, const char *s, size_t n);
static int cast_internal_is_ident_start(int c);
static int cast_internal_is_ident_cont(int c);
static int cast_internal_is_digit(int c);
static int cast_internal_is_kw_word(const char *s, size_t n);
static int cast_internal_is_spec_word(const char *s, size_t n);
static int cast_internal_is_qual_word(const char *s, size_t n);
static size_t cast_internal_skip_ws(const char *s, size_t n, size_t i, int *bol, uint32_t flags);
static size_t cast_internal_punct_len(const char *s, size_t n, size_t i);
static size_t cast_internal_skip_quoted(const char *s, size_t n, size_t i, int *bad);
static int cast_internal_lex_one(const char *s, size_t n, size_t *i, int *bol, uint32_t flags, CastToken *out);
static int cast_internal_lex(const char *s, size_t n, uint32_t flags, CastToken *out, size_t cap, size_t *n_out);

static size_t cast_internal_node_new(CastParser *p, uint32_t kind, CastToken *t, const char *msg);
static void cast_internal_node_end(CastParser *p, size_t node);
static void cast_internal_node_add(CastAst *ast, size_t parent, size_t child);
static void cast_internal_node_name(CastParser *p, size_t node, CastToken *t);
static CastToken *cast_internal_peek(CastParser *p);
static CastToken *cast_internal_peek_n(CastParser *p, size_t n);
static CastToken *cast_internal_eat(CastParser *p);
static int cast_internal_tok_text(CastParser *p, CastToken *t, const char *s);
static int cast_internal_is_punct(CastParser *p, const char *s);
static int cast_internal_is_kw(CastParser *p, const char *kw);
static int cast_internal_eat_punct(CastParser *p, const char *s);
static int cast_internal_at_specifier(CastParser *p);
static int cast_internal_eat_attribute(CastParser *p);
static void cast_internal_skip_balanced(CastParser *p, const char *open, const char *close);
static void cast_internal_scan(CastParser *p, const char *stops);
static size_t cast_internal_parse_expr(CastParser *p, size_t parent, const char *stops);
static void cast_internal_parse_paren(CastParser *p, size_t parent);
static void cast_internal_parse_initializer(CastParser *p, size_t parent);
static void cast_internal_parse_fields(CastParser *p, size_t type, int is_enum);
static size_t cast_internal_parse_specifiers(CastParser *p);
static void cast_internal_parse_params(CastParser *p, size_t parent);
static void cast_internal_parse_declarator(CastParser *p, size_t parent, char **name, uint32_t *line, int *is_fn);
static int cast_internal_parse_decl(CastParser *p, size_t parent);
static void cast_internal_parse_block(CastParser *p, size_t parent);
static void cast_internal_parse_item(CastParser *p, size_t parent);
static void cast_internal_parse_stmt(CastParser *p, size_t parent);
static void cast_internal_parse_external(CastParser *p, size_t tu);
static void cast_internal_parse_tu(CastParser *p);

size_t
cast_internal_align_up(size_t n, size_t align)
{
	return (n + (align - 1)) & ~(align - 1);
}

void *
cast_internal_alloc(CastMemory *m, size_t n)
{
	size_t off;
	void *p;

	if (!m || !m->buf)
		return NULL;
	if (n == 0)
		n = 1;
	off = cast_internal_align_up(m->len, CAST_ALIGN);
	if (off + n > m->cap)
		return NULL;
	p = m->buf + off;
	m->len = off + n;
	memset(p, 0, n);
	return p;
}

char *
cast_internal_intern(CastMemory *m, const char *s, size_t n)
{
	char *p;

	p = cast_internal_alloc(m, n + 1);
	if (!p)
		return NULL;
	if (n && s)
		memcpy(p, s, n);
	p[n] = 0;
	return p;
}

int
cast_internal_is_ident_start(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

int
cast_internal_is_ident_cont(int c)
{
	return cast_internal_is_ident_start(c) || (c >= '0' && c <= '9');
}

int
cast_internal_is_digit(int c)
{
	return c >= '0' && c <= '9';
}

int
cast_internal_is_kw_word(const char *s, size_t n)
{
	size_t i, nk;

	nk = sizeof cast_internal_kws / sizeof *cast_internal_kws;
	for (i = 0; i < nk; i++) {
		const char *k;
		size_t kn;

		k = cast_internal_kws[i];
		kn = strlen(k);
		if (kn == n && memcmp(s, k, n) == 0)
			return 1;
	}
	return 0;
}

int
cast_internal_is_spec_word(const char *s, size_t n)
{
	static const char *w[] = {
		"_Alignas", "_Atomic", "_Bool", "_Complex", "_Imaginary",
		"_Noreturn", "_Static_assert", "_Thread_local",
		"auto", "char", "const", "double", "enum", "extern",
		"float", "inline", "int", "long", "register", "restrict",
		"short", "signed", "static", "struct", "typedef", "union",
		"unsigned", "void", "volatile"
	};
	size_t i;

	for (i = 0; i < sizeof w / sizeof *w; i++) {
		size_t kn;

		kn = strlen(w[i]);
		if (kn == n && memcmp(s, w[i], n) == 0)
			return 1;
	}
	return 0;
}

int
cast_internal_is_qual_word(const char *s, size_t n)
{
	return (n == 5 && memcmp(s, "const", 5) == 0)
	    || (n == 8 && memcmp(s, "volatile", 8) == 0)
	    || (n == 8 && memcmp(s, "restrict", 8) == 0)
	    || (n == 7 && memcmp(s, "_Atomic", 7) == 0);
}

size_t
cast_internal_skip_ws(const char *s, size_t n, size_t i, int *bol, uint32_t flags)
{
	for (;;) {
		if (i >= n)
			return i;
		if (s[i] == ' ' || s[i] == '\t' || s[i] == '\v' || s[i] == '\f' || s[i] == '\r') {
			i++;
			continue;
		}
		if (s[i] == '\n') {
			*bol = 1;
			i++;
			continue;
		}
		if ((flags & CAST_LEX_COMMENTS) && s[i] == '/' && i + 1 < n
		    && (s[i + 1] == '/' || s[i + 1] == '*'))
			return i;
		if (s[i] == '/' && i + 1 < n && s[i + 1] == '/') {
			i += 2;
			while (i < n && s[i] != '\n')
				i++;
			continue;
		}
		if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
			i += 2;
			while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) {
				if (s[i] == '\n')
					*bol = 1;
				i++;
			}
			if (i + 1 < n)
				i += 2;
			else
				i = n;
			continue;
		}
		if (*bol && s[i] == '#') {
			if (flags & CAST_LEX_PP)
				return i;
			i++;
			while (i < n && s[i] != '\n') {
				if (s[i] == '\\' && i + 1 < n && s[i + 1] == '\n')
					i += 2;
				else
					i++;
			}
			continue;
		}
		return i;
	}
}

size_t
cast_internal_punct_len(const char *s, size_t n, size_t i)
{
	char a, b, c;

	if (i >= n)
		return 0;
	a = s[i];
	b = (i + 1 < n) ? s[i + 1] : 0;
	c = (i + 2 < n) ? s[i + 2] : 0;
	if (a == '>' && b == '>' && c == '=')
		return 3;
	if (a == '<' && b == '<' && c == '=')
		return 3;
	if (a == '.' && b == '.' && c == '.')
		return 3;
	if ((a == '-' && b == '>')
	    || (a == '+' && b == '+')
	    || (a == '-' && b == '-')
	    || (a == '<' && b == '<')
	    || (a == '>' && b == '>')
	    || (a == '<' && b == '=')
	    || (a == '>' && b == '=')
	    || (a == '=' && b == '=')
	    || (a == '!' && b == '=')
	    || (a == '&' && b == '&')
	    || (a == '|' && b == '|')
	    || (a == '+' && b == '=')
	    || (a == '-' && b == '=')
	    || (a == '*' && b == '=')
	    || (a == '/' && b == '=')
	    || (a == '%' && b == '=')
	    || (a == '&' && b == '=')
	    || (a == '^' && b == '=')
	    || (a == '|' && b == '=')
	    || (a == '#' && b == '#'))
		return 2;
	switch (a) {
	case '[': /* FALLTHROUGH */
	case ']': /* FALLTHROUGH */
	case '(': /* FALLTHROUGH */
	case ')': /* FALLTHROUGH */
	case '{': /* FALLTHROUGH */
	case '}': /* FALLTHROUGH */
	case '.': /* FALLTHROUGH */
	case '&': /* FALLTHROUGH */
	case '*': /* FALLTHROUGH */
	case '+': /* FALLTHROUGH */
	case '-': /* FALLTHROUGH */
	case '~': /* FALLTHROUGH */
	case '!': /* FALLTHROUGH */
	case '/': /* FALLTHROUGH */
	case '%': /* FALLTHROUGH */
	case '<': /* FALLTHROUGH */
	case '>': /* FALLTHROUGH */
	case '^': /* FALLTHROUGH */
	case '|': /* FALLTHROUGH */
	case '?': /* FALLTHROUGH */
	case ':': /* FALLTHROUGH */
	case ';': /* FALLTHROUGH */
	case '=': /* FALLTHROUGH */
	case ',': /* FALLTHROUGH */
	case '#':
		return 1;
	default:
		return 0;
	}
}

/* End of the string or character literal whose quote is at i. */
size_t
cast_internal_skip_quoted(const char *s, size_t n, size_t i, int *bad)
{
	char q;

	q = s[i++];
	while (i < n && s[i] != q) {
		if (s[i] == '\\' && i + 1 < n)
			i += 2;
		else
			i++;
	}
	if (i < n)
		return i + 1;
	*bad = 1;
	return n;
}

int
cast_internal_lex_one(const char *s, size_t n, size_t *i, int *bol, uint32_t flags, CastToken *out)
{
	size_t start, plen, pre;
	int bad;
	CastToken tok;

	*i = cast_internal_skip_ws(s, n, *i, bol, flags);
	if (*i >= n)
		return 0;
	start = *i;
	memset(&tok, 0, sizeof tok);
	tok.arg = (int)start;
	bad = 0;

	pre = 0;
	if (*i + 2 < n && s[*i] == 'u' && s[*i + 1] == '8'
	    && (s[*i + 2] == '"' || s[*i + 2] == '\''))
		pre = 2;
	else if (*i + 1 < n && (s[*i] == 'L' || s[*i] == 'U' || s[*i] == 'u')
	    && (s[*i + 1] == '"' || s[*i + 1] == '\''))
		pre = 1;

	if (s[*i] == '/' && *i + 1 < n && s[*i + 1] == '/') {
		/* Only reached with CAST_LEX_COMMENTS. */
		tok.type = CAST_TOK_COMMENT;
		while (*i < n && s[*i] != '\n')
			*i += 1;
	} else if (s[*i] == '/' && *i + 1 < n && s[*i + 1] == '*') {
		tok.type = CAST_TOK_COMMENT;
		*i += 2;
		while (*i + 1 < n && !(s[*i] == '*' && s[*i + 1] == '/')) {
			if (s[*i] == '\n')
				*bol = 1;
			*i += 1;
		}
		*i = *i + 1 < n ? *i + 2 : n;
	} else if (*bol && s[*i] == '#') {
		/* Only reached with CAST_LEX_PP. */
		tok.type = CAST_TOK_PP;
		while (*i < n && s[*i] != '\n') {
			if (s[*i] == '\\' && *i + 1 < n && s[*i + 1] == '\n') {
				*i += 2;
			} else if (s[*i] == '/' && *i + 1 < n && s[*i + 1] == '*') {
				for (*i += 2; *i + 1 < n && !(s[*i] == '*' && s[*i + 1] == '/'); *i += 1)
					;
				*i = *i + 1 < n ? *i + 2 : n;
			} else {
				*i += 1;
			}
		}
		*bol = 0;
	} else if (pre || s[*i] == '"' || s[*i] == '\'') {
		tok.type = s[*i + pre] == '"' ? CAST_TOK_STRING : CAST_TOK_CHAR;
		*i = cast_internal_skip_quoted(s, n, *i + pre, &bad);
		if (bad)
			tok.type = CAST_TOK_ERROR;
		*bol = 0;
	} else if (cast_internal_is_digit((unsigned char)s[*i])
	    || (s[*i] == '.' && *i + 1 < n && cast_internal_is_digit((unsigned char)s[*i + 1]))) {
		tok.type = CAST_TOK_NUMBER;
		while (*i < n) {
			int ch;

			ch = (unsigned char)s[*i];
			if ((ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P')
			    && *i + 1 < n && (s[*i + 1] == '+' || s[*i + 1] == '-')) {
				*i += 2;
				continue;
			}
			if (cast_internal_is_ident_cont(ch) || ch == '.') {
				*i += 1;
				continue;
			}
			break;
		}
		*bol = 0;
	} else if (cast_internal_is_ident_start((unsigned char)s[*i])) {
		*i += 1;
		while (*i < n && cast_internal_is_ident_cont((unsigned char)s[*i]))
			*i += 1;
		tok.type = cast_internal_is_kw_word(s + start, *i - start)
		    ? CAST_TOK_KW : CAST_TOK_IDENT;
		*bol = 0;
	} else if ((plen = cast_internal_punct_len(s, n, *i))) {
		tok.type = CAST_TOK_PUNCT;
		*i += plen;
		*bol = 0;
	} else {
		tok.type = CAST_TOK_ERROR;
		*i += 1;
		*bol = 0;
	}
	tok.arglen = (int)(*i - start);
	if (out)
		*out = tok;
	return 1;
}

int
cast_internal_lex(const char *s, size_t n, uint32_t flags, CastToken *out, size_t cap, size_t *n_out)
{
	CastToken tok;
	size_t i, count, at, linestart;
	uint32_t line;
	int bol;

	i = 0;
	count = 0;
	at = 0;
	linestart = 0;
	line = 1;
	bol = 1;
	while (cast_internal_lex_one(s, n, &i, &bol, flags, &tok)) {
		if (out) {
			if (count >= cap)
				return -1;
			for (; at < (size_t)tok.arg; at++) {
				if (s[at] == '\n') {
					line++;
					linestart = at + 1;
				}
			}
			tok.line = line;
			tok.col = (uint32_t)((size_t)tok.arg - linestart);
			out[count] = tok;
		}
		count++;
	}
	*n_out = count;
	return 0;
}

size_t
cast_internal_node_new(CastParser *p, uint32_t kind, CastToken *t, const char *msg)
{
	CastNode *n;
	size_t i;

	if (!p || p->oom || !p->ast || p->ast->n >= p->ast->cap) {
		if (p)
			p->oom = 1;
		return (size_t)-1;
	}
	i = p->ast->n++;
	n = &p->ast->nodes[i];
	n->kind = kind;
	n->msg = msg;
	n->line = 1;
	if (t) {
		n->arg = t->arg;
		n->arglen = t->arglen;
		n->line = t->line;
		n->tok = (size_t)(t - p->toks);
		n->ntok = 1;
	}
	return i;
}

/* Extend the node's token span to the last token eaten. */
void
cast_internal_node_end(CastParser *p, size_t node)
{
	CastNode *n;
	size_t last;

	if (node == (size_t)-1 || !p->i)
		return;
	n = &p->ast->nodes[node];
	last = p->idx[p->i - 1];
	if (last >= n->tok)
		n->ntok = last - n->tok + 1;
}

void
cast_internal_node_add(CastAst *ast, size_t parent, size_t child)
{
	CastNode *pn, *it;
	size_t s;

	if (!ast || parent == (size_t)-1 || child == (size_t)-1)
		return;
	if (parent >= ast->n || child >= ast->n)
		return;
	pn = &ast->nodes[parent];
	if (!pn->first_child) {
		pn->first_child = child;
		return;
	}
	s = pn->first_child;
	for (;;) {
		it = &ast->nodes[s];
		if (!it->next_sibling) {
			it->next_sibling = child;
			return;
		}
		s = it->next_sibling;
	}
}

void
cast_internal_node_name(CastParser *p, size_t node, CastToken *t)
{
	if (node == (size_t)-1 || !t)
		return;
	p->ast->nodes[node].name = cast_internal_intern(p->mem, p->ast->src + t->arg, (size_t)t->arglen);
	if (!p->ast->nodes[node].name)
		p->oom = 1;
}

CastToken *
cast_internal_peek(CastParser *p)
{
	if (!p || p->i >= p->nidx)
		return NULL;
	return &p->toks[p->idx[p->i]];
}

CastToken *
cast_internal_peek_n(CastParser *p, size_t n)
{
	if (!p || p->i + n >= p->nidx)
		return NULL;
	return &p->toks[p->idx[p->i + n]];
}

CastToken *
cast_internal_eat(CastParser *p)
{
	CastToken *t;

	t = cast_internal_peek(p);
	if (t)
		p->i++;
	return t;
}

int
cast_internal_tok_text(CastParser *p, CastToken *t, const char *s)
{
	size_t n;

	if (!p || !p->ast || !p->ast->src || !t || !s)
		return 0;
	n = strlen(s);
	return (size_t)t->arglen == n && memcmp(p->ast->src + t->arg, s, n) == 0;
}

int
cast_internal_is_punct(CastParser *p, const char *s)
{
	CastToken *t;

	t = cast_internal_peek(p);
	return t && t->type == CAST_TOK_PUNCT && cast_internal_tok_text(p, t, s);
}

int
cast_internal_is_kw(CastParser *p, const char *kw)
{
	CastToken *t;

	t = cast_internal_peek(p);
	return t && t->type == CAST_TOK_KW && cast_internal_tok_text(p, t, kw);
}

int
cast_internal_eat_punct(CastParser *p, const char *s)
{
	if (!cast_internal_is_punct(p, s))
		return 0;
	cast_internal_eat(p);
	return 1;
}

int
cast_internal_at_specifier(CastParser *p)
{
	CastToken *t, *n;
	const char *cc[] = {"(", NULL, "*", NULL, ")", "("};
	size_t i;

	t = cast_internal_peek(p);
	if (!t)
		return 0;
	if (t->type == CAST_TOK_KW && p->ast->src
	    && cast_internal_is_spec_word(p->ast->src + t->arg, (size_t)t->arglen))
		return 1;
	if (t->type == CAST_TOK_IDENT && (cast_internal_tok_text(p, t, "__attribute__")
	    || cast_internal_tok_text(p, t, "__declspec")))
		return 1;
	if (t->type != CAST_TOK_IDENT || cast_internal_tok_text(p, t, "asm")
	    || cast_internal_tok_text(p, t, "__asm") || cast_internal_tok_text(p, t, "__asm__"))
		return 0;
	n = cast_internal_peek_n(p, 1);
	if (!n)
		return 0;
	if (n->type == CAST_TOK_IDENT)
		return 1;
	if (n->type == CAST_TOK_PUNCT && cast_internal_tok_text(p, n, "*"))
		return 1;
	/* A name before a type keyword is a macro such as "API int f(void);". */
	if (n->type == CAST_TOK_KW && p->ast->src
	    && cast_internal_is_spec_word(p->ast->src + n->arg, (size_t)n->arglen))
		return 1;
	/* "UINT (WINAPI *fp)(void);" at file scope, where nothing is called. */
	for (i = 0; !p->depth && i < sizeof cc / sizeof *cc; i++) {
		if (!(n = cast_internal_peek_n(p, i + 1)))
			return 0;
		if (cc[i] ? !cast_internal_tok_text(p, n, cc[i]) : n->type != CAST_TOK_IDENT)
			return 0;
	}
	return !p->depth;
}

/* Skip __attribute__((...)) or __declspec(...); 0 if neither is next. */
int
cast_internal_eat_attribute(CastParser *p)
{
	CastToken *t;

	t = cast_internal_peek(p);
	if (!t || t->type != CAST_TOK_IDENT || (!cast_internal_tok_text(p, t, "__attribute__")
	    && !cast_internal_tok_text(p, t, "__declspec")))
		return 0;
	cast_internal_eat(p);
	cast_internal_skip_balanced(p, "(", ")");
	return 1;
}

void
cast_internal_skip_balanced(CastParser *p, const char *open, const char *close)
{
	int depth;

	if (!cast_internal_is_punct(p, open))
		return;
	cast_internal_eat(p);
	depth = 1;
	while (depth && cast_internal_peek(p)) {
		if (cast_internal_is_punct(p, open))
			depth++;
		else if (cast_internal_is_punct(p, close))
			depth--;
		cast_internal_eat(p);
	}
}

/* Eat tokens up to a depth-0 punctuator in stops or an unmatched closer. */
void
cast_internal_scan(CastParser *p, const char *stops)
{
	CastToken *t;
	int depth, q;
	char c;

	depth = 0;
	q = 0;
	while ((t = cast_internal_peek(p))) {
		if (t->type == CAST_TOK_PUNCT && t->arglen == 1) {
			c = p->ast->src[t->arg];
			if (depth == 0 && strchr(stops, c)) {
				if (c != ':' || !q)
					return;
				q--;
			} else if (c == '?') {
				q++;
			} else if (c == '(' || c == '[' || c == '{') {
				depth++;
			} else if (c == ')' || c == ']' || c == '}') {
				if (!depth)
					return;
				depth--;
			}
		}
		cast_internal_eat(p);
	}
}

/* An EXPR spanning the scanned tokens, EMPTY if there are none. */
size_t
cast_internal_parse_expr(CastParser *p, size_t parent, const char *stops)
{
	size_t e, before;

	before = p->i;
	e = (size_t)-1;
	if (p->flags & CAST_PARSE_BODIES)
		e = cast_internal_node_new(p, CAST_NODE_EXPR, cast_internal_peek(p), "expr");
	cast_internal_scan(p, stops);
	if (e != (size_t)-1 && p->i == before) {
		p->ast->nodes[e].kind = CAST_NODE_EMPTY;
		p->ast->nodes[e].msg = "empty";
		p->ast->nodes[e].ntok = 0;
	}
	cast_internal_node_end(p, e);
	cast_internal_node_add(p->ast, parent, e);
	return e;
}

void
cast_internal_parse_paren(CastParser *p, size_t parent)
{
	size_t e;

	e = cast_internal_node_new(p, CAST_NODE_EXPR, cast_internal_peek(p), "expr");
	cast_internal_skip_balanced(p, "(", ")");
	cast_internal_node_end(p, e);
	cast_internal_node_add(p->ast, parent, e);
}

void
cast_internal_parse_initializer(CastParser *p, size_t parent)
{
	size_t init, before;

	if (!(p->flags & CAST_PARSE_BODIES) || !cast_internal_is_punct(p, "{")) {
		cast_internal_parse_expr(p, parent, ",;");
		return;
	}
	init = cast_internal_node_new(p, CAST_NODE_INIT, cast_internal_eat(p), "init");
	cast_internal_node_add(p->ast, parent, init);
	while (cast_internal_peek(p) && !cast_internal_is_punct(p, "}")) {
		before = p->i;
		if (cast_internal_is_punct(p, "{"))
			cast_internal_parse_initializer(p, init);
		else
			cast_internal_parse_expr(p, init, ",");
		cast_internal_eat_punct(p, ",");
		if (p->i == before)
			cast_internal_eat(p);
	}
	cast_internal_eat_punct(p, "}");
	cast_internal_node_end(p, init);
}

void
cast_internal_parse_fields(CastParser *p, size_t type, int is_enum)
{
	size_t fields, d, before;
	CastToken *t;

	fields = cast_internal_node_new(p, CAST_NODE_FIELDS, cast_internal_eat(p), "fields");
	cast_internal_node_add(p->ast, type, fields);
	while ((t = cast_internal_peek(p)) && !cast_internal_is_punct(p, "}")) {
		before = p->i;
		if (is_enum) {
			d = cast_internal_node_new(p, CAST_NODE_DECL, t, "decl");
			if (t->type == CAST_TOK_IDENT)
				cast_internal_node_name(p, d, cast_internal_eat(p));
			if (cast_internal_eat_punct(p, "="))
				cast_internal_parse_expr(p, d, ",");
			cast_internal_node_end(p, d);
			cast_internal_node_add(p->ast, fields, d);
			cast_internal_eat_punct(p, ",");
		} else if (!cast_internal_parse_decl(p, fields)) {
			d = cast_internal_node_new(p, CAST_NODE_ERROR, t, "error");
			cast_internal_scan(p, ";");
			cast_internal_eat_punct(p, ";");
			cast_internal_node_end(p, d);
			cast_internal_node_add(p->ast, fields, d);
		}
		if (p->i == before)
			cast_internal_eat(p);
	}
	cast_internal_eat_punct(p, "}");
	cast_internal_node_end(p, fields);
}

size_t
cast_internal_parse_specifiers(CastParser *p)
{
	CastToken *t, *prev;
	size_t type, begin, end;
	int is_enum;

	t = cast_internal_peek(p);
	if (!t || !cast_internal_at_specifier(p))
		return (size_t)-1;
	type = cast_internal_node_new(p, CAST_NODE_TYPE, t, "type");
	begin = (size_t)t->arg;
	end = begin;
	while (cast_internal_at_specifier(p)) {
		if (cast_internal_is_kw(p, "struct")
		    || cast_internal_is_kw(p, "union")
		    || cast_internal_is_kw(p, "enum")) {
			is_enum = cast_internal_is_kw(p, "enum");
			t = cast_internal_eat(p);
			if (cast_internal_peek(p) && cast_internal_peek(p)->type == CAST_TOK_IDENT)
				t = cast_internal_eat(p);
			if (cast_internal_is_punct(p, "{")) {
				if (p->flags & CAST_PARSE_BODIES)
					cast_internal_parse_fields(p, type, is_enum);
				else
					cast_internal_skip_balanced(p, "{", "}");
				t = NULL;
			}
		} else if (cast_internal_eat_attribute(p)) {
			t = NULL;
		} else {
			t = cast_internal_eat(p);
		}
		if (!t) {
			prev = &p->toks[p->idx[p->i - 1]];
			end = (size_t)prev->arg + (size_t)prev->arglen;
		} else {
			end = (size_t)t->arg + (size_t)t->arglen;
		}
	}
	if (type == (size_t)-1)
		return type;
	p->ast->nodes[type].name = cast_internal_intern(p->mem, p->ast->src + begin, end - begin);
	if (!p->ast->nodes[type].name)
		p->oom = 1;
	cast_internal_node_end(p, type);
	return type;
}

void
cast_internal_parse_params(CastParser *p, size_t parent)
{
	while (cast_internal_peek(p) && !cast_internal_is_punct(p, ")")) {
		size_t param, type;
		char *name;
		uint32_t line;
		int is_fn;
		CastToken *t;

		if (cast_internal_is_punct(p, "...")) {
			cast_internal_eat(p);
			break;
		}
		if (cast_internal_is_punct(p, ",")) {
			cast_internal_eat(p);
			continue;
		}
		t = cast_internal_peek(p);
		param = cast_internal_node_new(p, CAST_NODE_PARAM, t, "param");
		type = cast_internal_parse_specifiers(p);
		if (type != (size_t)-1)
			cast_internal_node_add(p->ast, param, type);
		name = NULL;
		line = 0;
		is_fn = 0;
		cast_internal_parse_declarator(p, param, &name, &line, &is_fn);
		if (param != (size_t)-1) {
			p->ast->nodes[param].name = name;
			if (line)
				p->ast->nodes[param].line = line;
			cast_internal_node_end(p, param);
			cast_internal_node_add(p->ast, parent, param);
		}
		if (cast_internal_is_punct(p, ","))
			cast_internal_eat(p);
		else
			break;
	}
}

void
cast_internal_parse_declarator(CastParser *p, size_t parent, char **name, uint32_t *line, int *is_fn)
{
	CastToken *t, *n;

	/* A name before '*' is a calling convention such as WINAPI. */
	while ((t = cast_internal_peek(p)) && t->type == CAST_TOK_IDENT
	    && (n = cast_internal_peek_n(p, 1)) && cast_internal_tok_text(p, n, "*"))
		cast_internal_eat(p);
	while (cast_internal_is_punct(p, "*")) {
		size_t ptr;

		t = cast_internal_eat(p);
		while (cast_internal_peek(p) && cast_internal_peek(p)->type == CAST_TOK_KW
		    && p->ast->src
		    && cast_internal_is_qual_word(p->ast->src + cast_internal_peek(p)->arg,
			(size_t)cast_internal_peek(p)->arglen))
			cast_internal_eat(p);
		ptr = cast_internal_node_new(p, CAST_NODE_PTR, t, "ptr");
		cast_internal_node_add(p->ast, parent, ptr);
	}
	if (cast_internal_is_punct(p, "(")) {
		cast_internal_eat(p);
		cast_internal_parse_declarator(p, parent, name, line, is_fn);
		cast_internal_eat_punct(p, ")");
	} else if (cast_internal_peek(p) && cast_internal_peek(p)->type == CAST_TOK_IDENT) {
		t = cast_internal_eat(p);
		if (name)
			*name = cast_internal_intern(p->mem, p->ast->src + t->arg, (size_t)t->arglen);
		if (line)
			*line = t->line;
		if (parent != (size_t)-1) {
			p->ast->nodes[parent].arg = t->arg;
			p->ast->nodes[parent].arglen = t->arglen;
		}
	}
	for (;;) {
		if (cast_internal_eat_attribute(p))
			continue;
		if (cast_internal_is_punct(p, "[")) {
			size_t arr;

			t = cast_internal_peek(p);
			arr = cast_internal_node_new(p, CAST_NODE_ARRAY, t, "array");
			cast_internal_skip_balanced(p, "[", "]");
			cast_internal_node_end(p, arr);
			cast_internal_node_add(p->ast, parent, arr);
		} else if (cast_internal_is_punct(p, "(")) {
			if (is_fn)
				*is_fn = 1;
			cast_internal_eat(p);
			cast_internal_parse_params(p, parent);
			cast_internal_eat_punct(p, ")");
		} else {
			break;
		}
	}
}

/* A declaration with all its declarators, or 0 if none starts here. */
int
cast_internal_parse_decl(CastParser *p, size_t parent)
{
	size_t type, decl, t2;
	char *name;
	uint32_t line;
	int is_fn, first;
	CastToken *t;

	t = cast_internal_peek(p);
	type = cast_internal_parse_specifiers(p);
	if (type == (size_t)-1)
		return 0;
	decl = (size_t)-1;
	for (first = 1;; first = 0) {
		name = NULL;
		line = 0;
		is_fn = 0;
		decl = cast_internal_node_new(p, CAST_NODE_DECL, first ? t : cast_internal_peek(p), "decl");
		if (first) {
			cast_internal_node_add(p->ast, decl, type);
		} else {
			/* Later declarators share the type text of the first. */
			t2 = cast_internal_node_new(p, CAST_NODE_TYPE, NULL, "type");
			if (t2 != (size_t)-1 && decl != (size_t)-1) {
				p->ast->nodes[t2].name = p->ast->nodes[type].name;
				p->ast->nodes[t2].line = p->ast->nodes[decl].line;
				p->ast->nodes[t2].tok = p->ast->nodes[decl].tok;
			}
			cast_internal_node_add(p->ast, decl, t2);
		}
		cast_internal_parse_declarator(p, decl, &name, &line, &is_fn);
		if (decl != (size_t)-1) {
			p->ast->nodes[decl].name = name;
			if (line)
				p->ast->nodes[decl].line = line;
			if (is_fn) {
				p->ast->nodes[decl].kind = CAST_NODE_FUNC;
				p->ast->nodes[decl].msg = "func";
			}
		}
		cast_internal_node_add(p->ast, parent, decl);
		if (cast_internal_eat_punct(p, ":"))
			cast_internal_parse_expr(p, decl, ",;");
		if (cast_internal_eat_punct(p, "="))
			cast_internal_parse_initializer(p, decl);
		if (is_fn && cast_internal_is_punct(p, "{")) {
			if (p->flags & CAST_PARSE_BODIES)
				cast_internal_parse_block(p, decl);
			else
				cast_internal_skip_balanced(p, "{", "}");
			cast_internal_node_end(p, decl);
			return 1;
		}
		cast_internal_node_end(p, decl);
		if (!cast_internal_eat_punct(p, ","))
			break;
	}
	if (!cast_internal_eat_punct(p, ";")) {
		cast_internal_scan(p, ";");
		cast_internal_eat_punct(p, ";");
	}
	cast_internal_node_end(p, decl);
	return 1;
}

void
cast_internal_parse_block(CastParser *p, size_t parent)
{
	size_t block, before;

	block = cast_internal_node_new(p, CAST_NODE_BLOCK, cast_internal_eat(p), "block");
	cast_internal_node_add(p->ast, parent, block);
	p->depth++;
	while (cast_internal_peek(p) && !cast_internal_is_punct(p, "}") && !p->oom) {
		before = p->i;
		cast_internal_parse_item(p, block);
		if (p->i == before)
			cast_internal_eat(p);
	}
	p->depth--;
	cast_internal_eat_punct(p, "}");
	cast_internal_node_end(p, block);
}

void
cast_internal_parse_item(CastParser *p, size_t parent)
{
	CastToken *t, *n;

	t = cast_internal_peek(p);
	n = cast_internal_peek_n(p, 1);
	if (t->type == CAST_TOK_IDENT && n && cast_internal_tok_text(p, n, ":"))
		cast_internal_parse_stmt(p, parent);
	else if (!cast_internal_parse_decl(p, parent))
		cast_internal_parse_stmt(p, parent);
}

void
cast_internal_parse_stmt(CastParser *p, size_t parent)
{
	static const struct {
		const char *kw;
		uint32_t kind;
	} kinds[] = {
		{"if", CAST_NODE_IF}, {"switch", CAST_NODE_SWITCH}, {"while", CAST_NODE_WHILE},
		{"do", CAST_NODE_DO}, {"for", CAST_NODE_FOR}, {"case", CAST_NODE_CASE},
		{"default", CAST_NODE_CASE}, {"return", CAST_NODE_RETURN},
		{"break", CAST_NODE_BREAK}, {"continue", CAST_NODE_CONTINUE}, {"goto", CAST_NODE_GOTO}
	};
	CastToken *t, *n;
	const char *msg;
	size_t s, e, i;
	uint32_t kind;

	if (!(t = cast_internal_peek(p)))
		return;
	n = cast_internal_peek_n(p, 1);
	if (cast_internal_is_punct(p, "{")) {
		cast_internal_parse_block(p, parent);
		return;
	}
	kind = CAST_NODE_EXPR;
	msg = "expr";
	if (cast_internal_is_punct(p, ";")) {
		kind = CAST_NODE_EMPTY;
		msg = "empty";
	} else if (t->type == CAST_TOK_IDENT && n && cast_internal_tok_text(p, n, ":")) {
		kind = CAST_NODE_LABEL;
		msg = "label";
	}
	for (i = 0; t->type == CAST_TOK_KW && i < sizeof kinds / sizeof *kinds; i++) {
		if (cast_internal_tok_text(p, t, kinds[i].kw)) {
			kind = kinds[i].kind;
			msg = kinds[i].kw;
		}
	}
	if (kind == CAST_NODE_EXPR) {
		/* An expression statement spans its ';'. */
		e = cast_internal_parse_expr(p, parent, ";");
		cast_internal_eat_punct(p, ";");
		cast_internal_node_end(p, e);
		return;
	}
	s = cast_internal_node_new(p, kind, t, msg);
	cast_internal_node_add(p->ast, parent, s);
	cast_internal_eat(p);
	switch (kind) {
	case CAST_NODE_LABEL:
		cast_internal_node_name(p, s, t);
		cast_internal_eat(p);
		break;
	case CAST_NODE_IF:
		cast_internal_parse_paren(p, s);
		cast_internal_parse_stmt(p, s);
		if (cast_internal_is_kw(p, "else")) {
			e = cast_internal_node_new(p, CAST_NODE_ELSE, cast_internal_eat(p), "else");
			cast_internal_node_add(p->ast, s, e);
			cast_internal_parse_stmt(p, e);
			cast_internal_node_end(p, e);
		}
		break;
	case CAST_NODE_SWITCH: /* FALLTHROUGH */
	case CAST_NODE_WHILE:
		cast_internal_parse_paren(p, s);
		cast_internal_parse_stmt(p, s);
		break;
	case CAST_NODE_DO:
		cast_internal_parse_stmt(p, s);
		if (cast_internal_is_kw(p, "while")) {
			cast_internal_eat(p);
			cast_internal_parse_paren(p, s);
		}
		cast_internal_eat_punct(p, ";");
		break;
	case CAST_NODE_FOR:
		cast_internal_eat_punct(p, "(");
		if (!cast_internal_parse_decl(p, s)) {
			cast_internal_parse_expr(p, s, ";");
			cast_internal_eat_punct(p, ";");
		}
		cast_internal_parse_expr(p, s, ";");
		cast_internal_eat_punct(p, ";");
		cast_internal_parse_expr(p, s, ")");
		cast_internal_eat_punct(p, ")");
		cast_internal_parse_stmt(p, s);
		break;
	case CAST_NODE_CASE:
		if (cast_internal_tok_text(p, t, "case"))
			cast_internal_parse_expr(p, s, ":");
		cast_internal_eat_punct(p, ":");
		break;
	case CAST_NODE_RETURN:
		if (!cast_internal_is_punct(p, ";"))
			cast_internal_parse_expr(p, s, ";");
		cast_internal_eat_punct(p, ";");
		break;
	case CAST_NODE_GOTO:
		if (cast_internal_peek(p) && cast_internal_peek(p)->type == CAST_TOK_IDENT)
			cast_internal_node_name(p, s, cast_internal_eat(p));
		cast_internal_eat_punct(p, ";");
		break;
	case CAST_NODE_BREAK: /* FALLTHROUGH */
	case CAST_NODE_CONTINUE:
		cast_internal_eat_punct(p, ";");
		break;
	default:
		break;
	}
	cast_internal_node_end(p, s);
}

void
cast_internal_parse_external(CastParser *p, size_t tu)
{
	size_t err;
	CastToken *t, *n;

	t = cast_internal_peek(p);
	if (cast_internal_parse_decl(p, tu))
		return;
	err = cast_internal_node_new(p, CAST_NODE_ERROR, t, "error");
	cast_internal_node_name(p, err, t);
	cast_internal_node_add(p->ast, tu, err);
	if (t->type == CAST_TOK_ERROR) {
		/* Foreign tokens such as Objective-C's "@end" stand alone. */
		cast_internal_eat(p);
		n = cast_internal_peek(p);
		if (n && (n->type == CAST_TOK_IDENT || n->type == CAST_TOK_KW) && n->arg == t->arg + t->arglen)
			cast_internal_eat(p);
	} else {
		if (!cast_internal_is_punct(p, "{"))
			cast_internal_eat(p);
		/* Skip to the ';', or past a { } body, which ends a definition. */
		while (cast_internal_peek(p) && !cast_internal_is_punct(p, ";")) {
			if (cast_internal_is_punct(p, "{")) {
				cast_internal_skip_balanced(p, "{", "}");
				cast_internal_node_end(p, err);
				return;
			}
			if (cast_internal_is_punct(p, "("))
				cast_internal_skip_balanced(p, "(", ")");
			else if (cast_internal_is_punct(p, "["))
				cast_internal_skip_balanced(p, "[", "]");
			else
				cast_internal_eat(p);
		}
		cast_internal_eat_punct(p, ";");
	}
	cast_internal_node_end(p, err);
}

void
cast_internal_parse_tu(CastParser *p)
{
	size_t tu;

	tu = cast_internal_node_new(p, CAST_NODE_TU, NULL, "tu");
	while (cast_internal_peek(p) && !p->oom) {
		size_t before;

		before = p->i;
		cast_internal_parse_external(p, tu);
		if (p->i == before)
			cast_internal_eat(p);
	}
	if (tu != (size_t)-1)
		p->ast->nodes[tu].ntok = p->ntoks;
}

CastMemory
cast_memory_create(void *buf, size_t size)
{
	CastMemory m;
	uintptr_t u;
	size_t pad;

	memset(&m, 0, sizeof m);
	if (!buf || size == 0)
		return m;
	u = (uintptr_t)buf;
	pad = (CAST_ALIGN - (u & (CAST_ALIGN - 1))) & (CAST_ALIGN - 1);
	if (pad >= size)
		return m;
	m.buf = (unsigned char *)buf + pad;
	m.cap = size - pad;
	return m;
}

CastToken *
cast_tokenize(CastMemory *mem, char *str, size_t len, size_t *out_len)
{
	return cast_tokenize_opt(mem, str, len, 0, out_len);
}

CastToken *
cast_tokenize_opt(CastMemory *mem, char *str, size_t len, uint32_t flags, size_t *out_len)
{
	CastToken *toks;
	size_t n, cap;

	CASSERT(mem);
	CASSERT(out_len);
	if (!out_len)
		return NULL;
	*out_len = 0;
	if (!mem || !mem->buf)
		return NULL;
	if (!str && len != 0)
		return NULL;
	mem->src = str ? str : "";
	mem->src_len = str ? len : 0;
	if (cast_internal_lex(mem->src, mem->src_len, flags, NULL, 0, &n) != 0)
		return NULL;
	if (n == 0)
		return NULL;
	cap = n;
	toks = cast_internal_alloc(mem, cap * sizeof *toks);
	if (!toks)
		return NULL;
	if (cast_internal_lex(mem->src, mem->src_len, flags, toks, cap, &n) != 0)
		return NULL;
	*out_len = n;
	return toks;
}

char *
cast_token_type(uint32_t type)
{
	switch (type) {
	case CAST_TOK_EOF:
		return "EOF";
	case CAST_TOK_IDENT:
		return "IDENT";
	case CAST_TOK_NUMBER:
		return "NUMBER";
	case CAST_TOK_STRING:
		return "STRING";
	case CAST_TOK_CHAR:
		return "CHAR";
	case CAST_TOK_PUNCT:
		return "PUNCT";
	case CAST_TOK_KW:
		return "KW";
	case CAST_TOK_ERROR:
		return "ERROR";
	case CAST_TOK_COMMENT:
		return "COMMENT";
	case CAST_TOK_PP:
		return "PP";
	default:
		return "UNKNOWN";
	}
}

CastAst *
cast_ast(CastMemory *mem, CastToken *tokens, size_t n_tokens, size_t *out_nodes)
{
	return cast_ast_opt(mem, tokens, n_tokens, 0, out_nodes);
}

CastAst *
cast_ast_opt(CastMemory *mem, CastToken *tokens, size_t n_tokens, uint32_t flags, size_t *out_nodes)
{
	CastAst *ast;
	CastParser p;
	size_t cap, i;

	CASSERT(mem);
	if (out_nodes)
		*out_nodes = 0;
	if (!mem || !mem->buf)
		return NULL;
	if (!tokens && n_tokens != 0)
		return NULL;
	ast = cast_internal_alloc(mem, sizeof *ast);
	if (!ast)
		return NULL;
	cap = n_tokens * 4 + 8;
	ast->nodes = cast_internal_alloc(mem, cap * sizeof *ast->nodes);
	if (!ast->nodes)
		return NULL;
	ast->cap = cap;
	ast->n = 0;
	ast->src = mem->src;
	ast->src_len = mem->src_len;
	memset(&p, 0, sizeof p);
	p.mem = mem;
	p.ast = ast;
	p.toks = tokens;
	p.ntoks = n_tokens;
	p.flags = flags;
	p.idx = cast_internal_alloc(mem, n_tokens * sizeof *p.idx);
	if (!p.idx)
		return NULL;
	for (i = 0; i < n_tokens; i++) {
		if (tokens[i].type != CAST_TOK_COMMENT && tokens[i].type != CAST_TOK_PP)
			p.idx[p.nidx++] = i;
	}
	cast_internal_parse_tu(&p);
	if (p.oom)
		return NULL;
	if (out_nodes)
		*out_nodes = ast->n;
	return ast;
}

CastNode
cast_ast_node(CastAst *ast, size_t i)
{
	CastNode z;

	memset(&z, 0, sizeof z);
	if (!ast || i >= ast->n)
		return z;
	return ast->nodes[i];
}

void
cast_memory_clear(CastMemory *mem)
{
	CASSERT(mem);
	if (!mem)
		return;
	mem->len = 0;
	mem->src = NULL;
	mem->src_len = 0;
}

#endif /* CAST_IMPLEMENTATION_ONCE */
#endif /* CAST_IMPLEMENTATION */
