/* Copyright (c) 2026 Vasco Alves. No heap allocation; bounded traversal. */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "fuse_dock.h"

typedef struct FuseDockWindow {
	char name[32], title[64];
	float min_w, min_h;
} FuseDockWindow;
struct FuseDock {
	int capacity, count, revision, focused, tab_focus;
	int rollback_focus, rollback_tab_focus;
	int capture, window, node, dragging, side, target;
	float start_x, start_y;
	FuseDockRect host, start_box, boxes[FUSE_DOCK_NODES];
	FuseDockLayout layout, rollback;
	FuseDockWindow windows[FUSE_DOCK_MAX];
};

static int fuse_dock_contains(FuseDockRect r, float x, float y);
static int fuse_dock_group(const FuseDock *d, int window);
static int fuse_dock_slot(FuseDock *d);
static int fuse_dock_top_float(const FuseDock *d, float x, float y);
static int fuse_dock_can_split(const FuseDock *d, int window, int group, FuseDockSide side);
static void fuse_dock_cancel(FuseDock *d);
static int fuse_dock_float_root(const FuseDock *d, int node);
static void fuse_dock_remove(FuseDock *d, int window);
static void fuse_dock_min(const FuseDock *d, int node, float *w, float *h);
static void fuse_dock_resolve(FuseDock *d, int node, FuseDockRect r);
static void fuse_dock_raise(FuseDock *d, int node);
static FuseDockSide fuse_dock_target(const FuseDock *d, int node, float x, float y);
static int fuse_dock_check(const FuseDock *d, const FuseDockLayout *l, int node, int floating, unsigned *seen, unsigned *windows);

size_t
fuse_dock_memory(void)
{
	return sizeof(FuseDock);
}

FuseDock *
fuse_dock_create(void *memory, size_t size, int capacity)
{
	FuseDock *d = memory;
	if (!d || (uintptr_t)d % 8 || size < sizeof(*d) || capacity < 1 || capacity > FUSE_DOCK_MAX)
		return NULL;
	memset(d, 0, sizeof(*d));
	d->capacity = capacity;
	d->layout.root = -1;
	d->focused = -1;
	return d;
}

int
fuse_dock_register(FuseDock *d, const char *name, const char *title, float w, float h)
{
	int i;
	if (!d || d->capture || !name || !title || !name[0] || strlen(name) >= 32 || strlen(title) >= 64 ||
		!isfinite(w) || !isfinite(h) || w < 0 || h < 0 || d->count == d->capacity)
		return -1;
	for (i = 0; i < d->count; i++)
		if (!strcmp(name, d->windows[i].name))
			return -1;
	i = d->count++;
	strcpy(d->windows[i].name, name);
	strcpy(d->windows[i].title, title);
	d->windows[i].min_w = w;
	d->windows[i].min_h = h;
	fuse_dock_place(d, i, -1, FUSE_DOCK_CENTER);
	return i;
}

const char *
fuse_dock_name(const FuseDock *d, int window)
{
	return window >= 0 && window < d->count ? d->windows[window].name : NULL;
}

const char *
fuse_dock_title(const FuseDock *d, int window)
{
	return window >= 0 && window < d->count ? d->windows[window].title : NULL;
}

int
fuse_dock_contains(FuseDockRect r, float x, float y)
{
	return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

int
fuse_dock_group(const FuseDock *d, int window)
{
	int i, j;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind == 1)
			for (j = 0; j < d->layout.nodes[i].count; j++)
				if (d->layout.nodes[i].tabs[j] == window)
					return i;
	return -1;
}

int
fuse_dock_float_root(const FuseDock *d, int node)
{
	int i, depth;
	for (depth = 0; node >= 0 && depth < FUSE_DOCK_NODES; depth++) {
		if (d->layout.nodes[node].floating) return node;
		for (i = 0; i < FUSE_DOCK_NODES; i++)
			if (d->layout.nodes[i].kind >= 2 && (d->layout.nodes[i].a == node || d->layout.nodes[i].b == node)) break;
		if (i == FUSE_DOCK_NODES) return -1;
		node = i;
	}
	return -1;
}

