+++
title = 'Cast symbols'
description = 'Types, macros, and functions declared by Cast.h.'
weight = 70
[params]
  id = 'symbols/cast'
  product = 'Cast'
  category = 'Symbols'
  tags = ['API', 'symbols', 'C']
  access = 'free'
  symbols = true
  audience = 'Core'
+++

# Cast symbols

For setup and API workflows, read the [Cast guide](/docs/cast/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Cast.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### CastMemory {#castmemory}

Source: `Cast.h:83`

```c
typedef struct CastMemory {
	unsigned char *buf;
	size_t cap;
	size_t len;
	char *src;
	size_t src_len;
} CastMemory;
```

User provided memory.

#### CastToken {#casttoken}

Source: `Cast.h:91`

```c
typedef struct CastToken {
	uint32_t type;
	int arg;
	int arglen;
} CastToken;
```

A single token.

#### CastAst {#castast}

Source: `Cast.h:97`

```c
typedef struct CastAst CastAst;
```

A parsed AST tree.

#### CastNode {#castnode}

Source: `Cast.h:99`

```c
typedef struct CastNode {
	const char *name;
	uint32_t line;
	const char *msg;
	uint32_t kind;
	int arg;
	int arglen;
	size_t first_child;
	size_t next_sibling;
} CastNode;
```

An AST tree node.

### Macros

#### CAST_MIN {#cast_min}

Source: `Cast.h:80`

```c
#define CAST_MIN(a, b) ((a) < (b) ? (a) : (b))
```

#### CAST_MAX {#cast_max}

Source: `Cast.h:81`

```c
#define CAST_MAX(a, b) ((a) > (b) ? (a) : (b))
```

### Functions

#### cast_memory_create {#cast_memory_create}

Source: `Cast.h:110`

```c
extern CastMemory cast_memory_create(void *buf, size_t size);
```

Creates a memory arena. Use as thought will.

#### cast_tokenize {#cast_tokenize}

Source: `Cast.h:111`

```c
extern CastToken *cast_tokenize(CastMemory *mem, char *str, size_t len, size_t *out_len);
```

Tokenizes the output and returns an array plus the number of tokens.

#### cast_token_type {#cast_token_type}

Source: `Cast.h:112`

```c
extern char* cast_token_type(uint32_t type);
```

Returns cstring from a type.

#### cast_ast {#cast_ast}

Source: `Cast.h:113`

```c
extern CastAst *cast_ast(CastMemory *mem, CastToken *tokens, size_t n_tokens, size_t *out_nodes);
```

#### cast_ast_node {#cast_ast_node}

Source: `Cast.h:114`

```c
extern CastNode cast_ast_node(CastAst *ast, size_t i);
```

Gets a node from an index.

#### cast_memory_clear {#cast_memory_clear}

Source: `Cast.h:115`

```c
extern void cast_memory_clear(CastMemory *mem);
```

Does not free. Clears the Arena so you can reuse the memory.

<!-- END GENERATED API -->
