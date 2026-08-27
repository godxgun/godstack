#include <stdio.h>
#include <string.h>

#include "cast.h"
#include "cast.c"

static int g_fails;

static void expect(int ok, const char *what);
static int tok_is(CastToken t, const char *type, const char *src, const char *lex);
static int node_named(CastAst *ast, size_t n, const char *name);
static void test_sample(void);
static void test_empty(void);
static void test_comments_pp(void);
static void test_literals(void);
static void test_reuse(void);
static void test_tiny(void);

void
expect(int ok, const char *what)
{
	if (ok) {
		printf("  ok   %s\n", what);
		return;
	}
	printf("  FAIL %s\n", what);
	g_fails++;
}

int
tok_is(CastToken t, const char *type, const char *src, const char *lex)
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
node_named(CastAst *ast, size_t n, const char *name)
{
	size_t i;

	for (i = 0; i < n; i++) {
		CastNode d;

		d = cast_ast_node(ast, i);
		if (d.name && strcmp(d.name, name) == 0)
			return 1;
	}
	return 0;
}

void
test_sample(void)
{
	unsigned char buf[1 << 16];
	char str[] = "void my_func(char *arg1, int arg2);";
	CastMemory mem;
	CastToken *tokens;
	CastAst *ast;
	CastNode d;
	size_t n, nodes;

	printf("sample\n");
	mem = cast_memory_create(buf, sizeof buf);
	n = 0;
	tokens = cast_tokenize(&mem, str, strlen(str), &n);
	expect(tokens != NULL, "tokenize");
	expect(n == 11, "11 tokens");
	if (!tokens || n != 11)
		return;
	expect(tok_is(tokens[0], "KW", str, "void"), "void");
	expect(tok_is(tokens[1], "IDENT", str, "my_func"), "my_func");
	expect(tok_is(tokens[2], "PUNCT", str, "("), "(");
	expect(tok_is(tokens[3], "KW", str, "char"), "char");
	expect(tok_is(tokens[4], "PUNCT", str, "*"), "*");
	expect(tok_is(tokens[5], "IDENT", str, "arg1"), "arg1");
	expect(tok_is(tokens[6], "PUNCT", str, ","), ",");
	expect(tok_is(tokens[7], "KW", str, "int"), "int");
	expect(tok_is(tokens[8], "IDENT", str, "arg2"), "arg2");
	expect(tok_is(tokens[9], "PUNCT", str, ")"), ")");
	expect(tok_is(tokens[10], "PUNCT", str, ";"), ";");

	nodes = 0;
	ast = cast_ast(&mem, tokens, n, &nodes);
	expect(ast != NULL, "ast");
	expect(nodes > 0, "nodes");
	if (!ast)
		return;
	d = cast_ast_node(ast, 0);
	expect(d.msg && strcmp(d.msg, "tu") == 0, "root tu");
	expect(d.line == 1, "tu line");
	expect(node_named(ast, nodes, "my_func"), "node my_func");
	expect(node_named(ast, nodes, "arg1"), "node arg1");
	expect(node_named(ast, nodes, "arg2"), "node arg2");
}

void
test_empty(void)
{
	unsigned char buf[1 << 12];
	CastMemory mem;
	size_t n;
	CastToken *tokens;
	CastAst *ast;
	size_t nodes;
	char empty[] = "";
	char ws[] = "  \n\t  ";

	printf("empty\n");
	mem = cast_memory_create(buf, sizeof buf);
	n = 99;
	tokens = cast_tokenize(&mem, empty, 0, &n);
	expect(tokens == NULL, "empty tokens");
	expect(n == 0, "empty n");
	nodes = 99;
	ast = cast_ast(&mem, tokens, n, &nodes);
	expect(ast != NULL, "empty ast");
	expect(nodes == 1, "empty tu");

	n = 99;
	tokens = cast_tokenize(&mem, ws, strlen(ws), &n);
	expect(n == 0, "ws n");
	expect(tokens == NULL, "ws tokens");
}

void
test_comments_pp(void)
{
	unsigned char buf[1 << 12];
	CastMemory mem;
	char str[] = "#include <stdio.h>\nint /* c */ x; // z\n";
	CastToken *tokens;
	size_t n;

	printf("comments_pp\n");
	mem = cast_memory_create(buf, sizeof buf);
	n = 0;
	tokens = cast_tokenize(&mem, str, strlen(str), &n);
	expect(tokens != NULL, "tokenize");
	expect(n == 3, "int x ;");
	if (!tokens || n != 3)
		return;
	expect(tok_is(tokens[0], "KW", str, "int"), "int");
	expect(tok_is(tokens[1], "IDENT", str, "x"), "x");
	expect(tok_is(tokens[2], "PUNCT", str, ";"), ";");
}

void
test_literals(void)
{
	unsigned char buf[1 << 12];
	CastMemory mem;
	char str[] = "a = 1.0e+2 + 0xFF + \"hi\" + 'c';";
	CastToken *tokens;
	size_t n;

	printf("literals\n");
	mem = cast_memory_create(buf, sizeof buf);
	n = 0;
	tokens = cast_tokenize(&mem, str, strlen(str), &n);
	expect(tokens != NULL && n == 10, "10 tokens");
	if (!tokens || n != 10)
		return;
	expect(tok_is(tokens[0], "IDENT", str, "a"), "a");
	expect(tok_is(tokens[1], "PUNCT", str, "="), "=");
	expect(tok_is(tokens[2], "NUMBER", str, "1.0e+2"), "1.0e+2");
	expect(tok_is(tokens[3], "PUNCT", str, "+"), "+");
	expect(tok_is(tokens[4], "NUMBER", str, "0xFF"), "0xFF");
	expect(tok_is(tokens[5], "PUNCT", str, "+"), "+2");
	expect(tok_is(tokens[6], "STRING", str, "\"hi\""), "string");
	expect(tok_is(tokens[7], "PUNCT", str, "+"), "+3");
	expect(tok_is(tokens[8], "CHAR", str, "'c'"), "char");
	expect(tok_is(tokens[9], "PUNCT", str, ";"), ";");
}

void
test_reuse(void)
{
	unsigned char buf[1 << 12];
	CastMemory mem;
	char a[] = "int x;";
	char b[] = "char y;";
	CastToken *tokens;
	size_t n;

	printf("reuse\n");
	mem = cast_memory_create(buf, sizeof buf);
	tokens = cast_tokenize(&mem, a, strlen(a), &n);
	expect(tokens != NULL && n == 3, "first");
	cast_memory_clear(&mem);
	n = 0;
	tokens = cast_tokenize(&mem, b, strlen(b), &n);
	expect(tokens != NULL && n == 3, "second");
	if (!tokens || n != 3)
		return;
	expect(tok_is(tokens[0], "KW", b, "char"), "char after clear");
}

void
test_tiny(void)
{
	unsigned char tiny[8];
	CastMemory mem;
	char str[] = "void my_func(char *arg1, int arg2);";
	size_t n;
	CastToken *tokens;

	printf("tiny\n");
	mem = cast_memory_create(tiny, sizeof tiny);
	n = 99;
	tokens = cast_tokenize(&mem, str, strlen(str), &n);
	expect(tokens == NULL, "tiny tokenize");
}

int
main(void)
{
	test_sample();
	test_empty();
	test_comments_pp();
	test_literals();
	test_reuse();
	test_tiny();
	if (g_fails) {
		printf("%d failed\n", g_fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}
