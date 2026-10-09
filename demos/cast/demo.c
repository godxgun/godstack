/* Copyright (c) 2026 Vasco Alves. Repeated source indexing with a reused arena. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAST_IMPLEMENTATION
#include "Cast.h"

#define DEMO_PASSES 256
#define DEMO_MEMORY (1 << 20)

int
main(void)
{
	void *backing;
	CastMemory memory;
	char source[16384];
	size_t total_tokens = 0, total_nodes = 0, named_nodes = 0;
	uint64_t fingerprint = 0;
	int pass, result = 1;

	if (!(backing = malloc(DEMO_MEMORY))) {
		fprintf(stderr, "cast demo: cannot allocate backing\n");
		return 1;
	}
	memory = cast_memory_create(backing, DEMO_MEMORY);
	for (pass = 0; pass < DEMO_PASSES; pass++) {
		CastToken *tokens;
		CastAst *ast;
		size_t length, token_count = 0, node_count = 0, i;
		int written, function;

		written = snprintf(source, sizeof(source),
			"/* Generated drawing module %d. */\n"
			"#define CHANNELS 4\n"
			"typedef unsigned int Pixel;\n"
			"struct Tile { int width; int height; };\n", pass);
		if (written < 0 || (size_t)written >= sizeof(source))
			goto source_error;
		length = (size_t)written;
		for (function = 0; function < 16 + pass % 32; function++) {
			written = snprintf(source + length, sizeof(source) - length,
				"static int shade_%d(int value, int bias) {\n"
				"  int sum = value + bias; return sum + %d;\n}\n"
				"void paint_%d(char *pixels, int count);\n",
				function, pass + function, function);
			if (written < 0 || (size_t)written >= sizeof(source) - length)
				goto source_error;
			length += (size_t)written;
		}

		tokens = cast_tokenize(&memory, source, length, &token_count);
		if (!tokens || !token_count) {
			fprintf(stderr, "cast demo: tokenization failed on pass %d\n", pass);
			goto done;
		}
		for (i = 0; i < token_count; i++) {
			CastToken token = tokens[i];
			if (token.arg < 0 || token.arglen < 0 || (size_t)token.arg > length ||
				(size_t)token.arglen > length - (size_t)token.arg) {
				fprintf(stderr, "cast demo: token outside source on pass %d\n", pass);
				goto done;
			}
			fingerprint += strlen(cast_token_type(token.type)) + (size_t)token.arglen;
		}
		ast = cast_ast(&memory, tokens, token_count, &node_count);
		if (!ast || !node_count) {
			fprintf(stderr, "cast demo: parsing failed on pass %d\n", pass);
			goto done;
		}
		/* Walk the public node index while the source and arena remain alive. */
		for (i = 0; i < node_count; i++) {
			CastNode node = cast_ast_node(ast, i);
			fingerprint += node.kind + node.line;
			if (node.msg)
				fingerprint += strlen(node.msg);
			if (node.name) {
				named_nodes++;
				fingerprint += strlen(node.name);
			}
		}
		total_tokens += token_count;
		total_nodes += node_count;
		cast_memory_clear(&memory);
	}
	printf("cast demo: %d passes, %zu tokens, %zu nodes (%zu named), fingerprint %llu\n",
		DEMO_PASSES, total_tokens, total_nodes, named_nodes, (unsigned long long)fingerprint);
	result = 0;
	goto done;

source_error:
	fprintf(stderr, "cast demo: generated source exceeded capacity\n");
done:
	free(backing);
	return result;
}
