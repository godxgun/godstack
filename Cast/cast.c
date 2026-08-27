#include "cast.h"

#ifndef CAST_ALIGN
#define CAST_ALIGN (sizeof (void *))
#endif

enum {
	CAST_TOK_EOF = 0,
	CAST_TOK_IDENT,
	CAST_TOK_NUMBER,
	CAST_TOK_STRING,
	CAST_TOK_CHAR,
	CAST_TOK_PUNCT,
	CAST_TOK_KW,
	CAST_TOK_ERROR
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
	CAST_NODE_ERROR
};

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
	size_t i;
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
static uint32_t cast_internal_line(const char *src, size_t len, size_t off);
static int cast_internal_is_ident_start(int c);
static int cast_internal_is_ident_cont(int c);
static int cast_internal_is_digit(int c);
static int cast_internal_is_kw_word(const char *s, size_t n);
static int cast_internal_is_spec_word(const char *s, size_t n);
static int cast_internal_is_qual_word(const char *s, size_t n);
static size_t cast_internal_skip_ws(const char *s, size_t n, size_t i, int *bol);
static size_t cast_internal_punct_len(const char *s, size_t n, size_t i);
static int cast_internal_lex_one(const char *s, size_t n, size_t *i, int *bol, CastToken *out);
static int cast_internal_lex(const char *s, size_t n, CastToken *out, size_t cap, size_t *n_out);

static size_t cast_internal_node_new(CastParser *p, uint32_t kind, CastToken *t, const char *msg);
static void cast_internal_node_add(CastAst *ast, size_t parent, size_t child);
static CastToken *cast_internal_peek(CastParser *p);
static CastToken *cast_internal_peek_n(CastParser *p, size_t n);
static CastToken *cast_internal_eat(CastParser *p);
static int cast_internal_tok_text(CastParser *p, CastToken *t, const char *s);
static int cast_internal_is_punct(CastParser *p, const char *s);
static int cast_internal_is_kw(CastParser *p, const char *kw);
static int cast_internal_eat_punct(CastParser *p, const char *s);
static int cast_internal_at_specifier(CastParser *p);
static void cast_internal_skip_balanced(CastParser *p, const char *open, const char *close);
static void cast_internal_skip_to_punct(CastParser *p, const char *s);
static size_t cast_internal_parse_specifiers(CastParser *p);
static void cast_internal_parse_params(CastParser *p, size_t parent);
static void cast_internal_parse_declarator(CastParser *p, size_t parent, char **name, uint32_t *line, int *is_fn);
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