/* A floating split's gaps occlude docked panels and splitters too. */
int
fuse_dock_top_float(const FuseDock *d, float x, float y)
{
	int i, top = -1;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind && d->layout.nodes[i].floating &&
			fuse_dock_contains(d->boxes[i], x, y) &&
			(top < 0 || d->layout.nodes[i].order > d->layout.nodes[top].order))
			top = i;
	return top;
}

int
fuse_dock_can_split(const FuseDock *d, int window, int group, FuseDockSide side)
{
	float mw, mh, tw = 0, th = 0;
	int i;
	const FuseDockNode *n;
	if (group < 0 || side == FUSE_DOCK_CENTER || side == FUSE_DOCK_FLOAT || (d->host.w == 0 && d->host.h == 0))
		return 1;
	n = &d->layout.nodes[group];
	/* The dragged tab leaves this group: do not count its minimum twice. */
	for (i = 0; i < n->count; i++) {
		if (n->tabs[i] == window) continue;
		tw = fmaxf(tw, d->windows[n->tabs[i]].min_w);
		th = fmaxf(th, d->windows[n->tabs[i]].min_h + FUSE_DOCK_TITLE);
	}
	mw = d->windows[window].min_w;
	mh = d->windows[window].min_h + FUSE_DOCK_TITLE;
	return d->boxes[group].w >= (side <= FUSE_DOCK_RIGHT ? mw + tw + 4 : fmaxf(mw, tw)) &&
		d->boxes[group].h >= (side >= FUSE_DOCK_TOP ? mh + th + 4 : fmaxf(mh, th));
}

void
fuse_dock_cancel(FuseDock *d)
{
	if (!d->capture) return;
	d->layout = d->rollback;
	d->focused = d->rollback_focus;
	d->tab_focus = d->rollback_tab_focus;
	d->capture = 0;
	d->revision++;
}

int
fuse_dock_slot(FuseDock *d)
{
	int i;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (!d->layout.nodes[i].kind)
			return i;
	return -1;
}

void
fuse_dock_remove(FuseDock *d, int window)
{
	int i, j, group = fuse_dock_group(d, window);
	FuseDockNode *n;
	if (group < 0)
		return;
	n = &d->layout.nodes[group];
	for (j = 0; j < n->count && n->tabs[j] != window; j++) {}
	memmove(n->tabs + j, n->tabs + j + 1, (size_t)(n->count - j - 1) * sizeof(int));
	if (--n->count) {
		if (j < n->active)
			n->active--;
		else if (n->active >= n->count)
			n->active = n->count - 1;
		return;
	}
	n->kind = 0;
	if (d->layout.root == group)
		d->layout.root = -1;
	for (i = 0; i < FUSE_DOCK_NODES; i++) {
		n = &d->layout.nodes[i];
		if (n->kind < 2 || (n->a != group && n->b != group))
			continue;
		/* Copy sibling into parent; indexed parent references stay valid. */
		j = n->a == group ? n->b : n->a;
		{
			int floating = n->floating, order = n->order;
			FuseDockRect rect = n->rect;
			*n = d->layout.nodes[j];
			n->floating = floating;
			if (floating) { n->rect = rect; n->order = order; }
		}
		d->layout.nodes[j].kind = 0;
		break;
	}
}

void
fuse_dock_reset(FuseDock *d)
{
	int i;
	memset(&d->layout, 0, sizeof(d->layout));
	d->layout.root = -1;
	d->capture = 0;
	d->focused = -1;
	d->tab_focus = 0;
	d->revision++;
	for (i = 0; i < d->count; i++)
		fuse_dock_place(d, i, -1, FUSE_DOCK_CENTER);
}

