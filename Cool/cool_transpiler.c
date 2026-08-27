/*
 * Transpile .cool templates to C.
 *
 *     cool_transpiler input.cool [-o out.c]
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cast.h"
#include "cast.c"

#define COOL_PARAM_MAX 64
#define COOL_NAME_MAX 64
#define COOL_TYPE_MAX 64
#define COOL_FUNC_MAX 128
#define COOL_SIG_MAX 4096
#define COOL_EXPR_MAX 256

typedef struct {
	char name[COOL_NAME_MAX];
	char type[COOL_TYPE_MAX];
} CoolParam;

typedef struct {
	const char *filename;
	char *src;
	size_t src_len;
	int line_num;
	int in_func;
	char func[COOL_FUNC_MAX];
	CoolParam params[COOL_PARAM_MAX];
	size_t nparams;
	char *raw;
	size_t raw_len;
	size_t raw_cap;
	char *out;
	size_t out_len;
	size_t out_cap;
	CastMemory mem;
} CoolState;

static unsigned char cool_arena[1 << 20];

static void usage(void);
static void cool_die(const char *msg);
static int cool_is_word(int c);
static char *cool_file_read(const char *path, size_t *out_len);
static void cool_buf_push(char **buf, size_t *len, size_t *cap, const char *s, size_t n);
static void cool_out_n(CoolState *st, const char *s, size_t n);
static void cool_out_s(CoolState *st, const char *s);
static void cool_out_fmt(CoolState *st, const char *fmt, ...);
static void cool_raw_push(CoolState *st, const char *s, size_t n);
static void cool_raw_flush(CoolState *st);
static const char *cool_spec(const char *type);
static int cool_is_char_ptr(const char *type);
static CoolParam *cool_param_find(CoolState *st, const char *name);
static int cool_emit_expr(CoolState *st, const char *expr);
static void cool_emit_call(CoolState *st, const char *s, size_t n);
static int cool_body_line(CoolState *st, const char *line, size_t n);
static int cool_line_is_end(const char *s, size_t n);
static int cool_tok_eq(CastToken t, const char *src, const char *type, const char *lex);
static int cool_params_from_ast(CoolState *st, CastAst *ast, size_t n_nodes);
static int cool_header(CoolState *st, char *line, size_t n);
static int cool_transpile(CoolState *st);

void
usage(void)
{
	fprintf(stderr, "Usage: cool_transpiler input.cool [-o out.c]\n");
}

void
cool_die(const char *msg)
{
	fprintf(stderr, "cool_transpiler: %s\n", msg);
	exit(1);
}

int
cool_is_word(int c)
{
	return (c >= 'a' && c <= 'z')
	    || (c >= 'A' && c <= 'Z')
	    || (c >= '0' && c <= '9')
	    || c == '_';
}

char *
cool_file_read(const char *path, size_t *out_len)
{
	FILE *f;
	char *buf;
	long sz;
	size_t n;

	f = fopen(path, "rb");
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	sz = ftell(f);
	if (sz < 0) {
		fclose(f);
		return NULL;
	}
	rewind(f);
	buf = malloc((size_t)sz + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	n = fread(buf, 1, (size_t)sz, f);
	fclose(f);
	buf[n] = 0;
	if (out_len)
		*out_len = n;
	return buf;
}

void
cool_buf_push(char **buf, size_t *len, size_t *cap, const char *s, size_t n)
{
	if (*len + n + 1 > *cap) {
		size_t next;
		char *p;

		next = *cap ? *cap : 256;
		while (next < *len + n + 1)
			next *= 2;
		p = realloc(*buf, next);
		if (!p)
			cool_die("out of memory");
		*buf = p;
		*cap = next;
	}
	memcpy(*buf + *len, s, n);
	*len += n;
	(*buf)[*len] = 0;
}

void
cool_out_n(CoolState *st, const char *s, size_t n)
{
	cool_buf_push(&st->out, &st->out_len, &st->out_cap, s, n);
}

void
cool_out_s(CoolState *st, const char *s)
{
	cool_out_n(st, s, strlen(s));
}

void
cool_out_fmt(CoolState *st, const char *fmt, ...)
{
	va_list ap;
	int n;
	char stack[512];
	char *heap;

	heap = NULL;
	va_start(ap, fmt);
	n = vsnprintf(stack, sizeof stack, fmt, ap);
	va_end(ap);
	if (n < 0)
		cool_die("format failed");
	if ((size_t)n < sizeof stack) {
		cool_out_n(st, stack, (size_t)n);
		return;
	}
	heap = malloc((size_t)n + 1);
	if (!heap)
		cool_die("out of memory");
	va_start(ap, fmt);
	n = vsnprintf(heap, (size_t)n + 1, fmt, ap);
	va_end(ap);
	if (n < 0) {
		free(heap);
		cool_die("format failed");
	}
	cool_out_n(st, heap, (size_t)n);
	free(heap);
}

void
cool_raw_push(CoolState *st, const char *s, size_t n)
{
	cool_buf_push(&st->raw, &st->raw_len, &st->raw_cap, s, n);
}

void
cool_raw_flush(CoolState *st)
{
	size_t i, n, cap;
	char *e;

	if (!st->raw_len)
		return;
	cap = st->raw_len * 2 + 1;
	e = malloc(cap);
	if (!e)
		cool_die("out of memory");
	n = 0;
	for (i = 0; i < st->raw_len; i++) {
		char c;

		c = st->raw[i];
		if (c == '\r')
			continue;
		if (c == '\\' || c == '"') {
			e[n++] = '\\';
			e[n++] = c;
		} else if (c == '\n') {
			e[n++] = '\\';
			e[n++] = 'n';
		} else {
			e[n++] = c;
		}
	}
	e[n] = 0;
	st->raw_len = 0;
	if (n == 0) {
		free(e);
		return;
	}
	cool_out_s(st, "    cool_html_raw(COOL_SV(\"");
	cool_out_n(st, e, n);
	cool_out_s(st, "\"));\n");
	free(e);
}

const char *
cool_spec(const char *type)
{
	static const char *tab[][2] = {
		{"int", "%d"},
		{"int32_t", "%d"},
		{"short", "%d"},
		{"long", "%ld"},
		{"unsigned", "%u"},
		{"unsigned int", "%u"},
		{"uint32_t", "%u"},
		{"size_t", "%zu"},
		{"float", "%f"},
		{"double", "%f"},
		{"char", "%c"},
		{"char*", "%s"},
		{"const char*", "%s"},
		{"char *", "%s"},
		{"const char *", "%s"},
	};
	size_t i;

	for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
		if (strcmp(tab[i][0], type) == 0)
			return tab[i][1];
	}
	return "%d";
}

int
cool_is_char_ptr(const char *type)
{
	return strstr(type, "char*") != NULL;
}

CoolParam *
cool_param_find(CoolState *st, const char *name)
{
	size_t i;

	for (i = 0; i < st->nparams; i++) {
		if (strcmp(st->params[i].name, name) == 0)
			return &st->params[i];
	}
	return NULL;
}

int
cool_emit_expr(CoolState *st, const char *expr)
{
	CoolParam *p;
	char avail[512];
	size_t i, n;

	p = cool_param_find(st, expr);
	if (p) {
		if (cool_is_char_ptr(p->type))
			cool_out_fmt(st, "    cool_html_txt(%s, strlen(%s));\n", expr, expr);
		else
			cool_out_fmt(st, "    cool_htmlf_raw(\"%s\", %s);\n", cool_spec(p->type), expr);
		return 0;
	}
	n = 0;
	avail[0] = 0;
	if (st->nparams == 0) {
		snprintf(avail, sizeof avail, "none");
	} else {
		for (i = 0; i < st->nparams; i++) {
			int w;

			w = snprintf(avail + n, sizeof avail - n, "%s'%s'",
			    i ? ", " : "", st->params[i].name);
			if (w < 0 || (size_t)w >= sizeof avail - n)
				break;
			n += (size_t)w;
		}
	}
	fprintf(stderr,
	    "\n[Transpiler Error] %s:%d: Expression '{%s}' in function '%s' does not match any parameter.\n"
	    "  -> Available parameter(s): %s\n\n",
	    st->filename, st->line_num, expr, st->func, avail);
	return 1;
}

void
cool_emit_call(CoolState *st, const char *s, size_t n)
{
	cool_out_fmt(st, "    %.*s;\n", (int)n, s);
}

int
cool_body_line(CoolState *st, const char *line, size_t n)
{
	size_t i;

	i = 0;
	while (i < n) {
		if (line[i] == '@' && i + 1 < n && cool_is_word((unsigned char)line[i + 1])) {
			size_t j, k, m;

			j = i + 1;
			while (j < n && cool_is_word((unsigned char)line[j]))
				j++;
			k = j;
			while (k < n && isspace((unsigned char)line[k]))
				k++;
			if (k < n && line[k] == '(') {
				m = k + 1;
				while (m < n && line[m] != ')')
					m++;
				if (m < n && line[m] == ')') {
					cool_raw_flush(st);
					cool_emit_call(st, line + i + 1, m - i);
					i = m + 1;
					continue;
				}
			}
		}
		if (line[i] == '{') {
			size_t j;
			int any;

			j = i + 1;
			any = 0;
			while (j < n && line[j] != '{' && line[j] != '}') {
				any = 1;
				j++;
			}
			if (any && j < n && line[j] == '}') {
				char expr[COOL_EXPR_MAX];
				size_t a, b, elen;

				a = i + 1;
				b = j;
				while (a < b && isspace((unsigned char)line[a]))
					a++;
				while (b > a && isspace((unsigned char)line[b - 1]))
					b--;
				elen = b - a;
				if (elen >= sizeof expr)
					cool_die("expression too long");
				memcpy(expr, line + a, elen);
				expr[elen] = 0;
				cool_raw_flush(st);
				if (cool_emit_expr(st, expr) != 0)
					return 1;
				i = j + 1;
				continue;
			}
		}
		cool_raw_push(st, line + i, 1);
		i++;
	}
	return 0;
}

int
cool_line_is_end(const char *s, size_t n)
{
	size_t i, j;

	i = 0;
	while (i < n && isspace((unsigned char)s[i]))
		i++;
	if (i >= n || s[i] != '}')
		return 0;
	j = i + 1;
	while (j < n && isspace((unsigned char)s[j]))
		j++;
	return j == n;
}

int
cool_tok_eq(CastToken t, const char *src, const char *type, const char *lex)
{
	size_t n;

	n = strlen(lex);
	if (strcmp(cast_token_type(t.type), type) != 0)
		return 0;
	if ((size_t)t.arglen != n)
		return 0;
	if (memcmp(src + t.arg, lex, n) != 0)
		return 0;
	return 1;
}

int
cool_params_from_ast(CoolState *st, CastAst *ast, size_t n_nodes)
{
	size_t i, c;

	st->nparams = 0;
	for (i = 0; i < n_nodes; i++) {
		CastNode fn;

		fn = cast_ast_node(ast, i);
		if (!fn.msg || strcmp(fn.msg, "func") != 0)
			continue;
		for (c = fn.first_child; c; c = cast_ast_node(ast, c).next_sibling) {
			CastNode p, ch;
			const char *type_name;
			int nptrs;
			size_t k;
			CoolParam *out;
			size_t tlen;

			p = cast_ast_node(ast, c);
			if (!p.msg || strcmp(p.msg, "param") != 0)
				continue;
			if (!p.name || strcmp(p.name, "void") == 0)
				continue;
			type_name = NULL;
			nptrs = 0;
			for (k = p.first_child; k; k = ch.next_sibling) {
				ch = cast_ast_node(ast, k);
				if (ch.msg && strcmp(ch.msg, "type") == 0)
					type_name = ch.name;
				else if (ch.msg && strcmp(ch.msg, "ptr") == 0)
					nptrs++;
			}
			if (type_name && strcmp(type_name, "void") == 0 && nptrs == 0)
				continue;
			if (!type_name)
				type_name = "";
			if (st->nparams >= COOL_PARAM_MAX)
				cool_die("too many parameters");
			if (strlen(p.name) >= COOL_NAME_MAX)
				cool_die("parameter name too long");
			tlen = strlen(type_name) + (size_t)nptrs;
			if (tlen >= COOL_TYPE_MAX)
				cool_die("parameter type too long");
			out = &st->params[st->nparams++];
			memset(out, 0, sizeof *out);
			memcpy(out->name, p.name, strlen(p.name));
			memcpy(out->type, type_name, strlen(type_name));
			while (nptrs--)
				out->type[strlen(out->type)] = '*';
		}
		return 0;
	}
	return 0;
}

int
cool_header(CoolState *st, char *line, size_t n)
{
	size_t i, nt, n_nodes, a0, a1, depth, t;
	CastToken *toks;
	CastAst *ast;
	char sig[COOL_SIG_MAX];
	const char *args;
	size_t args_len;
	int nsig;
	size_t end;

	i = 0;
	while (i < n && isspace((unsigned char)line[i]))
		i++;
	if (i + 4 > n || memcmp(line + i, "COOL", 4) != 0)
		return 0;
	if (i + 4 >= n || !isspace((unsigned char)line[i + 4]))
		return 0;

	cast_memory_clear(&st->mem);
	nt = 0;
	toks = cast_tokenize(&st->mem, line, n, &nt);
	if (!toks || nt < 6)
		return 0;
	if (!cool_tok_eq(toks[0], line, "IDENT", "COOL"))
		return 0;
	if (!cool_tok_eq(toks[1], line, "KW", "void"))
		return 0;
	if (strcmp(cast_token_type(toks[2].type), "IDENT") != 0)
		return 0;
	if (!cool_tok_eq(toks[3], line, "PUNCT", "("))
		return 0;

	depth = 1;
	t = 4;
	while (t < nt && depth) {
		if (cool_tok_eq(toks[t], line, "PUNCT", "("))
			depth++;
		else if (cool_tok_eq(toks[t], line, "PUNCT", ")"))
			depth--;
		if (depth)
			t++;
	}
	if (t >= nt || depth != 0)
		return 0;
	if (t + 1 >= nt || !cool_tok_eq(toks[t + 1], line, "PUNCT", "{"))
		return 0;
	end = (size_t)toks[t + 1].arg + (size_t)toks[t + 1].arglen;
	while (end < n && isspace((unsigned char)line[end]))
		end++;
	if (end != n)
		return 0;

	if ((size_t)toks[2].arglen >= sizeof st->func)
		cool_die("function name too long");
	memcpy(st->func, line + toks[2].arg, (size_t)toks[2].arglen);
	st->func[toks[2].arglen] = 0;

	a0 = (size_t)toks[3].arg + 1;
	a1 = (size_t)toks[t].arg;
	while (a0 < a1 && isspace((unsigned char)line[a0]))
		a0++;
	while (a1 > a0 && isspace((unsigned char)line[a1 - 1]))
		a1--;
	args = line + a0;
	args_len = a1 - a0;
	if (args_len == 0) {
		args = "void";
		args_len = 4;
	}

	cool_out_fmt(st, "void %s(%.*s) {\n", st->func, (int)args_len, args);

	nsig = snprintf(sig, sizeof sig, "void %s(%.*s);", st->func, (int)args_len, args);
	if (nsig < 0 || (size_t)nsig >= sizeof sig)
		cool_die("signature too long");

	nt = 0;
	toks = cast_tokenize(&st->mem, sig, (size_t)nsig, &nt);
	if (!toks)
		cool_die("failed to tokenize signature");
	n_nodes = 0;
	ast = cast_ast(&st->mem, toks, nt, &n_nodes);
	if (!ast)
		cool_die("failed to parse signature");
	cool_params_from_ast(st, ast, n_nodes);
	cast_memory_clear(&st->mem);
	st->in_func = 1;
	st->raw_len = 0;
	return 1;
}

int
cool_transpile(CoolState *st)
{
	size_t off;

	off = 0;
	st->line_num = 1;
	st->in_func = 0;
	while (off < st->src_len) {
		size_t start, n;

		start = off;
		while (off < st->src_len && st->src[off] != '\n')
			off++;
		if (off < st->src_len)
			off++;
		n = off - start;
		if (st->in_func) {
			if (cool_line_is_end(st->src + start, n)) {
				cool_raw_flush(st);
				cool_out_s(st, "}\n\n");
				st->in_func = 0;
				st->nparams = 0;
				st->func[0] = 0;
			} else if (cool_body_line(st, st->src + start, n) != 0) {
				return 1;
			}
		} else if (!cool_header(st, st->src + start, n)) {
			cool_out_n(st, st->src + start, n);
		}
		st->line_num++;
	}
	if (st->in_func) {
		fprintf(stderr, "\n[Transpiler Error] %s:%d: unclosed COOL function '%s'\n\n",
		    st->filename, st->line_num, st->func);
		return 1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	CoolState st;
	const char *input, *output;
	int i, rc;
	FILE *out;

	input = NULL;
	output = NULL;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
			if (i + 1 >= argc) {
				usage();
				return 1;
			}
			output = argv[++i];
		} else if (argv[i][0] == '-') {
			usage();
			return 1;
		} else if (input) {
			usage();
			return 1;
		} else {
			input = argv[i];
		}
	}
	if (!input) {
		usage();
		return 1;
	}

	memset(&st, 0, sizeof st);
	st.filename = input;
	st.src = cool_file_read(input, &st.src_len);
	if (!st.src) {
		fprintf(stderr, "cool_transpiler: cannot read %s: %s\n", input, strerror(errno));
		return 1;
	}
	st.mem = cast_memory_create(cool_arena, sizeof cool_arena);
	rc = cool_transpile(&st);
	if (rc == 0) {
		if (output) {
			out = fopen(output, "w");
			if (!out) {
				fprintf(stderr, "cool_transpiler: cannot write %s: %s\n", output, strerror(errno));
				rc = 1;
			}
		} else {
			out = stdout;
		}
		if (rc == 0 && st.out_len)
			fwrite(st.out, 1, st.out_len, out);
		if (output && out)
			fclose(out);
	}
	free(st.src);
	free(st.raw);
	free(st.out);
	return rc;
}
