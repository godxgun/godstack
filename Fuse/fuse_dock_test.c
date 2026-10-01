/* Copyright (c) 2026 Vasco Alves. Included by fuse_test.c. */
#include "fuse_dock.c"

static void test_dock(void);
static void test_dock_interaction(void);
static void test_dock_regressions(void);
static void test_dock_constrained_builder(FuseDock *d);

void
test_dock(void)
{
	FuseDock d, copy;
	FuseDockLayout layout, before;
	FuseDockRect a, b;
	int side, i, window, target;
	unsigned random = 7;

	expect(!fuse_dock_create(&d, sizeof(d) - 1, 3), "dock short backing rejected");
	expect(fuse_dock_create(&d, sizeof(d), 3) == &d, "dock caller backing");
	expect(fuse_dock_register(&d, "a", "A", 40, 40) == 0, "dock register A");
	expect(fuse_dock_register(&d, "b", "B", 40, 40) == 1, "dock register B");
	expect(fuse_dock_register(&d, "b", "Duplicate", 40, 40) < 0, "dock duplicate rejected");
	expect(fuse_dock_register(&d, "c", "C", 40, 40) == 2, "dock register C");
	expect(fuse_dock_register(&d, "d", "D", 40, 40) < 0, "dock registration capacity");
	for (side = FUSE_DOCK_LEFT; side <= FUSE_DOCK_BOTTOM; side++) {
		fuse_dock_reset(&d);
		fuse_dock_layout(&d, (FuseDockRect){10, 20, 800, 600});
		expect(fuse_dock_place(&d, 1, 0, side), "dock edge placement");
		expect(fuse_dock_content(&d, 1, &b), "incoming visible");
		/* A and C started tabbed; center placement activates A. */
		expect(fuse_dock_place(&d, 0, 2, FUSE_DOCK_CENTER), "dock tab activation");
		expect(fuse_dock_content(&d, 0, &a), "target visible");
		expect(side == FUSE_DOCK_LEFT ? b.x < a.x : side == FUSE_DOCK_RIGHT ? b.x > a.x :
			side == FUSE_DOCK_TOP ? b.y < a.y : b.y > a.y, "dock split orientation");
		fuse_dock_export(&d, &layout);
		fuse_dock_create(&copy, sizeof(copy), 3);
		fuse_dock_register(&copy, "a", "A", 40, 40);
		fuse_dock_register(&copy, "b", "B", 40, 40);
		fuse_dock_register(&copy, "c", "C", 40, 40);
		expect(fuse_dock_import(&copy, &layout), "dock round trip");
		fuse_dock_layout(&copy, (FuseDockRect){0, 0, 1600, 1200});
		expect(fuse_dock_content(&copy, 0, &a) && a.w > 700, "dock scales with host");
		before = copy.layout;
		layout.nodes[layout.root].a = layout.root;
		expect(!fuse_dock_import(&copy, &layout), "dock cyclic import rejected");
		expect(!memcmp(&copy.layout, &before, sizeof(before)), "failed import transactional");
	}
	fuse_dock_reset(&d);
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 10, 10});
	before = d.layout;
	expect(!fuse_dock_place(&d, 1, 0, FUSE_DOCK_LEFT), "dock impossible minima rejected");
	expect(!memcmp(&d.layout, &before, sizeof(before)), "failed drop transactional");
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 1200, 900});
	expect(fuse_dock_place(&d, 1, 0, FUSE_DOCK_FLOAT), "dock float");
	expect(fuse_dock_place(&d, 2, 1, FUSE_DOCK_LEFT), "dock splits floating target");
	expect(fuse_dock_place(&d, 2, 0, FUSE_DOCK_CENTER), "dock collapse floating split");
	/* Exercise many topology changes, including self drops and empty roots. */
	for (i = 0; i < 2000; i++) {
		random = random * 1664525u + 1013904223u;
		window = random % 3;
		target = (random >> 8) % 3;
		side = (random >> 16) % 6;
		fuse_dock_place(&d, window, target, side);
		fuse_dock_export(&d, &layout);
		if (!fuse_dock_import(&d, &layout)) {
			expect(0, "dock randomized topology remains importable");
			break;
		}
	}
}