int
fuse_dock_place(FuseDock *d, int window, int target, FuseDockSide side)
{
	FuseDockLayout before;
	FuseDockNode *n;
	int group, source, slot, child;
	if (!d || d->capture || window < 0 || window >= d->count || side < 0 || side > FUSE_DOCK_FLOAT)
		return 0;
	if (side == FUSE_DOCK_FLOAT) target = -1;
	group = fuse_dock_group(d, target);
	source = fuse_dock_group(d, window);
	if (target >= 0 && group < 0)
		return 0;
	if (source == group && source >= 0 && (d->layout.nodes[source].count == 1 ||
		(side == FUSE_DOCK_CENTER && target == window))) {
		n = &d->layout.nodes[source];
		for (slot = 0; n->tabs[slot] != window; slot++) {}
		if (n->active != slot) { n->active = slot; d->revision++; }
		d->focused = window;
		return 1;
	}
	if (!fuse_dock_can_split(d, window, group, side))
		return 0;
	if (target == window && group >= 0 && d->layout.nodes[group].count > 1)
		target = d->layout.nodes[group].tabs[d->layout.nodes[group].tabs[0] == window ? 1 : 0];
	before = d->layout;
	fuse_dock_remove(d, window);
	/* A collapsing source can relocate the target group. */
	group = fuse_dock_group(d, target);
	if (side == FUSE_DOCK_CENTER && group < 0 && d->layout.root >= 0)
		group = fuse_dock_group(d, fuse_dock_visible(d, 0));
	if (side == FUSE_DOCK_CENTER && group >= 0) {
		n = &d->layout.nodes[group];
		n->tabs[n->count++] = window;
		n->active = n->count - 1;
	} else {
		if ((slot = fuse_dock_slot(d)) < 0)
			goto fail;
		n = &d->layout.nodes[slot];
		memset(n, 0, sizeof(*n));
		n->kind = 1;
		n->count = 1;
		n->tabs[0] = window;
		if (side == FUSE_DOCK_FLOAT || (group < 0 && d->layout.root >= 0)) {
			n->floating = 1;
			n->rect = (FuseDockRect){0.1f, 0.1f, 0.5f, 0.5f};
			fuse_dock_raise(d, slot);
		} else if (group < 0) {
			d->layout.root = slot;
		} else {
			if ((child = fuse_dock_slot(d)) < 0)
				goto fail;
			d->layout.nodes[child] = d->layout.nodes[group];
			d->layout.nodes[child].floating = 0;
			n = &d->layout.nodes[group];
			{
				int floating = n->floating, order = n->order;
				FuseDockRect rect = n->rect;
				memset(n, 0, sizeof(*n));
				n->floating = floating; n->order = order; n->rect = rect;
			}
			n->kind = side <= FUSE_DOCK_RIGHT ? 2 : 3;
			n->ratio = 0.5f;
			n->a = side == FUSE_DOCK_LEFT || side == FUSE_DOCK_TOP ? slot : child;
			n->b = n->a == slot ? child : slot;
		}
	}
	d->focused = window;
	if (memcmp(&before, &d->layout, sizeof(before))) d->revision++;
	fuse_dock_layout(d, d->host);
	return 1;
fail:
	d->layout = before;
	return 0;
}

void
fuse_dock_min(const FuseDock *d, int node, float *w, float *h)
{
	const FuseDockNode *n = &d->layout.nodes[node];
	float aw, ah, bw, bh;
	int i;
	*w = *h = 0;
	if (n->kind == 1) {
		for (i = 0; i < n->count; i++) {
			*w = fmaxf(*w, d->windows[n->tabs[i]].min_w);
			*h = fmaxf(*h, d->windows[n->tabs[i]].min_h + FUSE_DOCK_TITLE);
		}
		return;
	}
	fuse_dock_min(d, n->a, &aw, &ah);
	fuse_dock_min(d, n->b, &bw, &bh);
	*w = n->kind == 2 ? aw + bw + 4 : fmaxf(aw, bw);
	*h = n->kind == 3 ? ah + bh + 4 : fmaxf(ah, bh);
}

