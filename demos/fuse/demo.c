/* Copyright (c) 2026 Vasco Alves. Headless editor frames with caller-owned state. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuse.h"
#include "fuse_dock.h"
#include "fuse.c"
#include "fuse_dock.c"

#define DEMO_FRAMES 600
#define DEMO_PANELS 4

typedef struct DemoPanel {
	const char *name, *title;
	char text[64];
	int caret;
	float gain, scroll;
} DemoPanel;

static void demo_panel_build(FuseCanvas canvas, DemoPanel *panel, size_t *actions);
static int demo_commands_consume(FuseCanvas canvas, const FuseCmd *commands, size_t count, uint64_t *fingerprint);

void
demo_panel_build(FuseCanvas canvas, DemoPanel *panel, size_t *actions)
{
	/* Styles are borrowed until draw, so this survives the panel builder. */
	static const FuseClass rows = {
		.direction = FUSE_DIRECTION_COLUMN, .gap = 3,
		.width_sizing = FUSE_SIZING_GROW, .height_sizing = FUSE_SIZING_FIXED,
		.height = 22
	};
	int row;

	fuse_text(canvas, 0, 0, 1, panel->title, 0xFFFFFFFFu);
	fuse_id(canvas, "value");
	fuse_textbox(canvas, 0, 0, 180, 24, 4, 4, panel->text, sizeof(panel->text),
		&panel->caret, "Label", 12, 0xFF202020u, 0xFF402020u,
		0xFFFFFFFFu, 0xFFAAAAAAu, 1, NULL, NULL);
	fuse_id(canvas, "gain");
	fuse_slider(canvas, 0, 0, 180, 20, 0xFF444444u, 0xFFAA3333u, &panel->gain);
	fuse_id(canvas, "apply");
	if (fuse_button_text(canvas, "Apply", 0, 0, 80, 24, 0xFF555555u, 0xFFAA7777u))
		(*actions)++;
	fuse_id(canvas, "steps");
	fuse_sizing(canvas, FUSE_SIZING_GROW, FUSE_SIZING_FIXED);
	fuse_div_begin_scroll(canvas, 0, 0, 180, 120, &rows, &panel->scroll);
	fuse_div_scope(canvas);
	for (row = 0; row < 8; row++) {
		fuse_idi(canvas, "step", row);
		if (fuse_button_text(canvas, "Step", 0, 0, 80, 22, 0xFF555555u, 0xFFAA7777u)) {
			panel->gain = (float)row / 7;
			(*actions)++;
		}
	}
	fuse_div_end(canvas);
}

int
demo_commands_consume(FuseCanvas canvas, const FuseCmd *commands, size_t count, uint64_t *fingerprint)
{
	const FuseFieldRun *fields;
	size_t field_count, i;
	int clips = 0;

	fields = fuse_canvas_fields(canvas, &field_count);
	/* Consume the real draw stream before clear invalidates commands/fields. */
	for (i = 0; i < count; i++) {
		const FuseCmd *command = commands + i;
		*fingerprint += command->id + (uint64_t)command->type + (uint16_t)command->z;
		switch (command->type) {
		case FUSE_CMD_RECT:
			*fingerprint += command->rect.color;
			break;
		case FUSE_CMD_LINE:
			*fingerprint += command->line.color;
			break;
		case FUSE_CMD_CLIP_START:
			clips++;
			break;
		case FUSE_CMD_CLIP_END:
			if (--clips < 0)
				return 0;
			break;
		case FUSE_CMD_IMAGE:
			*fingerprint += command->image.handle;
			break;
		case FUSE_CMD_TEXT:
			if (!fields || command->text.slot >= field_count || !fields[command->text.slot].str)
				return 0;
			*fingerprint += fields[command->text.slot].color + strlen(fields[command->text.slot].str);
			break;
		case FUSE_CMD_TRIANGLE:
			*fingerprint += command->triangle.color;
			break;
		default:
			return 0;
		}
	}
	return clips == 0;
}