void
test_dock_interaction(void)
{
	FuseDock d;
	FuseDockLayout before;
	FuseDockEvent event = {0};
	FuseDockRect r;
	fuse_dock_create(&d, sizeof(d), 2);
	fuse_dock_register(&d, "a", "A", 30, 30);
	fuse_dock_register(&d, "b", "B", 30, 30);
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 800, 600});
	before = d.layout;
	event = (FuseDockEvent){10, 10, 1, 0, 0, 0, 0, 0};
	expect(fuse_dock_event(&d, &event), "dock title press consumed");
	event = (FuseDockEvent){600, 400, 0, 0, 0, 0, 0, 0};
	expect(fuse_dock_event(&d, &event) && fuse_dock_busy(&d), "dock drag capture");
	expect(d.layout.nodes[d.layout.root].count == 2, "drag preview does not publish topology");
	event.cancel = 1;
	expect(fuse_dock_event(&d, &event) && !fuse_dock_busy(&d), "dock cancellation consumed");
	expect(!memcmp(&d.layout, &before, sizeof(before)), "dock cancel retains topology");
	event = (FuseDockEvent){10, 10, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &event);
	event = (FuseDockEvent){900, 650, 0, 1, 0, 0, 0, 0};
	expect(fuse_dock_event(&d, &event), "outside-host release consumed");
	expect(!memcmp(&d.layout, &before, sizeof(before)), "outside-host drop cancels");
	event = (FuseDockEvent){10, 10, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &event);
	event.press = 0; event.release = 1;
	fuse_dock_event(&d, &event);
	event = (FuseDockEvent){0}; event.next = 1;
	expect(fuse_dock_event(&d, &event), "keyboard tab next");
	expect(fuse_dock_content(&d, 1, &r) && !fuse_dock_content(&d, 0, &r), "hidden tab no content");
	test_dock_constrained_builder(&d);
	test_dock_regressions();
}