void
fuse_dock_resolve(FuseDock *d, int node, FuseDockRect r)
{
	FuseDockNode *n = &d->layout.nodes[node];
	FuseDockRect a = r, b = r;
	float aw, ah, bw, bh, length, cut, gap;
	d->boxes[node] = r;
	if (n->kind == 1)
		return;
	fuse_dock_min(d, n->a, &aw, &ah);
	fuse_dock_min(d, n->b, &bw, &bh);
	gap = fminf(4, n->kind == 2 ? r.w : r.h);
	length = fmaxf(0, (n->kind == 2 ? r.w : r.h) - gap);
	cut = length * n->ratio;
	if (length >= (n->kind == 2 ? aw + bw : ah + bh))
		cut = fmaxf(n->kind == 2 ? aw : ah, fminf(cut, length - (n->kind == 2 ? bw : bh)));
	if (n->kind == 2) {
		a.w = cut;
		b.x += cut + gap;
		b.w = length - cut;
	} else {
		a.h = cut;
		b.y += cut + gap;
		b.h = length - cut;
	}
	fuse_dock_resolve(d, n->a, a);
	fuse_dock_resolve(d, n->b, b);
}

void
fuse_dock_layout(FuseDock *d, FuseDockRect host)
{
	FuseDockNode *n;
	FuseDockRect r;
	float w, h;
	int i;
	if (!d || !isfinite(host.x) || !isfinite(host.y) || !isfinite(host.w) || !isfinite(host.h) || !isfinite(host.x + host.w) || !isfinite(host.y + host.h) || host.w < 0 || host.h < 0)
		return;
	if (d->capture && (host.x != d->host.x || host.y != d->host.y || host.w != d->host.w || host.h != d->host.h))
		fuse_dock_cancel(d);
	d->host = host;
	memset(d->boxes, 0, sizeof(d->boxes));
	if (d->layout.root >= 0)
		fuse_dock_resolve(d, d->layout.root, host);
	for (i = 0; i < FUSE_DOCK_NODES; i++) {
		n = &d->layout.nodes[i];
		if (!n->kind || !n->floating)
			continue;
		fuse_dock_min(d, i, &w, &h);
		r.w = fminf(host.w, fmaxf(w, host.w * n->rect.w));
		r.h = fminf(host.h, fmaxf(h, host.h * n->rect.h));
		r.x = host.x + fmaxf(0, fminf(host.w - r.w, host.w * n->rect.x));
		r.y = host.y + fmaxf(0, fminf(host.h - r.h, host.h * n->rect.y));
		fuse_dock_resolve(d, i, r);
	}
}

void
fuse_dock_raise(FuseDock *d, int node)
{
	int i, old = d->layout.nodes[node].order;
	if (old == FUSE_DOCK_MAX) return;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind && d->layout.nodes[i].floating && d->layout.nodes[i].order > old)
			d->layout.nodes[i].order--;
	d->layout.nodes[node].order = FUSE_DOCK_MAX;
	/* Renormalize to avoid unbounded counters over the workspace lifetime. */
}

int
fuse_dock_visible(const FuseDock *d, int position)
{
	int i, order;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind == 1 && fuse_dock_float_root(d, i) < 0)
			if (!position--)
				return d->layout.nodes[i].tabs[d->layout.nodes[i].active];
	for (order = 0; order <= FUSE_DOCK_MAX; order++)
		for (i = 0; i < FUSE_DOCK_NODES; i++)
			if (d->layout.nodes[i].kind == 1 && fuse_dock_float_root(d, i) >= 0 && d->layout.nodes[fuse_dock_float_root(d, i)].order == order)
				if (!position--)
					return d->layout.nodes[i].tabs[d->layout.nodes[i].active];
	return -1;
}

int
fuse_dock_hit(const FuseDock *d, float x, float y)
{
	int i, window, hit = -1, group, top;
	if (!fuse_dock_contains(d->host, x, y)) return -1;
	top = fuse_dock_top_float(d, x, y);
	for (i = 0; (window = fuse_dock_visible(d, i)) >= 0; i++) {
		group = fuse_dock_group(d, window);
		if (fuse_dock_float_root(d, group) == top && fuse_dock_contains(d->boxes[group], x, y))
			hit = window;
	}
	return hit;
}

int
fuse_dock_group_rect(const FuseDock *d, int window, FuseDockRect *r)
{
	int group = fuse_dock_group(d, window);
	if (group < 0 || d->layout.nodes[group].tabs[d->layout.nodes[group].active] != window)
		return 0;
	*r = d->boxes[group];
	return 1;
}

