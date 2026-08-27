#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

/*
 * Generate API docs from header `//` comments.
 *
 *     doc_generator FILE          HTML cards on stdout
 *     doc_generator [--serve]     host docs at http://127.0.0.1:8080
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *cool_out;
#define COOL_OUTPUT cool_out

#include "cool.h"
#include "cool.c"

#include "view.cool.c"

#include "wire.h"
#include "wire.c"

#define DOCS_PORT 8080
#define DOCS_MAX 32

typedef struct DocsLib {
	char name[64];
	char path[256];
} DocsLib;

typedef struct Docs {
	DocsLib lib[DOCS_MAX];
	int n;
} Docs;

static const char *const docs_default[] = {
	"Cast/cast.h",
	"Cool/cool.h",
	"Fuse/fuse.h",
	"Grit/grit.h",
	"Peak/peak.h",
	"Poof/poof.h",
	"Rend/rend.h",
	"Term/term.h",
	"Wire/wire.h",
};

static int parse_header_line(const char *line, char *func_name, char *func_decl, char **comment_out);
static void docs_name_from_path(const char *path, char *name, size_t cap);
static int docs_add(Docs *docs, const char *path);
static void docs_load(Docs *docs, int argc, char **argv);
static void docs_write_cards(const char *path);
static void docs_page_begin(Docs *docs, const char *title);
static void docs_page_end(void);
static int docs_send(int fd, void (*write_page)(Docs *, int), Docs *docs, int idx);
static void docs_write_index(Docs *docs, int idx);
static void docs_write_lib(Docs *docs, int idx);
static int docs_on_req(int fd, const WireHttpRequest *req, void *user);
static int docs_serve(Docs *docs);
static int docs_stdout(const char *path);

int
parse_header_line(const char *line, char *func_name, char *func_decl, char **comment_out)
{
	char *comment_ptr;
	char *open_paren;
	char *end_name;
	char *start_name;
	size_t decl_len;
	size_t name_len;

	comment_ptr = strstr(line, "//");
	if (!comment_ptr)
		return 0;

	*comment_out = comment_ptr + 2;
	while (**comment_out == ' ' || **comment_out == '\t')
		(*comment_out)++;

	decl_len = (size_t)(comment_ptr - line);
	if (decl_len >= 256)
		decl_len = 255;
	strncpy(func_decl, line, decl_len);
	func_decl[decl_len] = '\0';

	while (decl_len > 0 && (isspace((unsigned char)func_decl[decl_len - 1]) ||
	    func_decl[decl_len - 1] == ';')) {
		func_decl[--decl_len] = '\0';
	}

	open_paren = strchr(func_decl, '(');
	if (!open_paren)
		return 0;

	end_name = open_paren - 1;
	while (end_name > func_decl && isspace((unsigned char)*end_name))
		end_name--;

	start_name = end_name;
	while (start_name > func_decl &&
	    (isalnum((unsigned char)start_name[-1]) || start_name[-1] == '_'))
		start_name--;

	name_len = (size_t)(end_name - start_name) + 1;
	if (name_len >= 256)
		name_len = 255;
	strncpy(func_name, start_name, name_len);
	func_name[name_len] = '\0';

	return 1;
}

void
docs_name_from_path(const char *path, char *name, size_t cap)
{
	const char *slash;
	const char *start;
	size_t n;

	slash = strrchr(path, '/');
	if (slash && slash != path) {
		start = slash;
		while (start > path && start[-1] != '/')
			start--;
		n = (size_t)(slash - start);
	} else {
		start = path;
		n = strlen(path);
		if (n > 2 && strcmp(start + n - 2, ".h") == 0)
			n -= 2;
	}
	if (n >= cap)
		n = cap - 1;
	memcpy(name, start, n);
	name[n] = 0;
}

int
docs_add(Docs *docs, const char *path)
{
	FILE *f;
	DocsLib *lib;

	if (docs->n >= DOCS_MAX)
		return 0;
	f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "doc_generator: skip %s: %s\n", path, strerror(errno));
		return 0;
	}
	fclose(f);
	lib = &docs->lib[docs->n];
	docs_name_from_path(path, lib->name, sizeof lib->name);
	strncpy(lib->path, path, sizeof lib->path - 1);
	lib->path[sizeof lib->path - 1] = 0;
	docs->n++;
	return 1;
}

void
docs_load(Docs *docs, int argc, char **argv)
{
	int i;

	docs->n = 0;
	if (argc > 0) {
		for (i = 0; i < argc; i++)
			docs_add(docs, argv[i]);
		return;
	}
	for (i = 0; i < (int)(sizeof docs_default / sizeof docs_default[0]); i++)
		docs_add(docs, docs_default[i]);
}

void
docs_write_cards(const char *path)
{
	FILE *header_file;
	char *line_buf;
	size_t len;

	header_file = fopen(path, "r");
	if (!header_file)
		return;
	line_buf = malloc(256);
	len = 0;
	while (getline(&line_buf, &len, header_file) > 0) {
		char func_name[256];
		char func_decl[256];
		char *comment_text = NULL;

		if (parse_header_line(line_buf, func_name, func_decl, &comment_text))
			Api(func_name, func_decl, comment_text);
	}
	free(line_buf);
	fclose(header_file);
}

void
docs_page_begin(Docs *docs, const char *title)
{
	int i;

	cool_html_raw(COOL_SV(
		"<!doctype html><html lang=\"en\"><head>"
		"<meta charset=\"utf-8\">"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
		"<title>"));
	cool_html_txt(title, strlen(title));
	cool_html_raw(COOL_SV(
		"</title><style>"
		"body{font-family:sans-serif;max-width:52rem;margin:2rem auto;padding:0 1rem;"
		"background:#111;color:#ddd}"
		"a{color:#9cf}"
		"header a.home{color:#fff;text-decoration:none;font-weight:700}"
		"nav{display:flex;flex-wrap:wrap;gap:.4rem 1rem;margin:1rem 0 2rem}"
		"h1{font-size:1.4rem}"
		".api-card{border:1px solid #333;border-radius:8px;padding:1rem;margin:1rem 0;"
		"background:#1a1a1a}"
		".api-name{margin:0 0 .5rem;font-family:ui-monospace,monospace}"
		"code{display:block;overflow-x:auto;background:#000;padding:.75rem;border-radius:4px;"
		"font-size:.9rem}"
		".api-description{color:#aaa;margin-top:.75rem;white-space:pre-wrap}"
		"</style></head><body><header><a class=\"home\" href=\"/\">godstack</a>"
		"<h1>"));
	cool_html_txt(title, strlen(title));
	cool_html_raw(COOL_SV("</h1><nav>"));
	for (i = 0; i < docs->n; i++)
		LibLink(docs->lib[i].name);
	cool_html_raw(COOL_SV("</nav></header><main>"));
}

void
docs_page_end(void)
{
	cool_html_raw(COOL_SV("</main></body></html>\n"));
}

int
docs_send(int fd, void (*write_page)(Docs *, int), Docs *docs, int idx)
{
	char *body;
	size_t len;

	body = NULL;
	len = 0;
	cool_out = open_memstream(&body, &len);
	if (!cool_out)
		return wire_http_error(fd, 500);
	write_page(docs, idx);
	fclose(cool_out);
	cool_out = NULL;
	wire_http_write_status(fd, 200, "text/html; charset=utf-8", body, len);
	free(body);
	return 200;
}

void
docs_write_index(Docs *docs, int idx)
{
	(void)idx;
	docs_page_begin(docs, "docs");
	if (docs->n == 0)
		cool_html_raw(COOL_SV("<p>No headers found. Run from the godstack root.</p>"));
	docs_page_end();
}

void
docs_write_lib(Docs *docs, int idx)
{
	docs_page_begin(docs, docs->lib[idx].name);
	cool_html_raw(COOL_SV("<p><code>"));
	cool_html_txt(docs->lib[idx].path, strlen(docs->lib[idx].path));
	cool_html_raw(COOL_SV("</code></p>"));
	docs_write_cards(docs->lib[idx].path);
	docs_page_end();
}

int
docs_on_req(int fd, const WireHttpRequest *req, void *user)
{
	Docs *docs;
	int i;
	char url[80];

	docs = user;
	if (strcmp(req->path, "/") == 0)
		return docs_send(fd, docs_write_index, docs, -1);
	for (i = 0; i < docs->n; i++) {
		snprintf(url, sizeof url, "/%s", docs->lib[i].name);
		if (strcmp(req->path, url) == 0)
			return docs_send(fd, docs_write_lib, docs, i);
	}
	return wire_http_error(fd, 404);
}

int
docs_serve(Docs *docs)
{
	printf("listening on http://127.0.0.1:%d\n", DOCS_PORT);
	fflush(stdout);
	if (wire_http_serve(DOCS_PORT, docs_on_req, docs) < 0) {
		fprintf(stderr, "doc_generator: listen %d: %s\n", DOCS_PORT, strerror(errno));
		return 1;
	}
	return 0;
}

int
docs_stdout(const char *path)
{
	FILE *header_file;

	header_file = fopen(path, "r");
	if (!header_file) {
		fprintf(stderr, "doc_generator: %s: %s\n", path, strerror(errno));
		return 1;
	}
	fclose(header_file);
	cool_out = stdout;
	docs_write_cards(path);
	return 0;
}

int
main(int argc, char **argv)
{
	Docs docs;
	int serve;
	int argi;

	serve = 0;
	argi = 1;
	if (argc < 2) {
		serve = 1;
	} else if (strcmp(argv[1], "--serve") == 0) {
		serve = 1;
		argi = 2;
	} else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		puts("Usage: doc_generator FILE");
		puts("       doc_generator [--serve] [FILE...]");
		return 0;
	}

	if (!serve)
		return docs_stdout(argv[1]);

	docs_load(&docs, argc - argi, argv + argi);
	return docs_serve(&docs);
}
