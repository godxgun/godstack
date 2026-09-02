#define _POSIX_C_SOURCE 200809L

#include "../Peak/peak.h"
#include "../Rend/rend.h"
#include "../Cast/cast.h"
#include "../Cast/cast.c"
#include "../Peak/peak.c"
#include "../Rend/rend.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FUNCS 2048
#define MAX_CALLS 48
#define MAX_NAME 64
#define MAX_FILES 256
#define MAX_PATH 512
#define NODE_W 168.0f
#define NODE_H 40.0f
#define GAP_X 40.0f
#define GAP_Y 96.0f
#define CTX_MAX 64
#define B_IF     (1u << 0)
#define B_ELSE   (1u << 1)
#define B_WHILE  (1u << 2)
#define B_FOR    (1u << 3)
#define B_DO     (1u << 4)
#define B_SWITCH (1u << 5)
#define B_CASE   (1u << 6)
#define B_RETURN (1u << 7)
#define FONT_W 8
#define FONT_H 8

#define COL_BG     0xFF11151Cu
#define COL_HUD    0xFF1C2330u
#define COL_NODE   0xFF2D3644u
#define COL_ROOT   0xFF1D4ED8u
#define COL_SEL    0xFF3B82F6u
#define COL_EDGE   0xFF64748Bu
#define COL_EDGE_H 0xFFF59E0Bu
#define COL_TEXT   0xFFF8FAFCu
#define COL_DIM    0xFF94A3B8u
#define COL_BORDER 0xFF0F172Au