int
fuse_dock_content(const FuseDock *d, int window, FuseDockRect *r)
{
	if (!fuse_dock_group_rect(d, window, r))
		return 0;
	r->y += fminf(FUSE_DOCK_TITLE, r->h);
	r->h = fmaxf(0, r->h - FUSE_DOCK_TITLE);
	return r->w > 0 && r->h > 0;
}

FuseDockSide
fuse_dock_target(const FuseDock *d, int node, float x, float y)
{
	FuseDockRect r = d->boxes[node];
	float nx = (x - r.x) / fmaxf(1, r.w), ny = (y - r.y) / fmaxf(1, r.h);
	if (ny < 0.2f) return FUSE_DOCK_TOP;
	if (ny > 0.8f) return FUSE_DOCK_BOTTOM;
	if (nx < 0.2f) return FUSE_DOCK_LEFT;
	if (nx > 0.8f) return FUSE_DOCK_RIGHT;
	return FUSE_DOCK_CENTER;
}

int
fuse_dock_event(FuseDock *d, const FuseDockEvent *e)
{
	FuseDockNode *n;
	FuseDockRect r, a;
	FuseDockLayout before;
	int i, hit, group, tab, floating, top, focus_before, tabs_before;
	float dx, dy, aw, ah, bw, bh, length, cut, grip;
	if (!d || !e)
		return 0;
	if (e->cancel) {
		i = d->capture != 0;
		fuse_dock_cancel(d);
		fuse_dock_layout(d, d->host);
		return i;
	}
	if (!isfinite(e->x) || !isfinite(e->y)) {
		i = d->capture != 0;
		fuse_dock_cancel(d);
		fuse_dock_layout(d, d->host);
		return i;
	}
	if (d->capture && (e->previous || e->next || e->activate)) return 1;
	if (!d->capture && (e->previous || e->next || e->activate)) {
		if (!d->tab_focus) return 0;
		group = fuse_dock_group(d, d->focused);
		if (group < 0) return 0;
		n = &d->layout.nodes[group];
		i = n->active;
		n->active = (n->active + n->count + (e->previous ? -1 : e->next ? 1 : 0)) % n->count;
		d->focused = n->tabs[n->active];
		if (i != n->active) d->revision++;
		return 1;
	}
	hit = fuse_dock_hit(d, e->x, e->y);
	group = fuse_dock_group(d, hit);
	top = fuse_dock_top_float(d, e->x, e->y);
	if (d->capture) {
		dx = e->x - d->start_x;
		dy = e->y - d->start_y;
		d->dragging |= dx * dx + dy * dy >= 25;
		n = &d->layout.nodes[d->node];
		if (d->capture == 1) {
			d->target = hit;
			d->side = group >= 0 ? fuse_dock_target(d, group, e->x, e->y) : FUSE_DOCK_FLOAT;
		} else if (d->dragging) {
			r = d->start_box;
			if (d->capture == 2 || d->capture == 3) {
				n->rect = (FuseDockRect){(r.x - d->host.x) / fmaxf(1, d->host.w),
					(r.y - d->host.y) / fmaxf(1, d->host.h), r.w / fmaxf(1, d->host.w), r.h / fmaxf(1, d->host.h)};
				if (d->capture == 2) {
					n->rect.x += dx / fmaxf(1, d->host.w);
					n->rect.y += dy / fmaxf(1, d->host.h);
				} else {
					n->rect.w = fmaxf(0.01f, n->rect.w + dx / fmaxf(1, d->host.w));
					n->rect.h = fmaxf(0.01f, n->rect.h + dy / fmaxf(1, d->host.h));
				}
			} else {
				fuse_dock_min(d, n->a, &aw, &ah);
				fuse_dock_min(d, n->b, &bw, &bh);
				length = fmaxf(0, (n->kind == 2 ? r.w : r.h) - 4);
				cut = n->kind == 2 ? e->x - r.x : e->y - r.y;
				if (length >= (n->kind == 2 ? aw + bw : ah + bh))
					cut = fmaxf(n->kind == 2 ? aw : ah, fminf(cut, length - (n->kind == 2 ? bw : bh)));
				n->ratio = fmaxf(0.01f, fminf(0.99f, cut / fmaxf(1, length)));
			}
			fuse_dock_layout(d, d->host);
		}
		if (e->release) {
			if (!fuse_dock_contains(d->host, e->x, e->y))
				fuse_dock_cancel(d);
			else if (d->dragging && d->capture == 1) {
				if (group == d->node && e->y < d->boxes[group].y + FUSE_DOCK_TITLE) {
					n = &d->layout.nodes[group];
					tab = (int)((e->x - d->boxes[group].x) / fmaxf(1, (d->boxes[group].w - (fuse_dock_float_root(d, group) >= 0 ? fminf(24, d->boxes[group].w * 0.25f) : 0)) / n->count));
					tab = tab < 0 ? 0 : tab >= n->count ? n->count - 1 : tab;
					for (i = 0; n->tabs[i] != d->window; i++) {}
					if (i != tab) d->revision++;
					while (i < tab) { n->tabs[i] = n->tabs[i + 1]; i++; }
					while (i > tab) { n->tabs[i] = n->tabs[i - 1]; i--; }
					n->tabs[tab] = d->window;
					n->active = tab;
				} else {
					d->capture = 0;
					if (!fuse_dock_place(d, d->window, d->target, d->side)) {
						d->capture = 1;
						fuse_dock_cancel(d);
					} else if (d->side == FUSE_DOCK_FLOAT) {
						n = &d->layout.nodes[fuse_dock_group(d, d->window)];
						n->rect.x = (e->x - d->host.x) / fmaxf(1, d->host.w);
						n->rect.y = (e->y - d->host.y) / fmaxf(1, d->host.h);
					}
				}
			} else if (d->dragging) {
				if (d->capture == 2 || d->capture == 3) {
					r = d->boxes[d->node];
					n->rect = (FuseDockRect){(r.x - d->host.x) / fmaxf(1, d->host.w),
						(r.y - d->host.y) / fmaxf(1, d->host.h), r.w / fmaxf(1, d->host.w), r.h / fmaxf(1, d->host.h)};
				}
				d->revision++;
			}
			d->capture = 0;
			fuse_dock_layout(d, d->host);
		}
		return 1;
	}
	if (!e->press)
		return (group >= 0 && e->y < d->boxes[group].y + FUSE_DOCK_TITLE) || (top >= 0 && group < 0);
	if (!fuse_dock_contains(d->host, e->x, e->y)) return 0;
	before = d->layout;
	focus_before = d->focused;
	tabs_before = d->tab_focus;
	d->tab_focus = 0;
	if (group >= 0) {
		n = &d->layout.nodes[group];
		r = d->boxes[group];
		d->focused = hit;
		floating = fuse_dock_float_root(d, group);
		grip = floating >= 0 ? fminf(24, r.w * 0.25f) : 0;
		if (floating >= 0 && d->layout.nodes[floating].order != FUSE_DOCK_MAX) {
			fuse_dock_raise(d, floating);
			d->revision++;
		}
		if (e->y < r.y + FUSE_DOCK_TITLE) {
			d->tab_focus = 1;
			d->capture = floating >= 0 && e->x >= r.x + r.w - grip ? 2 : 1;
			tab = (int)((e->x - r.x) / fmaxf(1, (r.w - grip) / n->count));
			if (tab >= n->count) tab = n->count - 1;
			if (d->capture == 1 && n->active != tab) {
				n->active = tab;
				d->revision++;
			}
			d->focused = n->tabs[n->active];
		} else if (floating >= 0) {
			r = d->boxes[floating];
			if (e->x >= r.x + r.w - 12 && e->y >= r.y + r.h - 12)
				d->capture = 3;
		}
	}
	if (!d->capture) {
		for (i = 0; i < FUSE_DOCK_NODES; i++) {
			n = &d->layout.nodes[i];
			if (n->kind < 2 || fuse_dock_float_root(d, i) != top) continue;
			a = d->boxes[n->a];
			r = d->boxes[i];
			if (n->kind == 2) { r.x = a.x + a.w; r.w = 4; }
			else { r.y = a.y + a.h; r.h = 4; }
			if (fuse_dock_contains(r, e->x, e->y)) {
				group = i;
				d->capture = 4;
				break;
			}
		}
	}
	if (!d->capture) return top >= 0 && group < 0;
	d->rollback = before;
	d->rollback_focus = focus_before;
	d->rollback_tab_focus = tabs_before;
	d->node = d->capture == 2 || d->capture == 3 ? fuse_dock_float_root(d, group) : group;
	d->window = d->focused;
	d->start_box = d->boxes[d->node];
	d->start_x = e->x;
	d->start_y = e->y;
	d->dragging = 0;
	return 1;
}

