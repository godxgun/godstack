/* Copyright (c) 2026 Vasco Alves. Caller-backed, bounded docking layouts. */
#ifndef FUSE_DOCK_H
#define FUSE_DOCK_H
#include "fuse.h"

#define FUSE_DOCK_MAX FUSE_WINDOWS_MAX
#define FUSE_DOCK_NODES (2 * FUSE_DOCK_MAX - 1)
#define FUSE_DOCK_TITLE 24.0f

typedef struct FuseDock FuseDock;
typedef struct FuseDockRect { float x, y, w, h; } FuseDockRect;
typedef enum {
	FUSE_DOCK_CENTER, FUSE_DOCK_LEFT, FUSE_DOCK_RIGHT,
	FUSE_DOCK_TOP, FUSE_DOCK_BOTTOM, FUSE_DOCK_FLOAT
} FuseDockSide;
/* Node kind: 0 unused, 1 group, 2 horizontal children, 3 vertical children.
 * Group tabs are registration indices; file formats must map stable names.
 * Floating group rectangles are fractions of the host, not pixels. */
typedef struct FuseDockNode {
	int kind, a, b, count, active, floating, order;
	int tabs[FUSE_DOCK_MAX];
	float ratio;
	FuseDockRect rect;
} FuseDockNode;
typedef struct FuseDockLayout {
	int root;
	FuseDockNode nodes[FUSE_DOCK_NODES];
} FuseDockLayout;
/* Pointer coordinates are absolute host coordinates. cancel covers Escape,
 * focus loss and modal activation. previous/next automatically activate tabs
 * only after a tab/title click or fuse_dock_focus_tabs; content clicks leave
 * keys to the panel.
 * activate consumes Enter/Space without changing an already selected tab. */
typedef struct FuseDockEvent {
	float x, y;
	int press, release, cancel, previous, next, activate;
} FuseDockEvent;

size_t fuse_dock_memory(void);
FuseDock *fuse_dock_create(void *memory, size_t size, int capacity);
/* Returns registration index, or -1. Names/titles are copied, 31/63 bytes max. */
int fuse_dock_register(FuseDock *, const char *name, const char *title, float min_w, float min_h);
const char *fuse_dock_name(const FuseDock *, int window);
const char *fuse_dock_title(const FuseDock *, int window);
void fuse_dock_reset(FuseDock *);
/* Returns 0 without mutation for invalid targets/minima or while captured.
 * A zero-by-zero host permits constructing defaults before host sizing.
 * Cancel any pending gesture before programmatically changing placement. */
int fuse_dock_place(FuseDock *, int window, int target, FuseDockSide side);
/* Host changes cancel a captured gesture and retain its prepress arrangement. */
void fuse_dock_layout(FuseDock *, FuseDockRect host);
/* Returns 1 for consumed input, including the terminating release/cancel. */
int fuse_dock_event(FuseDock *, const FuseDockEvent *);
int fuse_dock_busy(const FuseDock *);
int fuse_dock_revision(const FuseDock *);
int fuse_dock_focused(const FuseDock *);
/* Give tabs keyboard focus (e.g. host Tab navigation), activating window.
 * Returns 0 for invalid windows or during capture. Does not reorder tabs. */
int fuse_dock_focus_tabs(FuseDock *, int window);
int fuse_dock_hit(const FuseDock *, float x, float y);
/* Visible windows in paint order; -1 after the last. */
int fuse_dock_visible(const FuseDock *, int position);
int fuse_dock_content(const FuseDock *, int window, FuseDockRect *);
int fuse_dock_group_rect(const FuseDock *, int window, FuseDockRect *);
void fuse_dock_export(const FuseDock *, FuseDockLayout *);
/* Every registered window must occur exactly once; float orders must be unique.
 * Rejects without mutation, including focus/capture. Success ends capture. */
int fuse_dock_import(FuseDock *, const FuseDockLayout *);
/* Builder takes absolute host coordinates and clips content. Its named root
 * scopes explicitly named children and keeps anonymous child IDs stable across
 * placement and emission order. Outside building, enter the registered window
 * name with fuse_scope_enter in the same enclosing scope for focus/edit queries. */
int fuse_dock_begin(FuseDock *, FuseCanvas, int window, const FuseClass *);
void fuse_dock_end(FuseCanvas);
/* Presentation colors are per-call widget arguments, never retained.
 * accent draws a 1px active-tab underline and a 2px keyboard-focus outline;
 * pass zero to omit those decorations. Layout/input geometry is unchanged. */
void fuse_dock_chrome(FuseDock *, FuseCanvas, int window,
	uint32_t background, uint32_t inactive, uint32_t active, uint32_t text,
	uint32_t grip, uint32_t accent);
void fuse_dock_feedback(FuseDock *, FuseCanvas, uint32_t color);
#endif