uint32_t
cast_internal_line(const char *src, size_t len, size_t off)
{
	uint32_t line;
	size_t i;

	if (!src)
		return 0;
	if (off > len)
		off = len;
	line = 1;
	for (i = 0; i < off; i++) {
		if (src[i] == '\n')
			line++;
	}
	return line;
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
cast_internal_skip_ws(const char *s, size_t n, size_t i, int *bol)
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

int
cast_internal_lex_one(const char *s, size_t n, size_t *i, int *bol, CastToken *out)
{
	size_t start, plen, pre;
	int q;
	CastToken tok;

	*i = cast_internal_skip_ws(s, n, *i, bol);
	if (*i >= n)
		return 0;
	start = *i;
	memset(&tok, 0, sizeof tok);
	tok.arg = (int)start;

	pre = 0;
	if (*i + 2 < n && s[*i] == 'u' && s[*i + 1] == '8'
	    && (s[*i + 2] == '"' || s[*i + 2] == '\''))
		pre = 2;
	else if (*i + 1 < n && (s[*i] == 'L' || s[*i] == 'U' || s[*i] == 'u')
	    && (s[*i + 1] == '"' || s[*i + 1] == '\''))
		pre = 1;
	if (pre) {
		*i += pre;
		q = s[*i];
		tok.type = (q == '"') ? CAST_TOK_STRING : CAST_TOK_CHAR;
		*i += 1;
		while (*i < n && s[*i] != q) {
			if (s[*i] == '\\' && *i + 1 < n)
				*i += 2;
			else
				*i += 1;
		}
		if (*i < n)
			*i += 1;
		else
			tok.type = CAST_TOK_ERROR;
		tok.arglen = (int)(*i - start);
		*bol = 0;
		if (out)
			*out = tok;
		return 1;
	}

	if (s[*i] == '"' || s[*i] == '\'') {
		q = s[*i];
		tok.type = (q == '"') ? CAST_TOK_STRING : CAST_TOK_CHAR;
		*i += 1;
		while (*i < n && s[*i] != q) {
			if (s[*i] == '\\' && *i + 1 < n)
				*i += 2;
			else
				*i += 1;
		}
		if (*i < n)
			*i += 1;
		else
			tok.type = CAST_TOK_ERROR;
		tok.arglen = (int)(*i - start);
		*bol = 0;
		if (out)
			*out = tok;
		return 1;
	}

	if (cast_internal_is_digit((unsigned char)s[*i])
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
		tok.arglen = (int)(*i - start);
		*bol = 0;
		if (out)
			*out = tok;
		return 1;
	}

	if (cast_internal_is_ident_start((unsigned char)s[*i])) {
		*i += 1;
		while (*i < n && cast_internal_is_ident_cont((unsigned char)s[*i]))
			*i += 1;
		tok.arglen = (int)(*i - start);
		tok.type = cast_internal_is_kw_word(s + start, (size_t)tok.arglen)
		    ? CAST_TOK_KW : CAST_TOK_IDENT;
		*bol = 0;
		if (out)
			*out = tok;
		return 1;
	}

	plen = cast_internal_punct_len(s, n, *i);
	if (plen) {
		tok.type = CAST_TOK_PUNCT;
		tok.arglen = (int)plen;
		*i += plen;
		*bol = 0;
		if (out)
			*out = tok;
		return 1;
	}

	tok.type = CAST_TOK_ERROR;
	tok.arglen = 1;
	*i += 1;
	*bol = 0;
	if (out)
		*out = tok;
	return 1;
}

int
cast_internal_lex(const char *s, size_t n, CastToken *out, size_t cap, size_t *n_out)
{
	size_t i, count;
	int bol;

	i = 0;
	count = 0;
	bol = 1;
	while (cast_internal_lex_one(s, n, &i, &bol, NULL)) {
		if (out) {
			if (count >= cap)
				return -1;
			i = 0;
			break;
		}
		count++;
	}
	if (!out) {
		*n_out = count;
		return 0;
	}
	i = 0;
	count = 0;
	bol = 1;
	while (count < cap && cast_internal_lex_one(s, n, &i, &bol, &out[count]))
		count++;
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
	if (t) {
		n->arg = t->arg;
		n->arglen = t->arglen;
		n->line = cast_internal_line(p->ast->src, p->ast->src_len, (size_t)t->arg);
	} else {
		n->line = 1;
	}
	return i;
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

CastToken *
cast_internal_peek(CastParser *p)
{
	if (!p || p->i >= p->ntoks)
		return NULL;
	return &p->toks[p->i];
}

CastToken *
cast_internal_peek_n(CastParser *p, size_t n)
{
	if (!p || p->i + n >= p->ntoks)
		return NULL;
	return &p->toks[p->i + n];
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

	t = cast_internal_peek(p);
	if (!t)
		return 0;
	if (t->type == CAST_TOK_KW && p->ast->src
	    && cast_internal_is_spec_word(p->ast->src + t->arg, (size_t)t->arglen))
		return 1;
	if (t->type != CAST_TOK_IDENT)
		return 0;
	n = cast_internal_peek_n(p, 1);
	if (!n)
		return 0;
	if (n->type == CAST_TOK_IDENT)
		return 1;
	if (n->type == CAST_TOK_PUNCT && cast_internal_tok_text(p, n, "*"))
		return 1;
	if (n->type == CAST_TOK_KW && p->ast->src
	    && cast_internal_is_qual_word(p->ast->src + n->arg, (size_t)n->arglen))
		return 1;
	return 0;
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

void
cast_internal_skip_to_punct(CastParser *p, const char *s)
{
	while (cast_internal_peek(p) && !cast_internal_is_punct(p, s)) {
		if (cast_internal_is_punct(p, "{"))
			cast_internal_skip_balanced(p, "{", "}");
		else if (cast_internal_is_punct(p, "("))
			cast_internal_skip_balanced(p, "(", ")");
		else if (cast_internal_is_punct(p, "["))
			cast_internal_skip_balanced(p, "[", "]");
		else
			cast_internal_eat(p);
	}
}

size_t
cast_internal_parse_specifiers(CastParser *p)
{
	CastToken *t, *start;
	size_t type, begin, end;
	int any;

	t = cast_internal_peek(p);
	if (!t)
		return (size_t)-1;
	start = t;
	begin = (size_t)t->arg;
	end = begin;
	any = 0;
	while (cast_internal_at_specifier(p)) {
		t = cast_internal_peek(p);
		if (cast_internal_is_kw(p, "struct")
		    || cast_internal_is_kw(p, "union")
		    || cast_internal_is_kw(p, "enum")) {
			t = cast_internal_eat(p);
			if (cast_internal_peek(p) && cast_internal_peek(p)->type == CAST_TOK_IDENT)
				t = cast_internal_eat(p);
			if (cast_internal_is_punct(p, "{")) {
				cast_internal_skip_balanced(p, "{", "}");
				t = NULL;
			}
		} else {
			t = cast_internal_eat(p);
		}
		if (t)
			end = (size_t)t->arg + (size_t)t->arglen;
		else if (p->i > 0) {
			CastToken *prev;

			prev = &p->toks[p->i - 1];
			end = (size_t)prev->arg + (size_t)prev->arglen;
		}
		any = 1;
	}
	if (!any)
		return (size_t)-1;
	type = cast_internal_node_new(p, CAST_NODE_TYPE, start, "type");
	if (type == (size_t)-1)
		return type;
	p->ast->nodes[type].name = cast_internal_intern(p->mem, p->ast->src + begin, end - begin);
	if (!p->ast->nodes[type].name)
		p->oom = 1;
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
	CastToken *t;

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
			*line = cast_internal_line(p->ast->src, p->ast->src_len, (size_t)t->arg);
		if (parent != (size_t)-1) {
			p->ast->nodes[parent].arg = t->arg;
			p->ast->nodes[parent].arglen = t->arglen;
		}
	}
	for (;;) {
		if (cast_internal_is_punct(p, "[")) {
			size_t arr;

			t = cast_internal_peek(p);
			cast_internal_skip_balanced(p, "[", "]");
			arr = cast_internal_node_new(p, CAST_NODE_ARRAY, t, "array");
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

void
cast_internal_parse_external(CastParser *p, size_t tu)
{
	size_t type, decl, before;
	char *name;
	uint32_t line;
	int is_fn;
	CastToken *t;

	before = p->i;
	t = cast_internal_peek(p);
	type = cast_internal_parse_specifiers(p);
	if (type == (size_t)-1) {
		size_t err;

		err = cast_internal_node_new(p, CAST_NODE_ERROR, t, "error");
		if (err != (size_t)-1 && t && p->ast->src)
			p->ast->nodes[err].name = cast_internal_intern(p->mem,
			    p->ast->src + t->arg, (size_t)t->arglen);
		cast_internal_node_add(p->ast, tu, err);
		if (p->i == before)
			cast_internal_eat(p);
		cast_internal_skip_to_punct(p, ";");
		cast_internal_eat_punct(p, ";");
		return;
	}
	name = NULL;
	line = 0;
	is_fn = 0;
	decl = cast_internal_node_new(p, CAST_NODE_DECL, t, "decl");
	cast_internal_node_add(p->ast, decl, type);
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
	cast_internal_node_add(p->ast, tu, decl);
	if (cast_internal_is_punct(p, "{"))
		cast_internal_skip_balanced(p, "{", "}");
	else {
		while (cast_internal_is_punct(p, ",")) {
			size_t d2, t2;

			cast_internal_eat(p);
			name = NULL;
			line = 0;
			is_fn = 0;
			d2 = cast_internal_node_new(p, CAST_NODE_DECL, cast_internal_peek(p), "decl");
			t2 = cast_internal_node_new(p, CAST_NODE_TYPE, NULL, "type");
			if (t2 != (size_t)-1 && type != (size_t)-1)
				p->ast->nodes[t2].name = p->ast->nodes[type].name;
			cast_internal_node_add(p->ast, d2, t2);
			cast_internal_parse_declarator(p, d2, &name, &line, &is_fn);
			if (d2 != (size_t)-1) {
				p->ast->nodes[d2].name = name;
				if (line)
					p->ast->nodes[d2].line = line;
				if (is_fn) {
					p->ast->nodes[d2].kind = CAST_NODE_FUNC;
					p->ast->nodes[d2].msg = "func";
				}
			}
			cast_internal_node_add(p->ast, tu, d2);
		}
		if (!cast_internal_eat_punct(p, ";")) {
			cast_internal_skip_to_punct(p, ";");
			cast_internal_eat_punct(p, ";");
		}
	}
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
	if (cast_internal_lex(mem->src, mem->src_len, NULL, 0, &n) != 0)
		return NULL;
	if (n == 0)
		return NULL;
	cap = n;
	toks = cast_internal_alloc(mem, cap * sizeof *toks);
	if (!toks)
		return NULL;
	if (cast_internal_lex(mem->src, mem->src_len, toks, cap, &n) != 0)
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
	default:
		return "UNKNOWN";
	}
}

CastAst *
cast_ast(CastMemory *mem, CastToken *tokens, size_t n_tokens, size_t *out_nodes)
{
	CastAst *ast;
	CastParser p;
	size_t cap;

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
	if (cap < 8)
		cap = 8;
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