int
fuse_dock_busy(const FuseDock *d) { return d->capture != 0; }
int
fuse_dock_revision(const FuseDock *d) { return d->revision; }
int
fuse_dock_focused(const FuseDock *d) { return d->focused; }
int
fuse_dock_focus_tabs(FuseDock *d, int window)
{
	FuseDockNode *n;
	int group, i, floating;
	if (!d || d->capture || window < 0 || window >= d->count) return 0;
	group = fuse_dock_group(d, window);
	if (group < 0) return 0;
	n = &d->layout.nodes[group];
	for (i = 0; n->tabs[i] != window; i++) {}
	if (n->active != i) { n->active = i; d->revision++; }
	floating = fuse_dock_float_root(d, group);
	if (floating >= 0 && d->layout.nodes[floating].order != FUSE_DOCK_MAX) {
		fuse_dock_raise(d, floating);
		d->revision++;
	}
	d->focused = window;
	d->tab_focus = 1;
	return 1;
}

void
fuse_dock_export(const FuseDock *d, FuseDockLayout *l) { *l = d->layout; }

int
fuse_dock_check(const FuseDock *d, const FuseDockLayout *l, int node, int floating, unsigned *seen, unsigned *windows)
{
	const FuseDockNode *n;
	int i, window;
	if (node < 0 || node >= FUSE_DOCK_NODES || (*seen & (1u << node))) return 0;
	*seen |= 1u << node;
	n = &l->nodes[node];
	if (n->kind < 1 || n->kind > 3 || n->floating != floating) return 0;
	if (floating && (!isfinite(n->rect.x) || !isfinite(n->rect.y) || !isfinite(n->rect.w) || !isfinite(n->rect.h) ||
		n->rect.x < 0 || n->rect.x > 1 || n->rect.y < 0 || n->rect.y > 1 || n->rect.w <= 0 || n->rect.w > 1 || n->rect.h <= 0 || n->rect.h > 1 || n->order < 0 || n->order > FUSE_DOCK_MAX)) return 0;
	if (n->kind > 1) {
		if (!isfinite(n->ratio) || n->ratio <= 0 || n->ratio >= 1) return 0;
		return fuse_dock_check(d, l, n->a, 0, seen, windows) && fuse_dock_check(d, l, n->b, 0, seen, windows);
	}
	if (n->count < 1 || n->count > d->count || n->active < 0 || n->active >= n->count) return 0;
	for (i = 0; i < n->count; i++) {
		window = n->tabs[i];
		if (window < 0 || window >= d->count || (*windows & (1u << window))) return 0;
		*windows |= 1u << window;
	}
	return 1;
}