void
test_dock_regressions(void)
{
	FuseDock d;
	FuseDockLayout before, candidate;
	FuseDockRect r, a, b;
	FuseDockEvent e = {0};
	FuseCanvas c;
	uint32_t child_id;
	size_t command_count;
	void *memory;
	int group, root, i, revision, focused;

	fuse_dock_create(&d, sizeof(d), 3);
	fuse_dock_register(&d, "a", "A", 200, 40);
	fuse_dock_register(&d, "b", "B", 20, 40);
	fuse_dock_register(&d, "c", "C", 20, 40);
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 230, 400});
	expect(fuse_dock_place(&d, 0, 1, FUSE_DOCK_LEFT), "split excludes incoming minimum from source group");
	group = fuse_dock_group(&d, 1);
	expect(d.layout.nodes[group].tabs[d.layout.nodes[group].active] == 2, "removing earlier tab preserves active identity");
	before = d.layout;
	expect(!fuse_dock_place(&d, 0, 99, FUSE_DOCK_CENTER), "unknown target rejected");
	expect(!memcmp(&before, &d.layout, sizeof(before)), "unknown target leaves layout unchanged");
	fuse_dock_layout(&d, (FuseDockRect){10, 20, 1, 1});
	for (i = 0; i < 3; i++) {
		if (!fuse_dock_group_rect(&d, i, &r)) continue;
		expect(r.x >= 10 && r.y >= 20 && r.x + r.w <= 11 && r.y + r.h <= 21, "tiny host does not push split children outside host");
	}

	/* A floating group covers the docked splitter, including pointer presses. */
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 800, 600});
	fuse_dock_place(&d, 2, -1, FUSE_DOCK_FLOAT);
	root = d.layout.root;
	a = d.boxes[d.layout.nodes[root].a];
	group = fuse_dock_group(&d, 2);
	d.layout.nodes[group].rect = (FuseDockRect){0.3f, 0.1f, 0.4f, 0.7f};
	fuse_dock_layout(&d, d.host);
	e = (FuseDockEvent){a.x + a.w + 2, 150, 1, 0, 0, 0, 0, 0};
	expect(!fuse_dock_event(&d, &e) && !fuse_dock_busy(&d), "covered splitter does not capture through float content");
	/* Split this float; its own gap must also cover lower panel input. */
	expect(fuse_dock_place(&d, 1, 2, FUSE_DOCK_LEFT), "floating split created");
	group = fuse_dock_float_root(&d, fuse_dock_group(&d, 2));
	a = d.boxes[d.layout.nodes[group].a];
	e.x = a.x + a.w + 2; e.y = a.y + 100;
	expect(fuse_dock_hit(&d, e.x, e.y) == -1, "floating splitter gap occludes underlying content");
	expect(fuse_dock_event(&d, &e) && d.node == group, "top floating splitter captures");
	before = d.rollback;
	e.press = 0; e.x += 80;
	fuse_dock_event(&d, &e);
	e.cancel = 1;
	expect(fuse_dock_event(&d, &e) && !memcmp(&before, &d.layout, sizeof(before)), "splitter cancellation rolls back ratio");

	/* Reject invalid imports while a captured interaction stays intact. */
	fuse_dock_group_rect(&d, 2, &r);
	e = (FuseDockEvent){r.x + 5, r.y + 5, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &e);
	before = d.layout; candidate = before;
	revision = d.revision; focused = d.focused;
	candidate.nodes[group].ratio = NAN;
	expect(!fuse_dock_import(&d, &candidate) && fuse_dock_busy(&d), "bad import preserves capture");
	expect(d.revision == revision && d.focused == focused && !memcmp(&d.layout, &before, sizeof(before)), "bad import preserves focus revision and layout");
	e.cancel = 1; e.press = 0;
	fuse_dock_event(&d, &e);

	/* Content owns arrow keys; title activation owns tab navigation. */
	fuse_dock_reset(&d);
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 800, 600});
	e = (FuseDockEvent){10, 100, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &e);
	e = (FuseDockEvent){0}; e.next = 1;
	expect(!fuse_dock_event(&d, &e), "content focus does not steal arrows for tabs");
	expect(fuse_dock_focus_tabs(&d, 0), "host can give tabs keyboard focus without pointer input");
	expect(fuse_dock_event(&d, &e) && d.focused == 1, "keyboard focus API enables arrow activation");
	expect(!fuse_dock_focus_tabs(&d, 99), "keyboard focus rejects missing window");
	e = (FuseDockEvent){10, 10, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &e);
	e.press = 0; e.release = 1;
	fuse_dock_event(&d, &e);
	e = (FuseDockEvent){0}; e.previous = 1;
	expect(fuse_dock_event(&d, &e) && d.focused == 2, "keyboard tabs wrap previous");
	revision = d.revision;
	e.previous = 0; e.activate = 1;
	expect(fuse_dock_event(&d, &e) && d.revision == revision, "activating selected tab is a layout no-op");

	/* Anonymous child IDs derive from the named window, not paint order. */
	memory = calloc(1, fuse_canvas_memory(64));
	c = fuse_canvas_create(memory, fuse_canvas_memory(64));
	fuse_canvas_resize(c, 800, 600);
	fuse_dock_begin(&d, c, 2, NULL);
	fuse_button(c, 0, 0, 20, 20, 1, 2);
	child_id = c->elements[c->element_count - 1].id;
	fuse_dock_end(c);
	fuse_canvas_draw(c, &command_count);
	fuse_canvas_clear(c);
	fuse_dock_place(&d, 2, -1, FUSE_DOCK_FLOAT);
	fuse_div_begin(c, 0, 0, 1, 1, NULL); fuse_div_end(c);
	fuse_dock_chrome(&d, c, 2);
	fuse_dock_begin(&d, c, 2, NULL);
	fuse_button(c, 0, 0, 20, 20, 1, 2);
	expect(c->elements[c->element_count - 1].id == child_id, "child identity survives floating and emission order changes");
	fuse_dock_end(c);
	fuse_canvas_draw(c, &command_count);
	free(memory);

	/* Clamped floating geometry is the starting point, not off-host intent. */
	group = fuse_dock_group(&d, 2);
	d.layout.nodes[group].rect = (FuseDockRect){0.95f, 0.95f, 0.5f, 0.5f};
	fuse_dock_layout(&d, d.host);
	fuse_dock_group_rect(&d, 2, &r);
	e = (FuseDockEvent){r.x + r.w - 5, r.y + 5, 1, 0, 0, 0, 0, 0};
	before = d.layout;
	fuse_dock_event(&d, &e);
	e.press = 0; e.x -= 20;
	fuse_dock_event(&d, &e);
	fuse_dock_group_rect(&d, 2, &b);
	expect(fabsf(b.x - (r.x - 20)) < 0.01f, "clamped float movement starts at visible rectangle");
	e.cancel = 1;
	fuse_dock_event(&d, &e);
	expect(!memcmp(&before, &d.layout, sizeof(before)), "float move cancellation restores prepress stacking and rectangle");

	fuse_dock_group_rect(&d, 2, &r);
	e = (FuseDockEvent){r.x + r.w - 2, r.y + r.h - 2, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &e);
	e.press = 0; e.x = r.x + 1; e.y = r.y + 1;
	fuse_dock_event(&d, &e);
	fuse_dock_group_rect(&d, 2, &b);
	expect(b.w >= 20 && b.h >= 40 + FUSE_DOCK_TITLE, "float resize enforces content minima and chrome");
	e.release = 1;
	fuse_dock_event(&d, &e);
	fuse_dock_export(&d, &candidate);
	expect(fuse_dock_import(&d, &candidate), "committed float resize remains importable");

	fuse_dock_group_rect(&d, 2, &r);
	e = (FuseDockEvent){r.x + r.w - 1, r.y + 2, 1, 0, 0, 0, 0, 0};
	before = d.layout; focused = d.focused;
	fuse_dock_event(&d, &e);
	e.press = 0; e.x -= 20;
	fuse_dock_event(&d, &e);
	expect(!fuse_dock_place(&d, 0, 2, FUSE_DOCK_CENTER), "programmatic placement rejects active gesture");
	fuse_dock_layout(&d, (FuseDockRect){10, 10, 500, 400});
	expect(!fuse_dock_busy(&d) && d.focused == focused && !memcmp(&before, &d.layout, sizeof(before)), "host resize cancels gesture without changing layout intent");

	/* Multiple floating roots cannot share ambiguous stacking identities. */
	fuse_dock_place(&d, 1, -1, FUSE_DOCK_FLOAT);
	fuse_dock_export(&d, &candidate); before = d.layout;
	candidate.nodes[fuse_dock_group(&d, 1)].order = candidate.nodes[fuse_dock_group(&d, 2)].order;
	expect(!fuse_dock_import(&d, &candidate) && !memcmp(&before, &d.layout, sizeof(before)), "duplicate float stacking order rejected transactionally");

	/* Invalid split previews emit no targets and rejected drops restore selection. */
	fuse_dock_reset(&d);
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 40, 80});
	before = d.layout;
	e = (FuseDockEvent){2, 2, 1, 0, 0, 0, 0, 0};
	fuse_dock_event(&d, &e);
	e.press = 0; e.x = 2; e.y = 60;
	fuse_dock_event(&d, &e);
	memory = calloc(1, fuse_canvas_memory(64));
	c = fuse_canvas_create(memory, fuse_canvas_memory(64));
	fuse_canvas_resize(c, 40, 80);
	fuse_dock_feedback(&d, c);
	fuse_canvas_draw(c, &command_count);
	expect(command_count == 0, "impossible split does not advertise feedback");
	free(memory);
	e.release = 1;
	fuse_dock_event(&d, &e);
	expect(!memcmp(&before, &d.layout, sizeof(before)), "impossible drop restores prepress tab selection");

	/* The maximum valid tree bounds traversal and node-slot pressure. */
	fuse_dock_create(&d, sizeof(d), FUSE_DOCK_MAX);
	for (i = 0; i < FUSE_DOCK_MAX; i++) {
		char name[32];
		snprintf(name, sizeof(name), "panel%d", i);
		expect(fuse_dock_register(&d, name, name, 1, 1) == i, "maximum dock registration");
	}
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 100000000, 100000000});
	for (i = 0; i < FUSE_DOCK_MAX - 1; i++)
		expect(fuse_dock_place(&d, i, FUSE_DOCK_MAX - 1, FUSE_DOCK_LEFT), "maximum-depth split placement");
	group = 0;
	for (i = 0; i < FUSE_DOCK_NODES; i++) group += d.layout.nodes[i].kind != 0;
	expect(group == FUSE_DOCK_NODES, "maximum tree uses every bounded node slot");
	fuse_dock_export(&d, &candidate);
	expect(fuse_dock_import(&d, &candidate), "maximum-depth import validates");
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 0, 0});
	expect(fuse_dock_hit(&d, 0, 0) == -1, "zero host receives no panel hits");
	fuse_dock_layout(&d, (FuseDockRect){0, 0, 1, 1});
	for (i = 0; i < FUSE_DOCK_MAX; i++) {
		expect(fuse_dock_group_rect(&d, i, &r) && r.x + r.w <= 1 && r.y + r.h <= 1,
			"maximum tree clips to tiny host without losing topology");
	}
	test_dock_constrained_builder(&d);
}

