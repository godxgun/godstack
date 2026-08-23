#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PEAK_CANVAS "#PeakCanvas"

static PeakKeyCode map_key(const char *code);
static PeakKeyMod map_mod(const EmscriptenKeyboardEvent *keyEvent);
static EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData);
static EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);
static EM_BOOL resize_callback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData);

void
peak_platform_window_open()
{
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, key_callback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, key_callback);
    emscripten_set_mousedown_callback(PEAK_CANVAS, NULL, EM_TRUE, mouse_callback);
    emscripten_set_mouseup_callback(PEAK_CANVAS, NULL, EM_TRUE, mouse_callback);
    emscripten_set_mousemove_callback(PEAK_CANVAS, NULL, EM_TRUE, mouse_callback);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, resize_callback);
}

void
peak_platform_window_close()
{
}

uint32_t*
peak_platform_window_buffer(size_t *width, size_t *height)
{
}

bool
peak_platform_epoll(PeakEvent *ev)
{
    return 0;
}