int
fuse_dock_import(FuseDock *d, const FuseDockLayout *l)
{
	unsigned seen = 0, windows = 0, orders = 0;
	int i;
	if (!d || !l || l->root < -1 || l->root >= FUSE_DOCK_NODES) return 0;
	if (l->root >= 0 && !fuse_dock_check(d, l, l->root, 0, &seen, &windows)) return 0;
	for (i = 0; i < FUSE_DOCK_NODES; i++) {
		if (!l->nodes[i].kind || !l->nodes[i].floating) continue;
		if (!fuse_dock_check(d, l, i, 1, &seen, &windows)) return 0;
		if (orders & (1u << l->nodes[i].order)) return 0;
		orders |= 1u << l->nodes[i].order;
	}
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (l->nodes[i].kind && !(seen & (1u << i))) return 0;
	if (windows != (1u << d->count) - 1) return 0;
	d->layout = *l;
	d->capture = 0;
	d->focused = fuse_dock_visible(d, 0);
	d->tab_focus = 0;
	d->revision++;
	fuse_dock_layout(d, d->host);
	return 1;
}

int
fuse_dock_begin(FuseDock *d, FuseCanvas canvas, int window, const FuseClass *cls)
{
	FuseDockRect r;
	if (!fuse_dock_content(d, window, &r)) return 0;
	fuse_id(canvas, d->windows[window].name);
	fuse_div_begin(canvas, r.x, r.y, r.w, r.h, cls);
	fuse_div_scope(canvas);
	return 1;
}