int
main(void)
{
	const FuseParams params = { .max_elements = 4096, .max_windows = DEMO_PANELS, .max_scopes = 16 };
	const FuseClass panel_class = {
		.color = 0xFF181818u, .direction = FUSE_DIRECTION_COLUMN, .gap = 6,
		.width_sizing = FUSE_SIZING_GROW, .height_sizing = FUSE_SIZING_FIT,
		.pad_l = 8, .pad_r = 8, .pad_t = 8, .pad_b = 8
	};
	DemoPanel panels[DEMO_PANELS] = {
		{ "canvas", "Canvas", "Canvas", 6, 0.25f, 0 },
		{ "tools", "Tools", "Tools", 5, 0.50f, 0 },
		{ "timeline", "Timeline", "Timeline", 8, 0.75f, 0 },
		{ "history", "History", "History", 7, 1.00f, 0 }
	};
	FuseCanvas canvas;
	FuseDock *dock;
	FuseDockLayout saved;
	void *canvas_backing = NULL, *dock_backing = NULL;
	size_t canvas_bytes = fuse_memory(&params), dock_bytes = fuse_dock_memory();
	size_t total_commands = 0, actions = 0, edits = 0;
	uint64_t fingerprint = 0;
	int handles[DEMO_PANELS], frame, window, result = 1;

	if (!canvas_bytes || !dock_bytes || !(canvas_backing = malloc(canvas_bytes)) ||
		!(dock_backing = malloc(dock_bytes))) {
		fprintf(stderr, "fuse demo: cannot allocate backing\n");
		goto done;
	}
	canvas = fuse_place_in_memory(canvas_backing, canvas_bytes, &params);
	dock = fuse_dock_create(dock_backing, dock_bytes, DEMO_PANELS);
	if (!canvas || !dock) {
		fprintf(stderr, "fuse demo: placement failed\n");
		goto done;
	}
	for (window = 0; window < DEMO_PANELS; window++) {
		handles[window] = fuse_dock_register(dock, panels[window].name, panels[window].title, 120, 160);
		if (handles[window] < 0) {
			fprintf(stderr, "fuse demo: registration failed\n");
			goto done;
		}
	}
	for (frame = 0; frame < DEMO_FRAMES; frame++) {
		FuseDockRect host = { 0, 0, 1024 + 64 * ((frame / 90) % 3), 720 + 48 * ((frame / 90) % 2) };
		FuseDockRect content;
		FuseDockEvent event;
		FusePointerState pointer;
		FuseCmd *commands;
		size_t command_count = 0;
		uint32_t previous;
		int consumed = 0, order, phase = frame % 90;

		fuse_canvas_clear(canvas);
		fuse_canvas_resize(canvas, host.w, host.h);
		fuse_dock_layout(dock, host);
		/* Programmatic view changes first cancel any pending pointer gesture. */
		if (phase == 0 || phase == 30 || phase == 60 || phase == 75) {
			FuseDockEvent cancel = { .cancel = 1 };
			consumed = fuse_dock_event(dock, &cancel);
		}
		/* Repeat an editing session: split, tab, float, redock, restore a saved view. */
		if (phase == 0) {
			fuse_dock_reset(dock);
			if (!fuse_dock_place(dock, handles[1], handles[0], FUSE_DOCK_RIGHT) ||
				!fuse_dock_place(dock, handles[2], handles[0], FUSE_DOCK_BOTTOM) ||
				!fuse_dock_place(dock, handles[3], handles[1], FUSE_DOCK_CENTER))
				goto dock_error;
			fuse_dock_export(dock, &saved);
		} else if (phase == 30) {
			if (!fuse_dock_place(dock, handles[3], handles[1], FUSE_DOCK_FLOAT))
				goto dock_error;
		} else if (phase == 60) {
			if (!fuse_dock_place(dock, handles[3], handles[1], FUSE_DOCK_CENTER))
				goto dock_error;
		} else if (phase == 75) {
			if (!fuse_dock_import(dock, &saved))
				goto dock_error;
		}
		if (!fuse_dock_content(dock, handles[0], &content))
			goto dock_error;
		pointer = frame % 4 == 0 ? FUSE_POINTER_NONE :
			frame % 4 == 3 ? FUSE_POINTER_RELEASED : FUSE_POINTER_PRESSED;
		event = (FuseDockEvent){ .x = content.x + 24 + frame % 120,
			.y = content.y + ((frame / 12) % 3 == 0 ? 64 : (frame / 12) % 3 == 1 ? 88 : 136),
			.press = frame % 4 == 1, .release = frame % 4 == 3 };
		consumed |= fuse_dock_event(dock, &event);
		if (phase == 15) {
			FuseDockEvent next = { .next = 1 };
			if (!fuse_dock_focus_tabs(dock, handles[1]))
				goto dock_error;
			consumed |= fuse_dock_event(dock, &next);
		}
		fuse_canvas_pointer(canvas, pointer, event.x, event.y);
		fuse_canvas_input_enabled(canvas, !consumed);
		fuse_canvas_wheel(canvas, 0, frame % 16 < 8 ? -1 : 1);
		previous = fuse_scope_enter(canvas, panels[0].name);
		fuse_focus(canvas, "value");
		if (!consumed) {
			if (frame % 4 == 0)
				fuse_canvas_text(canvas, "x");
			else if (frame % 4 == 2)
				fuse_canvas_key(canvas, FUSE_KEY_BACKSPACE);
		}
		if (fuse_textbox_edit(canvas, "value", panels[0].text, sizeof(panels[0].text), &panels[0].caret, NULL, NULL))
			edits++;
		fuse_scope_restore(canvas, previous);

		for (order = 0; (window = fuse_dock_visible(dock, order)) >= 0; order++) {
			int panel;
			for (panel = 0; panel < DEMO_PANELS && handles[panel] != window; panel++)
				;
			if (panel == DEMO_PANELS || order >= DEMO_PANELS)
				goto dock_error;
			fuse_canvas_layer(canvas, (int16_t)order);
			fuse_dock_chrome(dock, canvas, window, 0xFF181818u, 0xFF333333u,
				0xFF552222u, 0xFFFFFFFFu, 0xFF888888u, 0xFFFF4444u);
			if (!fuse_dock_begin(dock, canvas, window, &panel_class))
				goto dock_error;
			demo_panel_build(canvas, panels + panel, &actions);
			fuse_dock_end(canvas);
		}
		commands = fuse_canvas_draw(canvas, &command_count);
		if (!commands || !command_count || fuse_canvas_error(canvas) != FUSE_ERR_OK ||
			!demo_commands_consume(canvas, commands, command_count, &fingerprint)) {
			fprintf(stderr, "fuse demo: frame %d command build/consumption failed (error %d)\n",
				frame, fuse_canvas_error(canvas));
			goto done;
		}
		for (window = 0; window < DEMO_PANELS; window++) {
			DemoPanel *panel = panels + window;
			if (!isfinite(panel->gain) || panel->gain < 0 || panel->gain > 1 || !isfinite(panel->scroll) ||
				panel->caret < 0 || (size_t)panel->caret > strlen(panel->text)) {
				fprintf(stderr, "fuse demo: frame %d invalid caller widget state\n", frame);
				goto done;
			}
		}
		total_commands += command_count;
	}
	if (!actions || !edits) {
		fprintf(stderr, "fuse demo: synthetic input produced no actions or text edits\n");
		goto done;
	}
	printf("fuse demo: %d frames, %zu commands, %zu actions, %zu edits, fingerprint %llu\n",
		DEMO_FRAMES, total_commands, actions, edits, (unsigned long long)fingerprint);
	result = 0;
	goto done;

dock_error:
	fprintf(stderr, "fuse demo: frame %d docking workload failed (busy %d)\n", frame, fuse_dock_busy(dock));
done:
	free(dock_backing);
	free(canvas_backing);
	return result;
}