void
test_dock_constrained_builder(FuseDock *d)
{
	const FuseDockRect hosts[] = {
		{0, 0, 100, 20}, {0, 0, 0, 600}, {0, 0, 800, 0},
		{0, 0, 0, 0}, {0, 0, 1, 1}, {0, 0, 100000000, 100000000}
	};
	FuseDockLayout before = d->layout;
	FuseDockRect group = {0}, content = {0}, host = d->host;
	FuseCanvas c;
	void *memory;
	size_t command_count;
	uint32_t elements;
	int i, j, window, visible, revision = d->revision, focused = d->focused;

	memory = calloc(1, fuse_canvas_memory(8192));
	c = fuse_canvas_create(memory, fuse_canvas_memory(8192));
	/* Keep the canvas nonempty so dock guards, rather than canvas clipping,
	 * are responsible for skipping empty geometry. */
	fuse_canvas_resize(c, 800, 600);
	for (i = 0; i < (int)(sizeof(hosts) / sizeof(hosts[0])); i++) {
		fuse_dock_layout(d, hosts[i]);
		fuse_canvas_clear(c);
		visible = 0;
		for (j = 0; (window = fuse_dock_visible(d, j)) >= 0; j++) {
			visible++;
			expect(fuse_dock_group_rect(d, window, &group), "constrained group remains visible");
			expect(fuse_dock_content(d, window, &content) == (group.w > 0 && group.h > FUSE_DOCK_TITLE),
				"constrained content rejects empty rectangles");
			elements = c->element_count;
			if (fuse_dock_begin(d, c, window, NULL)) {
				expect(content.w > 0 && content.h > 0, "constrained builder opens only nonempty content");
				fuse_dock_end(c);
			} else {
				expect(c->element_count == elements && c->screen_count == 1,
					"empty content does not change builder state");
			}
			elements = c->element_count;
			fuse_dock_chrome(d, c, window);
			if (group.w <= 0 || group.h <= 0)
				expect(c->element_count == elements, "empty group emits no chrome");
			else
				expect(c->element_count > elements, "nonempty group retains chrome even without content");

			/* White-box preview: collapsed groups cannot be pointer targets,
			 * but feedback must independently reject their empty rectangles. */
			d->capture = 1; d->dragging = 1; d->node = -1;
			d->window = window; d->target = window; d->side = FUSE_DOCK_CENTER;
			elements = c->element_count;
			fuse_dock_feedback(d, c);
			if (group.w <= 0 || group.h <= 0)
				expect(c->element_count == elements, "empty target emits no feedback");
			d->capture = 0; d->dragging = 0;
		}
		expect(visible > 0, "constrained layout retains visible windows");
		fuse_canvas_draw(c, &command_count);
		expect(get_error(c) == FUSE_ERR_OK, "constrained dock canvas stays balanced and valid");
		expect(d->revision == revision && d->focused == focused && !memcmp(&before, &d->layout, sizeof(before)),
			"constrained rendering preserves layout focus and revision");
	}
	fuse_dock_layout(d, host);
	free(memory);
}
