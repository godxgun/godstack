/* Standalone scope/input regression: cc -std=c99 fuse_scope_test.c -lm */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "fuse.c"

static void
panel(FuseCanvas c, const char *name, float x)
{
    fuse_id(c, name);
    fuse_div_begin(c, x, 0, 100, 100, NULL);
    fuse_div_scope(c); /* Exact additional dock-begin integration. */
}

static void
field(FuseCanvas c, char *text, int *caret)
{
    fuse_id(c, "value");
    fuse_textbox(c, 0, 0, 80, 20, 0, 0, text, 16, caret, "", 12,
        1, 2, 3, 4, 1, NULL, NULL);
}

static void
draw(FuseCanvas c)
{
    size_t count;
    assert(fuse_canvas_draw(c, &count));
    assert(c->error == FUSE_ERR_OK);
}

int
main(void)
{
    size_t size = fuse_canvas_memory(256);
    void *memory = malloc(size);
    FuseCanvas c = fuse_canvas_create(memory, size);
    uint32_t a, b, indexed, anonymous;
    char left[16] = "", right[16] = "";
    int lc = 0, rc = 0;
    float slider = 0.25f, scroll = 0;

    assert(c);
    fuse_canvas_resize(c, 400, 200);
    panel(c, "left", 0);
    fuse_focus(c, "value");
    field(c, left, &lc);
    a = c->elements[c->element_count - 1].id;
    fuse_idi(c, "item", 4);
    fuse_button(c, 0, 30, 20, 20, 1, 2);
    indexed = c->elements[c->element_count - 1].id;
    fuse_button(c, 25, 30, 20, 20, 1, 2);
    anonymous = c->elements[c->element_count - 1].id;
    fuse_div_end(c);
    assert(!fuse_focused(c, "value"));
    panel(c, "right", 100);
    assert(!fuse_focused(c, "value"));
    field(c, right, &rc);
    b = c->elements[c->element_count - 1].id;
    assert(a != b);
    fuse_div_end(c);
    draw(c);

    /* Reverse emission order and move panels: identity/edit ownership survives. */
    fuse_canvas_clear(c);
    fuse_canvas_text(c, "X");
    fuse_canvas_pointer(c, FUSE_POINTER_NONE, 10, 10);
    panel(c, "right", 0);
    assert(!fuse_element_is_hovered(c, "value"));
    assert(!fuse_textbox_edit(c, "value", right, 16, &rc, NULL, NULL));
    field(c, right, &rc);
    assert(c->elements[c->element_count - 1].id == b);
    fuse_div_end(c);
    panel(c, "left", 100);
    assert(fuse_element_is_hovered(c, "value"));
    assert(fuse_focused(c, "value"));
    assert(fuse_textbox_edit(c, "value", left, 16, &lc, NULL, NULL));
    /* Ordinary nested layout preserves named identity. */
    fuse_div_begin(c, 0, 0, 90, 90, NULL);
    field(c, left, &lc);
    assert(c->elements[c->element_count - 1].id == a);
    assert(!strcmp(left, "X") && !right[0]);
    fuse_div_end(c);
    fuse_idi(c, "item", 4);
    fuse_button(c, 0, 30, 20, 20, 1, 2);
    assert(c->elements[c->element_count - 1].id == indexed);
    /* Nested scoped div has a distinct namespace and restores its parent. */
    fuse_id(c, "nested");
    fuse_div_begin(c, 0, 50, 20, 20, NULL);
    fuse_div_scope(c);
    assert(!fuse_focused(c, "value"));
    fuse_div_end(c);
    assert(fuse_focused(c, "value"));
    fuse_div_end(c);
    draw(c);

    /* Anonymous widgets stay stable with unchanged child structure. */
    fuse_canvas_clear(c);
    panel(c, "left", 50);
    field(c, left, &lc);
    fuse_idi(c, "item", 4);
    fuse_button(c, 0, 30, 20, 20, 1, 2);
    fuse_button(c, 25, 30, 20, 20, 1, 2);
    assert(c->elements[c->element_count - 1].id == anonymous);
    fuse_div_end(c);
    draw(c);

    /* Focus and edits survive a hidden frame, then disabled pointer picking. */
    fuse_canvas_clear(c);
    panel(c, "right", 0);
    field(c, right, &rc);
    fuse_div_end(c);
    draw(c);
    fuse_canvas_clear(c);
    fuse_canvas_text(c, "Y");
    fuse_canvas_input_enabled(c, 0);
    panel(c, "left", 0);
    assert(fuse_focused(c, "value"));
    field(c, left, &lc);
    assert(!strcmp(left, "XY"));
    fuse_div_end(c);
    draw(c);

    /* Legacy names remain global when no div is explicitly scoped. */
    fuse_canvas_clear(c);
    fuse_focus(c, "legacy");
    fuse_div_begin(c, 0, 0, 100, 100, NULL);
    assert(fuse_focused(c, "legacy"));
    fuse_id(c, "legacy");
    fuse_button(c, 0, 0, 20, 20, 1, 2);
    assert(c->elements[c->element_count - 1].id == fuse_internal_hash_str("legacy"));
    fuse_div_end(c);
    draw(c);

    /* Disabled input also suppresses an already captured slider's motion. */
    fuse_canvas_clear(c);
    fuse_id(c, "slider");
    fuse_slider(c, 0, 0, 100, 20, 1, 2, &slider);
    draw(c);
    fuse_canvas_clear(c);
    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 25, 10);
    fuse_id(c, "slider");
    fuse_slider(c, 0, 0, 100, 20, 1, 2, &slider);
    assert(c->capture_id && slider == 0.25f);
    draw(c);
    fuse_canvas_clear(c);
    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 75, 10);
    fuse_canvas_input_enabled(c, 0);
    fuse_id(c, "slider");
    fuse_slider(c, 0, 0, 100, 20, 1, 2, &slider);
    assert(slider == 0.25f);
    draw(c);
    fuse_canvas_clear(c);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 75, 10);
    fuse_canvas_input_enabled(c, 0);
    fuse_id(c, "slider");
    fuse_slider(c, 0, 0, 100, 20, 1, 2, &slider);
    assert(!c->capture_id && slider == 0.25f);
    draw(c);

    /* Disabled picking suppresses hover/release and wheel ownership. */
    fuse_canvas_clear(c);
    fuse_id(c, "button");
    fuse_button(c, 0, 0, 100, 20, 1, 2);
    fuse_id(c, "scroll");
    fuse_div_begin_scroll(c, 0, 30, 100, 100, NULL, &scroll);
    fuse_rect(c, 0, 0, 20, 200, 1);
    fuse_div_end(c);
    draw(c);
    fuse_canvas_clear(c);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 10, 10);
    fuse_canvas_input_enabled(c, 0);
    assert(!fuse_element_is_hovered(c, "button"));
    fuse_id(c, "button");
    assert(!fuse_button(c, 0, 0, 100, 20, 1, 2));
    fuse_canvas_pointer(c, FUSE_POINTER_NONE, 10, 50);
    fuse_canvas_wheel(c, 0, -3);
    fuse_id(c, "scroll");
    fuse_div_begin_scroll(c, 0, 30, 100, 100, NULL, &scroll);
    fuse_rect(c, 0, 0, 20, 200, 1);
    fuse_div_end(c);
    assert(!c->wheel_capture);
    draw(c);
    assert(scroll == 0);

    /* Scope misuse and duplicate names still report normal canvas errors. */
    fuse_canvas_clear(c);
    fuse_div_scope(c);
    assert(c->error == FUSE_ERR_UNBALANCED);
    fuse_canvas_clear(c);
    panel(c, "left", 0);
    fuse_id(c, "same");
    fuse_button(c, 0, 0, 10, 10, 1, 2);
    fuse_id(c, "same");
    fuse_button(c, 20, 0, 10, 10, 1, 2);
    assert(c->error == FUSE_ERR_DUPLICATE_ID);
    free(memory);
    return 0;
}