void
fuse_dock_end(FuseCanvas canvas) { fuse_div_end(canvas); }

void
fuse_dock_chrome(FuseDock *d, FuseCanvas canvas, int window,
	uint32_t background, uint32_t inactive, uint32_t active, uint32_t text,
	uint32_t grip, uint32_t accent)
{
	FuseDockRect r;
	FuseDockNode *n;
	float width;
	int i, floating;
	if (!fuse_dock_group_rect(d, window, &r) || r.w <= 0 || r.h <= 0) return;
	n = &d->layout.nodes[fuse_dock_group(d, window)];
	floating = fuse_dock_float_root(d, fuse_dock_group(d, window)) >= 0;
	fuse_div_begin(canvas, r.x, r.y, r.w, r.h, NULL);
	fuse_rect(canvas, 0, 0, r.w, r.h, background);
	width = fmaxf(1, (r.w - (floating ? fminf(24, r.w * 0.25f) : 0)) / n->count);
	for (i = 0; i < n->count; i++) {
		fuse_rect(canvas, i * width, 0, width - 1, FUSE_DOCK_TITLE, i == n->active ? active : inactive);
		fuse_div_begin(canvas, i * width, 0, width, FUSE_DOCK_TITLE, NULL);
		if (accent && i == n->active) {
			fuse_rect(canvas, 0, FUSE_DOCK_TITLE - 1, width - 1, 1, accent);
			if (d->tab_focus && d->focused == window) {
				fuse_rect(canvas, 0, 0, width - 1, 2, accent);
				fuse_rect(canvas, 0, 0, 2, FUSE_DOCK_TITLE, accent);
				fuse_rect(canvas, width - 3, 0, 2, FUSE_DOCK_TITLE, accent);
				fuse_rect(canvas, 0, FUSE_DOCK_TITLE - 2, width - 1, 2, accent);
			}
		}
		fuse_text(canvas, 6, 8, 1, d->windows[n->tabs[i]].title, text);
		fuse_div_end(canvas);
	}
	if (floating) {
		fuse_text(canvas, r.w - 19, 8, 1, "::", text);
		fuse_rect(canvas, r.w - 10, r.h - 3, 8, 2, grip);
	}
	fuse_div_end(canvas);
}

void
fuse_dock_feedback(FuseDock *d, FuseCanvas canvas, uint32_t color)
{
	FuseDockRect r;
	int group;
	if (d->capture != 1 || !d->dragging) return;
	group = fuse_dock_group(d, d->target);
	if (group < 0 || d->host.w <= 0 || d->host.h <= 0 ||
		!fuse_dock_can_split(d, d->window, group, d->side)) return;
	if (group == d->node && d->layout.nodes[group].count == 1) return;
	r = d->boxes[group];
	if (d->side == FUSE_DOCK_LEFT || d->side == FUSE_DOCK_RIGHT) {
		r.w *= 0.5f;
		if (d->side == FUSE_DOCK_RIGHT) r.x += r.w;
	} else if (d->side == FUSE_DOCK_TOP || d->side == FUSE_DOCK_BOTTOM) {
		r.h *= 0.5f;
		if (d->side == FUSE_DOCK_BOTTOM) r.y += r.h;
	}
	if (r.w <= 0 || r.h <= 0) return;
	fuse_rect(canvas, r.x, r.y, r.w, 3, color);
	fuse_rect(canvas, r.x, r.y + r.h - 3, r.w, 3, color);
	fuse_rect(canvas, r.x, r.y, 3, r.h, color);
	fuse_rect(canvas, r.x + r.w - 3, r.y, 3, r.h, color);
}