static const uint8_t font8[95][8] = {
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
	{0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
	{0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
	{0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
	{0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
	{0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
	{0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
	{0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
	{0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
	{0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
	{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
	{0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
	{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
	{0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
	{0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
	{0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
	{0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
	{0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
	{0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
	{0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
	{0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
	{0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
	{0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
	{0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
	{0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
	{0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
	{0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
	{0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
	{0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
	{0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
	{0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
	{0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
	{0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
	{0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
	{0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
	{0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
	{0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
	{0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
	{0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
	{0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
	{0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
	{0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
	{0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
	{0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
	{0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
	{0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
	{0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
	{0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
	{0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
	{0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
	{0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
	{0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
	{0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
	{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
	{0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
	{0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
	{0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
	{0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
	{0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
	{0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
	{0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
	{0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
	{0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
	{0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
	{0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
	{0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00},
	{0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00},
	{0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00},
	{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
	{0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
	{0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
	{0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
	{0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
	{0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
	{0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
	{0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
	{0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
	{0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
	{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
	{0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
	{0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
	{0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
	{0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
	{0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
	{0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
	{0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
	{0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
	{0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
	{0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
	{0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
	{0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
	{0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
};

static const char sample_src[] =
	"int leaf(int x) { return x + 1; }\n"
	"int mid(int x) { return leaf(x) + leaf(x); }\n"
	"void log_val(int x) { (void)x; }\n"
	"int work(int n) { log_val(n); return mid(n); }\n"
	"int main(void) { if (work(2)) return mid(3); while (leaf(0)) log_val(1); return 0; }\n";

enum {
	CK_NONE = 0,
	CK_IF,
	CK_ELSE,
	CK_WHILE,
	CK_FOR,
	CK_DO,
	CK_SWITCH,
	CK_CASE,
	CK_RETURN
};

typedef struct {
	int kind;
	int brace;
	int wait;
	int cond_p;
} CtxFrame;

typedef struct {
	int to;
	unsigned ctx;
} Call;

typedef struct {
	char name[MAX_NAME];
	char file[MAX_NAME];
	int ncalls;
	Call calls[MAX_CALLS];
	int defined;
	int is_root;
	int in_tree;
	int depth;
	int slot;
	size_t body_lo, body_hi;
	float x, y, w, h;
} Func;

typedef struct {
	char paths[MAX_FILES][MAX_PATH];
	int n;
	char dir[MAX_PATH];
} FileList;

typedef struct {
	Func funcs[MAX_FUNCS];
	int nfuncs;
	int nroots;
	int root;
	int selected;
	int ntree;
	int max_depth;
	float cam_x, cam_y, zoom;
	int fitted;
	uint32_t *pix;
	uint32_t pw, ph;
	RendTexture canvas;
	int have_tex;
	int dragging;
	int drag_node;
	float drag_mx, drag_my;
	float pan_x, pan_y;
	char files[MAX_FILES][MAX_PATH];
	int nfiles;
	int file_lo[MAX_FILES];
	int file_hi[MAX_FILES];
} App;

static int tok_type(CastToken t, const char *ty);
static int tok_lex(CastToken t, const char *src, const char *lex);
static int tok_kw(CastToken t, const char *src, const char *kw);
static size_t skip_balanced(CastToken *toks, size_t n, size_t i, const char *src, const char *open, const char *close);
static void copy_name(char *dst, size_t cap, const char *src, CastToken t);
static void path_base(char *dst, size_t cap, const char *path);
static unsigned ctx_bit(int kind);
static void ctx_label(unsigned bits, char *dst, size_t cap);
static int name_dup(App *app, const char *name);
static int add_func_def(App *app, const char *file, const char *name);
static int resolve_callee(App *app, const char *file, const char *name);
static void add_call(App *app, int from, int to, unsigned ctx);
static void ctx_push(CtxFrame *st, int *sp, int kind, int wait);
static unsigned ctx_now(CtxFrame *st, int sp);
static void scan_calls(App *app, int fid, CastToken *toks, size_t n, const char *src, size_t lo, size_t hi);
static void scan_tokens(App *app, CastToken *toks, size_t n, const char *src, const char *file, int calls);
static void parse_source(App *app, char *src, size_t len, const char *file, int calls);
static void parse_file(App *app, const char *path, int calls);
static int collect_c(const char *name, void *ud);
static void ingest_path(App *app, const char *path);
static void bfs_walk(App *app, int *q, int *head, int *tail, int *seen);
static void layout_graph(App *app);
static void fit_camera(App *app, float W, float H);
static void world_to_screen(App *app, float wx, float wy, float W, float H, float *sx, float *sy);
static void screen_to_world(App *app, float sx, float sy, float W, float H, float *wx, float *wy);
static int hit_node(App *app, float wx, float wy);
static uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
static void unpack_rgba(uint32_t c, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void pix_put(App *app, int x, int y, uint32_t c);
static void fill_rect(App *app, int x, int y, int w, int h, uint32_t c);
static void fill_rect_border(App *app, int x, int y, int w, int h, uint32_t fill, uint32_t border);
static void draw_line(App *app, int x0, int y0, int x1, int y1, uint32_t c);
static void draw_ortho(App *app, float x0, float y0, float x1, float y1, uint32_t c);
static void draw_char(App *app, int x, int y, char ch, uint32_t c, int scale);
static void draw_text(App *app, int x, int y, const char *s, uint32_t c, int scale);
static int ensure_canvas(App *app, RendRenderer renderer, uint32_t w, uint32_t h);
static void draw_graph(App *app, float W, float H, const char *status);

int
tok_type(CastToken t, const char *ty)
{
	return strcmp(cast_token_type(t.type), ty) == 0;
}

int
tok_lex(CastToken t, const char *src, const char *lex)
{
	size_t n;

	n = strlen(lex);
	if ((size_t)t.arglen != n)
		return 0;
	return memcmp(src + t.arg, lex, n) == 0;
}

int
tok_kw(CastToken t, const char *src, const char *kw)
{
	return tok_type(t, "KW") && tok_lex(t, src, kw);
}

size_t
skip_balanced(CastToken *toks, size_t n, size_t i, const char *src, const char *open, const char *close)
{
	int d;

	if (i >= n || !tok_type(toks[i], "PUNCT") || !tok_lex(toks[i], src, open))
		return i;
	d = 1;
	i++;
	while (i < n && d) {
		if (tok_type(toks[i], "PUNCT")) {
			if (tok_lex(toks[i], src, open))
				d++;
			else if (tok_lex(toks[i], src, close))
				d--;
		}
		i++;
	}
	return i;
}

void
copy_name(char *dst, size_t cap, const char *src, CastToken t)
{
	size_t n;

	if (cap == 0)
		return;
	n = (size_t)t.arglen;
	if (n >= cap)
		n = cap - 1;
	memcpy(dst, src + t.arg, n);
	dst[n] = 0;
}

void
path_base(char *dst, size_t cap, const char *path)
{
	const char *s, *p;

	s = path;
	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\')
			s = p + 1;
	}
	snprintf(dst, cap, "%s", s);
}

unsigned
ctx_bit(int kind)
{
	switch (kind) {
	case CK_IF: return B_IF;
	case CK_ELSE: return B_ELSE;
	case CK_WHILE: return B_WHILE;
	case CK_FOR: return B_FOR;
	case CK_DO: return B_DO;
	case CK_SWITCH: return B_SWITCH;
	case CK_CASE: return B_CASE;
	case CK_RETURN: return B_RETURN;
	default: return 0;
	}
}

void
ctx_label(unsigned bits, char *dst, size_t cap)
{
	static const unsigned bit[] = { B_IF, B_ELSE, B_WHILE, B_FOR, B_DO, B_SWITCH, B_CASE, B_RETURN };
	static const char *name[] = { "if", "else", "while", "for", "do", "switch", "case", "return" };
	size_t i, n;
	int first;

	n = 0;
	first = 1;
	dst[0] = 0;
	for (i = 0; i < 8; i++) {
		if (!(bits & bit[i]))
			continue;
		n += (size_t)snprintf(dst + n, cap > n ? cap - n : 0, "%s%s", first ? "" : ",", name[i]);
		first = 0;
		if (n >= cap)
			break;
	}
}

int
name_dup(App *app, const char *name)
{
	int i, n;

	n = 0;
	for (i = 0; i < app->nfuncs; i++) {
		if (app->funcs[i].defined && strcmp(app->funcs[i].name, name) == 0)
			n++;
	}
	return n > 1;
}

int
add_func_def(App *app, const char *file, const char *name)
{
	int i;

	if (app->nfuncs >= MAX_FUNCS)
		return -1;
	i = app->nfuncs++;
	memset(&app->funcs[i], 0, sizeof app->funcs[i]);
	snprintf(app->funcs[i].name, MAX_NAME, "%s", name);
	snprintf(app->funcs[i].file, MAX_NAME, "%s", file);
	app->funcs[i].defined = 1;
	app->funcs[i].w = NODE_W;
	app->funcs[i].h = NODE_H;
	return i;
}

int
resolve_callee(App *app, const char *file, const char *name)
{
	int i, same, other, nsame, nother;

	same = other = -1;
	nsame = nother = 0;
	for (i = 0; i < app->nfuncs; i++) {
		if (!app->funcs[i].defined)
			continue;
		if (strcmp(app->funcs[i].name, name) != 0)
			continue;
		if (strcmp(app->funcs[i].file, file) == 0) {
			same = i;
			nsame++;
		} else {
			other = i;
			nother++;
		}
	}
	if (nsame == 1)
		return same;
	if (nsame > 1)
		return same;
	if (strcmp(name, "main") == 0)
		return -1;
	if (nother == 1)
		return other;
	return -1;
}

void
add_call(App *app, int from, int to, unsigned ctx)
{
	int i;

	if (from < 0 || to < 0)
		return;
	for (i = 0; i < app->funcs[from].ncalls; i++) {
		if (app->funcs[from].calls[i].to == to) {
			app->funcs[from].calls[i].ctx |= ctx;
			return;
		}
	}
	if (app->funcs[from].ncalls >= MAX_CALLS)
		return;
	i = app->funcs[from].ncalls++;
	app->funcs[from].calls[i].to = to;
	app->funcs[from].calls[i].ctx = ctx;
}

void
ctx_push(CtxFrame *st, int *sp, int kind, int wait)
{
	if (*sp >= CTX_MAX)
		return;
	st[*sp].kind = kind;
	st[*sp].brace = 0;
	st[*sp].wait = wait;
	st[*sp].cond_p = 0;
	(*sp)++;
}

unsigned
ctx_now(CtxFrame *st, int sp)
{
	if (sp <= 0)
		return 0;
	return ctx_bit(st[sp - 1].kind);
}

void
scan_calls(App *app, int fid, CastToken *toks, size_t n, const char *src, size_t lo, size_t hi)
{
	CtxFrame st[CTX_MAX];
	int sp, brace, paren;
	size_t i;
	char name[MAX_NAME];
	int to;
	const char *file;

	sp = 0;
	brace = 0;
	paren = 0;
	file = app->funcs[fid].file;
	i = lo;
	while (i < hi && i < n) {
		if (i + 1 < hi && tok_type(toks[i], "IDENT") && tok_type(toks[i + 1], "PUNCT") && tok_lex(toks[i + 1], src, "(")) {
			copy_name(name, sizeof name, src, toks[i]);
			to = resolve_callee(app, file, name);
			if (to >= 0)
				add_call(app, fid, to, ctx_now(st, sp));
			i++;
			continue;
		}
		if (tok_kw(toks[i], src, "if"))
			ctx_push(st, &sp, CK_IF, 0);
		else if (tok_kw(toks[i], src, "while")) {
			if (sp > 0 && st[sp - 1].kind == CK_DO && st[sp - 1].wait >= 2)
				st[sp - 1].wait = 0;
			else
				ctx_push(st, &sp, CK_WHILE, 0);
		} else if (tok_kw(toks[i], src, "for"))
			ctx_push(st, &sp, CK_FOR, 0);
		else if (tok_kw(toks[i], src, "switch"))
			ctx_push(st, &sp, CK_SWITCH, 0);
		else if (tok_kw(toks[i], src, "do"))
			ctx_push(st, &sp, CK_DO, 2);
		else if (tok_kw(toks[i], src, "else"))
			ctx_push(st, &sp, CK_ELSE, 2);
		else if (tok_kw(toks[i], src, "return"))
			ctx_push(st, &sp, CK_RETURN, 2);
		else if (tok_kw(toks[i], src, "case") || tok_kw(toks[i], src, "default")) {
			while (sp > 0 && st[sp - 1].kind == CK_CASE && st[sp - 1].brace == brace)
				sp--;
			ctx_push(st, &sp, CK_CASE, 2);
			if (sp > 0)
				st[sp - 1].brace = brace;
		} else if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, "(")) {
			paren++;
			if (sp > 0 && st[sp - 1].wait == 0) {
				st[sp - 1].wait = 1;
				st[sp - 1].cond_p = paren;
			}
		} else if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, ")")) {
			if (sp > 0 && st[sp - 1].wait == 1 && paren == st[sp - 1].cond_p)
				st[sp - 1].wait = 2;
			paren--;
			if (paren < 0)
				paren = 0;
		} else if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, "{")) {
			brace++;
			if (sp > 0 && st[sp - 1].wait == 2 && st[sp - 1].brace == 0)
				st[sp - 1].brace = brace;
		} else if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, "}")) {
			if (sp > 0 && st[sp - 1].kind == CK_DO && st[sp - 1].brace == brace)
				st[sp - 1].wait = 3;
			else {
				while (sp > 0 && st[sp - 1].brace == brace && st[sp - 1].kind != CK_DO)
					sp--;
			}
			brace--;
			if (brace < 0)
				brace = 0;
		} else if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, ";")) {
			while (sp > 0) {
				CtxFrame *f;

				f = &st[sp - 1];
				if (f->wait == 1)
					break;
				if (f->kind == CK_DO && f->wait == 2 && f->brace == 0) {
					f->wait = 3;
					break;
				}
				if (f->kind == CK_DO && f->wait == 2 && f->brace != 0)
					break;
				if (f->wait == 2 && f->brace == 0) {
					sp--;
					continue;
				}
				if (f->kind == CK_DO && f->wait >= 2) {
					sp--;
					continue;
				}
				break;
			}
		}
		i++;
	}
}

void
scan_tokens(App *app, CastToken *toks, size_t n, const char *src, const char *file, int calls)
{
	size_t i, after, body;
	int last, fid, slot, k;
	char name[MAX_NAME];
	char base[MAX_NAME];

	path_base(base, sizeof base, file);
	slot = -1;
	fid = 0;
	if (calls) {
		for (k = 0; k < app->nfiles; k++) {
			if (strcmp(app->files[k], file) == 0) {
				slot = k;
				break;
			}
		}
		if (slot < 0)
			return;
		fid = app->file_lo[slot];
	}
	i = 0;
	last = -1;
	while (i < n) {
		if (tok_type(toks[i], "IDENT")) {
			last = (int)i;
			i++;
			continue;
		}
		if (tok_type(toks[i], "KW")) {
			i++;
			continue;
		}
		if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, "{")) {
			i = skip_balanced(toks, n, i, src, "{", "}");
			last = -1;
			continue;
		}
		if (tok_type(toks[i], "PUNCT") && tok_lex(toks[i], src, "(") && last >= 0) {
			copy_name(name, sizeof name, src, toks[last]);
			after = skip_balanced(toks, n, i, src, "(", ")");
			if (after < n && tok_type(toks[after], "PUNCT") && tok_lex(toks[after], src, "{")) {
				body = after;
				i = skip_balanced(toks, n, after, src, "{", "}");
				if (!calls) {
					fid = add_func_def(app, base, name);
					if (fid >= 0) {
						app->funcs[fid].body_lo = body;
						app->funcs[fid].body_hi = i;
					}
				} else if (fid < app->file_hi[slot]) {
					app->funcs[fid].body_lo = body;
					app->funcs[fid].body_hi = i;
					scan_calls(app, fid, toks, n, src, body, i);
					fid++;
				}
				last = -1;
				continue;
			}
			i = after;
			last = -1;
			continue;
		}
		last = -1;
		i++;
	}
}

void
parse_source(App *app, char *src, size_t len, const char *file, int calls)
{
	static unsigned char arena[1 << 24];
	CastMemory mem;
	CastToken *toks;
	size_t n;

	mem = cast_memory_create(arena, sizeof arena);
	n = 0;
	toks = cast_tokenize(&mem, src, len, &n);
	if (!toks || n == 0)
		return;
	scan_tokens(app, toks, n, src, file, calls);
}

void
parse_file(App *app, const char *path, int calls)
{
	unsigned long n;
	char *buf;
	int slot;

	buf = peak_file_alloc(path, &n);
	if (!buf) {
		PERROR("cannot read %s", path);
		return;
	}
	slot = -1;
	if (!calls) {
		if (app->nfiles >= MAX_FILES) {
			free(buf);
			return;
		}
		slot = app->nfiles;
		snprintf(app->files[slot], MAX_PATH, "%s", path);
		app->file_lo[slot] = app->nfuncs;
	}
	parse_source(app, buf, (size_t)n, path, calls);
	if (!calls) {
		app->file_hi[slot] = app->nfuncs;
		app->nfiles++;
	}
	free(buf);
}

int
collect_c(const char *name, void *ud)
{
	FileList *fl;
	size_t n, dlen;
	char *out;
	char saved[MAX_PATH];
	char sub[MAX_PATH];

	fl = ud;
	n = strlen(name);
	if (name[0] == '.')
		return 1;
	dlen = strlen(fl->dir);
	if (dlen + 1 + n + 1 > MAX_PATH)
		return 1;
	memcpy(sub, fl->dir, dlen);
	sub[dlen] = '/';
	memcpy(sub + dlen + 1, name, n);
	sub[dlen + 1 + n] = 0;
	if (n >= 2 && name[n - 2] == '.' && name[n - 1] == 'c') {
		if (fl->n >= MAX_FILES)
			return 0;
		out = fl->paths[fl->n];
		memcpy(out, sub, dlen + 1 + n + 1);
		fl->n++;
		return 1;
	}
	snprintf(saved, sizeof saved, "%s", fl->dir);
	snprintf(fl->dir, sizeof fl->dir, "%s", sub);
	peak_filesystem_list(sub, collect_c, fl);
	snprintf(fl->dir, sizeof fl->dir, "%s", saved);
	return fl->n < MAX_FILES;
}

void
ingest_path(App *app, const char *path)
{
	FileList fl;
	int i;

	memset(&fl, 0, sizeof fl);
	snprintf(fl.dir, sizeof fl.dir, "%s", path);
	if (peak_filesystem_list(path, collect_c, &fl) && fl.n > 0) {
		for (i = 0; i < fl.n; i++)
			parse_file(app, fl.paths[i], 0);
		return;
	}
	parse_file(app, path, 0);
}

void
bfs_walk(App *app, int *q, int *head, int *tail, int *seen)
{
	int i, u, v;

	while (*head < *tail) {
		u = q[(*head)++];
		if (app->funcs[u].depth > app->max_depth)
			app->max_depth = app->funcs[u].depth;
		for (i = 0; i < app->funcs[u].ncalls; i++) {
			v = app->funcs[u].calls[i].to;
			if (seen[v])
				continue;
			seen[v] = 1;
			app->funcs[v].in_tree = 1;
			app->funcs[v].depth = app->funcs[u].depth + 1;
			if (*tail < MAX_FUNCS)
				q[(*tail)++] = v;
		}
	}
}

void
layout_graph(App *app)
{
	int q[MAX_FUNCS];
	int head, tail, i, u, d, nlev, two;
	int counts[256];
	int seen[MAX_FUNCS];
	float x;

	app->root = -1;
	app->nroots = 0;
	for (i = 0; i < app->nfuncs; i++) {
		app->funcs[i].is_root = 0;
		app->funcs[i].in_tree = 0;
		app->funcs[i].depth = 0;
		app->funcs[i].slot = 0;
		if (app->funcs[i].defined && strcmp(app->funcs[i].name, "main") == 0) {
			app->funcs[i].is_root = 1;
			app->nroots++;
			if (app->root < 0)
				app->root = i;
		}
	}
	if (app->nroots == 0) {
		for (i = 0; i < app->nfuncs; i++) {
			if (app->funcs[i].defined) {
				app->funcs[i].is_root = 1;
				app->nroots = 1;
				app->root = i;
				break;
			}
		}
	}
	app->ntree = 0;
	app->max_depth = 0;
	if (app->nroots == 0)
		return;

	head = 0;
	tail = 0;
	memset(seen, 0, sizeof seen);
	for (i = 0; i < app->nfuncs; i++) {
		if (!app->funcs[i].is_root)
			continue;
		q[tail++] = i;
		app->funcs[i].in_tree = 1;
		app->funcs[i].depth = 0;
		seen[i] = 1;
	}
	bfs_walk(app, q, &head, &tail, seen);
	d = app->max_depth + 1;
	for (i = 0; i < app->nfuncs; i++) {
		if (!app->funcs[i].defined || seen[i])
			continue;
		if (tail >= MAX_FUNCS)
			break;
		q[tail++] = i;
		app->funcs[i].in_tree = 1;
		app->funcs[i].depth = d;
		seen[i] = 1;
	}
	bfs_walk(app, q, &head, &tail, seen);
	app->ntree = tail;

	memset(counts, 0, sizeof counts);
	for (i = 0; i < tail; i++) {
		u = q[i];
		d = app->funcs[u].depth;
		if (d < 256)
			app->funcs[u].slot = counts[d]++;
	}
	for (i = 0; i < tail; i++) {
		u = q[i];
		d = app->funcs[u].depth;
		nlev = (d < 256) ? counts[d] : 1;
		if (nlev < 1)
			nlev = 1;
		two = name_dup(app, app->funcs[u].name) || (app->nroots > 1 && app->funcs[u].is_root);
		x = ((float)app->funcs[u].slot - 0.5f * (float)(nlev - 1)) * (NODE_W + GAP_X);
		app->funcs[u].w = NODE_W;
		app->funcs[u].h = two ? NODE_H + 14.0f : NODE_H;
		app->funcs[u].x = x - NODE_W * 0.5f;
		app->funcs[u].y = (float)d * (NODE_H + GAP_Y);
	}
	app->fitted = 0;
}

void
fit_camera(App *app, float W, float H)
{
	int i, any;
	float minx, miny, maxx, maxy, zx, zy, pad;

	any = 0;
	minx = miny = 0;
	maxx = maxy = 0;
	for (i = 0; i < app->nfuncs; i++) {
		if (!app->funcs[i].in_tree)
			continue;
		if (!any) {
			minx = app->funcs[i].x;
			miny = app->funcs[i].y;
			maxx = app->funcs[i].x + app->funcs[i].w;
			maxy = app->funcs[i].y + app->funcs[i].h;
			any = 1;
		} else {
			if (app->funcs[i].x < minx)
				minx = app->funcs[i].x;
			if (app->funcs[i].y < miny)
				miny = app->funcs[i].y;
			if (app->funcs[i].x + app->funcs[i].w > maxx)
				maxx = app->funcs[i].x + app->funcs[i].w;
			if (app->funcs[i].y + app->funcs[i].h > maxy)
				maxy = app->funcs[i].y + app->funcs[i].h;
		}
	}
	if (!any) {
		app->cam_x = 0;
		app->cam_y = 0;
		app->zoom = 1;
		app->fitted = 1;
		return;
	}
	pad = 80.0f;
	minx -= pad;
	miny -= pad;
	maxx += pad;
	maxy += pad + 24.0f;
	zx = W / (maxx - minx);
	zy = H / (maxy - miny);
	app->zoom = (zx < zy) ? zx : zy;
	if (app->zoom > 2.5f)
		app->zoom = 2.5f;
	if (app->zoom < 0.15f)
		app->zoom = 0.15f;
	app->cam_x = 0.5f * (minx + maxx);
	app->cam_y = 0.5f * (miny + maxy);
	app->fitted = 1;
}

void
world_to_screen(App *app, float wx, float wy, float W, float H, float *sx, float *sy)
{
	*sx = (wx - app->cam_x) * app->zoom + W * 0.5f;
	*sy = (wy - app->cam_y) * app->zoom + H * 0.5f;
}

void
screen_to_world(App *app, float sx, float sy, float W, float H, float *wx, float *wy)
{
	*wx = app->cam_x + (sx - W * 0.5f) / app->zoom;
	*wy = app->cam_y + (sy - H * 0.5f) / app->zoom;
}

int
hit_node(App *app, float wx, float wy)
{
	int i;

	for (i = app->nfuncs - 1; i >= 0; i--) {
		if (!app->funcs[i].in_tree)
			continue;
		if (wx >= app->funcs[i].x && wx <= app->funcs[i].x + app->funcs[i].w &&
		    wy >= app->funcs[i].y && wy <= app->funcs[i].y + app->funcs[i].h)
			return i;
	}
	return -1;
}

uint32_t
pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

void
unpack_rgba(uint32_t c, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	*r = (uint8_t)(c & 255);
	*g = (uint8_t)((c >> 8) & 255);
	*b = (uint8_t)((c >> 16) & 255);
	*a = (uint8_t)((c >> 24) & 255);
}

void
pix_put(App *app, int x, int y, uint32_t c)
{
	uint8_t sr, sg, sb, sa, dr, dg, db, da;
	uint32_t *dst;
	int t;

	if ((uint32_t)x >= app->pw || (uint32_t)y >= app->ph)
		return;
	unpack_rgba(c, &sr, &sg, &sb, &sa);
	if (sa == 0)
		return;
	dst = app->pix + (size_t)y * app->pw + (size_t)x;
	if (sa == 255) {
		*dst = c;
		return;
	}
	unpack_rgba(*dst, &dr, &dg, &db, &da);
	t = 255 - sa;
	dr = (uint8_t)((sr * sa + dr * t) / 255);
	dg = (uint8_t)((sg * sa + dg * t) / 255);
	db = (uint8_t)((sb * sa + db * t) / 255);
	*dst = pack_rgba(dr, dg, db, 255);
}

void
fill_rect(App *app, int x, int y, int w, int h, uint32_t c)
{
	int ix, iy, x1, y1;

	if (w <= 0 || h <= 0)
		return;
	x1 = x + w;
	y1 = y + h;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x1 > (int)app->pw)
		x1 = (int)app->pw;
	if (y1 > (int)app->ph)
		y1 = (int)app->ph;
	for (iy = y; iy < y1; iy++) {
		for (ix = x; ix < x1; ix++)
			app->pix[(size_t)iy * app->pw + (size_t)ix] = c;
	}
}

void
fill_rect_border(App *app, int x, int y, int w, int h, uint32_t fill, uint32_t border)
{
	fill_rect(app, x, y, w, h, fill);
	fill_rect(app, x, y, w, 2, border);
	fill_rect(app, x, y + h - 2, w, 2, border);
	fill_rect(app, x, y, 2, h, border);
	fill_rect(app, x + w - 2, y, 2, h, border);
}

void
draw_line(App *app, int x0, int y0, int x1, int y1, uint32_t c)
{
	int dx, dy, sx, sy, err, e2;

	dx = (x1 > x0) ? x1 - x0 : x0 - x1;
	dy = (y1 > y0) ? y1 - y0 : y0 - y1;
	sx = (x0 < x1) ? 1 : -1;
	sy = (y0 < y1) ? 1 : -1;
	err = dx - dy;
	for (;;) {
		pix_put(app, x0, y0, c);
		pix_put(app, x0 + 1, y0, c);
		pix_put(app, x0, y0 + 1, c);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void
draw_ortho(App *app, float x0, float y0, float x1, float y1, uint32_t c)
{
	float midy;

	midy = 0.5f * (y0 + y1);
	draw_line(app, (int)x0, (int)y0, (int)x0, (int)midy, c);
	draw_line(app, (int)x0, (int)midy, (int)x1, (int)midy, c);
	draw_line(app, (int)x1, (int)midy, (int)x1, (int)y1, c);
}

void
draw_char(App *app, int x, int y, char ch, uint32_t c, int scale)
{
	int gx, gy, bit, row;
	unsigned char u;

	u = (unsigned char)ch;
	if (u < 32 || u > 126)
		u = '?';
	u = (unsigned char)(u - 32);
	if (scale < 1)
		scale = 1;
	for (gy = 0; gy < FONT_H; gy++) {
		row = font8[u][gy];
		for (gx = 0; gx < FONT_W; gx++) {
			bit = row & (1 << gx);
			if (bit)
				fill_rect(app, x + gx * scale, y + gy * scale, scale, scale, c);
		}
	}
}

void
draw_text(App *app, int x, int y, const char *s, uint32_t c, int scale)
{
	while (*s) {
		draw_char(app, x, y, *s, c, scale);
		x += FONT_W * scale;
		s++;
	}
}

int
ensure_canvas(App *app, RendRenderer renderer, uint32_t w, uint32_t h)
{
	size_t bytes;

	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	if (app->have_tex && app->pw == w && app->ph == h && app->pix)
		return 1;
	if (app->have_tex) {
		rend_texture_destroy(renderer, &app->canvas);
		app->have_tex = 0;
	}
	free(app->pix);
	app->pix = NULL;
	bytes = (size_t)w * (size_t)h * 4u;
	app->pix = malloc(bytes);
	if (!app->pix)
		return 0;
	app->canvas = rend_texture_create(renderer, w, h, 1, 1, 1, REND_FORMAT_R8G8B8A8_UNORM);
	if (!app->canvas.handle) {
		free(app->pix);
		app->pix = NULL;
		return 0;
	}
	app->pw = w;
	app->ph = h;
	app->have_tex = 1;
	return 1;
}

void
draw_graph(App *app, float W, float H, const char *status)
{
	int i, k, v, hot, tw, lx, ly, two, scale, lscale;
	float x0, y0, x1, y1, nx, ny, nw, nh, mx, my;
	uint32_t fill, edge;
	char line[256];
	char lab[64];

	fill_rect(app, 0, 0, (int)app->pw, (int)app->ph, COL_BG);
	if (!app->fitted)
		fit_camera(app, W, H);

	for (i = 0; i < app->nfuncs; i++) {
		if (!app->funcs[i].in_tree)
			continue;
		world_to_screen(app, app->funcs[i].x + app->funcs[i].w * 0.5f,
		    app->funcs[i].y + app->funcs[i].h, W, H, &x0, &y0);
		for (k = 0; k < app->funcs[i].ncalls; k++) {
			v = app->funcs[i].calls[k].to;
			if (!app->funcs[v].in_tree)
				continue;
			world_to_screen(app, app->funcs[v].x + app->funcs[v].w * 0.5f,
			    app->funcs[v].y, W, H, &x1, &y1);
			hot = (app->selected == i || app->selected == v);
			edge = hot ? COL_EDGE_H : COL_EDGE;
			draw_ortho(app, x0, y0, x1, y1, edge);
			ctx_label(app->funcs[i].calls[k].ctx, lab, sizeof lab);
			if (lab[0] && app->zoom >= 0.45f) {
				mx = 0.5f * (x0 + x1);
				my = 0.5f * (y0 + y1);
				lscale = (app->zoom >= 1.2f) ? 2 : 1;
				tw = (int)strlen(lab) * FONT_W * lscale;
				lx = (int)mx - tw / 2;
				ly = (int)my - FONT_H * lscale / 2;
				fill_rect(app, lx - 3, ly - 2, tw + 6, FONT_H * lscale + 4, COL_HUD);
				draw_text(app, lx, ly, lab, hot ? COL_EDGE_H : COL_TEXT, lscale);
			}
		}
	}

	scale = (int)(app->zoom * 1.25f);
	if (scale < 1)
		scale = 1;
	if (scale > 3)
		scale = 3;
	for (i = 0; i < app->nfuncs; i++) {
		if (!app->funcs[i].in_tree)
			continue;
		world_to_screen(app, app->funcs[i].x, app->funcs[i].y, W, H, &nx, &ny);
		nw = app->funcs[i].w * app->zoom;
		nh = app->funcs[i].h * app->zoom;
		if (nw < 8.0f || nh < 8.0f)
			continue;
		if (app->funcs[i].is_root)
			fill = COL_ROOT;
		else if (i == app->selected)
			fill = COL_SEL;
		else
			fill = COL_NODE;
		fill_rect_border(app, (int)nx, (int)ny, (int)nw, (int)nh, fill, COL_BORDER);
		two = name_dup(app, app->funcs[i].name) || (app->nroots > 1 && app->funcs[i].is_root);
		if (two) {
			draw_text(app, (int)nx + 8, (int)ny + 6, app->funcs[i].name, COL_TEXT, scale);
			draw_text(app, (int)nx + 8, (int)ny + 6 + FONT_H * scale + 2, app->funcs[i].file, COL_DIM, 1);
		} else {
			draw_text(app, (int)nx + 8, (int)ny + ((int)nh - FONT_H * scale) / 2,
			    app->funcs[i].name, COL_TEXT, scale);
		}
	}

	fill_rect(app, 0, 0, (int)app->pw, 28, COL_HUD);
	draw_text(app, 8, 8, status, COL_DIM, 1);
	fill_rect(app, 0, (int)app->ph - 24, (int)app->pw, 24, COL_HUD);
	snprintf(line, sizeof line, "drag pan   wheel zoom   click select   r reset   esc quit");
	draw_text(app, 8, (int)app->ph - 18, line, COL_DIM, 1);
}

int
main(int argc, char **argv)
{
	App app;
	PeakWindow win;
	RendBindingInfo bind;
	RendRenderer renderer;
	RendTexture *color;
	PeakEvent ev;
	int i, running, hit;
	uint32_t width, height;
	float mx, my, wx, wy, W, H, oldz;
	char status[256];

	memset(&app, 0, sizeof app);
	app.zoom = 1.0f;
	app.selected = -1;
	app.root = -1;

	if (argc < 2) {
		char *copy;
		size_t n;

		n = strlen(sample_src);
		copy = malloc(n + 1);
		if (!copy)
			return 1;
		memcpy(copy, sample_src, n + 1);
		parse_source(&app, copy, n, "(sample)", 0);
		snprintf(app.files[0], MAX_PATH, "%s", "(sample)");
		app.file_lo[0] = 0;
		app.file_hi[0] = app.nfuncs;
		app.nfiles = 1;
		parse_source(&app, copy, n, "(sample)", 1);
		free(copy);
		PINFO("no files; sample graph. usage: codeanalizer [file.c|dir] ...");
	} else {
		for (i = 1; i < argc; i++)
			ingest_path(&app, argv[i]);
		for (i = 0; i < app.nfiles; i++)
			parse_file(&app, app.files[i], 1);
	}
	layout_graph(&app);
	snprintf(status, sizeof status, "%d root%s   %d funcs in tree / %d defined",
	    app.nroots, app.nroots == 1 ? "" : "s", app.ntree, app.nfuncs);
	PINFO("%s", status);

	if (!peak_init()) {
		PFATAL("peak_init failed");
		return 1;
	}
	width = 1280;
	height = 720;
	win = peak_window_open("codeanalizer", width, height, 0);
	if (!win.running) {
		PFATAL("window_open failed");
		peak_quit();
		return 1;
	}
	width = win.width;
	height = win.height;

	memset(&bind, 0, sizeof bind);
	renderer = rend_renderer_create(&win, REND_BACKEND_CPU, NULL, true, &bind);
	if (!renderer) {
		PFATAL("renderer_create failed");
		peak_window_close(&win);
		peak_quit();
		return 1;
	}

	mx = 0;
	my = 0;
	running = 1;
	while (running && win.running) {
		while (peak_window_epoll(&win, &ev)) {
			if (ev.type == PEAK_EVENT_WINDOW_CLOSE)
				running = 0;
			if (ev.type == PEAK_EVENT_KEY_DOWN) {
				if (ev.key.key == PEAK_KEY_ESCAPE)
					running = 0;
				if (ev.key.key == PEAK_KEY_R)
					app.fitted = 0;
			}
			if (ev.type == PEAK_EVENT_WINDOW_RESIZE) {
				width = ev.resize.width;
				height = ev.resize.height;
				win.width = width;
				win.height = height;
				app.fitted = 0;
			}
			if (ev.type == PEAK_EVENT_POINTER) {
				W = (float)width;
				H = (float)height;
				if (ev.pointer.type == PEAK_POINTER_WHEEL_UP) {
					screen_to_world(&app, mx, my, W, H, &wx, &wy);
					oldz = app.zoom;
					app.zoom *= 1.12f;
					if (app.zoom > 6.0f)
						app.zoom = 6.0f;
					app.cam_x = wx - (mx - W * 0.5f) / app.zoom;
					app.cam_y = wy - (my - H * 0.5f) / app.zoom;
					(void)oldz;
				} else if (ev.pointer.type == PEAK_POINTER_WHEEL_DOWN) {
					screen_to_world(&app, mx, my, W, H, &wx, &wy);
					app.zoom /= 1.12f;
					if (app.zoom < 0.12f)
						app.zoom = 0.12f;
					app.cam_x = wx - (mx - W * 0.5f) / app.zoom;
					app.cam_y = wy - (my - H * 0.5f) / app.zoom;
				} else {
					mx = ev.pointer.x;
					my = ev.pointer.y;
					if (ev.pointer.type == PEAK_POINTER_LEFT ||
					    ev.pointer.type == PEAK_POINTER_MIDDLE) {
						if (ev.pointer.state == PEAK_POINTER_PRESSED) {
							screen_to_world(&app, mx, my, W, H, &wx, &wy);
							hit = hit_node(&app, wx, wy);
							app.dragging = 1;
							app.drag_node = hit;
							app.drag_mx = mx;
							app.drag_my = my;
							app.pan_x = app.cam_x;
							app.pan_y = app.cam_y;
							if (hit >= 0)
								app.selected = hit;
						} else if (ev.pointer.state == PEAK_POINTER_RELEASED) {
							app.dragging = 0;
							app.drag_node = -1;
						} else if (ev.pointer.state == PEAK_POINTER_MOVED && app.dragging) {
							if (app.drag_node < 0) {
								app.cam_x = app.pan_x - (mx - app.drag_mx) / app.zoom;
								app.cam_y = app.pan_y - (my - app.drag_my) / app.zoom;
							} else {
								screen_to_world(&app, mx, my, W, H, &wx, &wy);
								app.funcs[app.drag_node].x = wx - app.funcs[app.drag_node].w * 0.5f;
								app.funcs[app.drag_node].y = wy - app.funcs[app.drag_node].h * 0.5f;
							}
						}
					}
				}
			}
		}

		if (!ensure_canvas(&app, renderer, width, height)) {
			PFATAL("canvas alloc failed");
			break;
		}
		draw_graph(&app, (float)width, (float)height, status);
		rend_texture_copy_data(renderer, &app.canvas, app.pix,
		    (size_t)app.pw * (size_t)app.ph * 4u);

		if (rend_renderer_frame_begin(renderer)) {
			color = rend_renderer_color_target(renderer);
			if (color)
				rend_cmd_blit(renderer, &app.canvas, color, 0, 0, app.pw, app.ph, 0, 0, width, height);
			rend_renderer_frame_end(renderer, NULL);
		}
		peak_wait(&win, NULL, 0, 16);
	}

	if (app.have_tex)
		rend_texture_destroy(renderer, &app.canvas);
	free(app.pix);
	rend_renderer_destroy(renderer);
	peak_window_close(&win);
	peak_quit();
	rend_quit();
	return 0;
}
